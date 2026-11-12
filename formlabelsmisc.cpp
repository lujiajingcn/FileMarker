// FormLabels 的杂项实现：右键菜单、拖拽、外部标签回传。

#include "formlabels.h"
#include "ui_formlabels.h"

#include <QMouseEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QUrl>
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDrag>
#include <QFile>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>

#include "adsoperation.h"
#include "common.h"
#include "sqliteoperation.h"
#include "utility.h"

namespace {

/** 大小写不敏感地判断标签是否已在集合里（Windows 的流名不区分大小写） */
bool labelExistsCaseInsensitive(const QSet<QString> &set, const QString &sLabel)
{
    for (const QString &sExisting : set) {
        if (QString::compare(sExisting, sLabel, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

}   // namespace

bool FormLabels::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_labelsTree->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_ptDragStart = mouseEvent->pos();
                m_mayStartDrag = true;
                // 先把"这一刻选中的标签"记下来：紧接着 QTreeView 自己处理这个按下事件时，
                // 会把多选收窄成按下那一行，等真正开始拖拽时就只剩一个标签了。
                m_pendingDragLabels = getSelLabels();
            }
        } else if (event->type() == QEvent::MouseMove && m_mayStartDrag) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton)
                && (mouseEvent->pos() - m_ptDragStart).manhattanLength()
                       >= QApplication::startDragDistance()) {
                m_mayStartDrag = false;
                startLabelDrag();
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_mayStartDrag = false;
            m_pendingDragLabels.clear();
            m_dragEnterLabels.clear();
        } else if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            // 反过来：把文件拖到某个标签上 = 给这些文件加这个标签
            auto *dragEvent = static_cast<QDragMoveEvent *>(event);
            const QMimeData *mime = dragEvent->mimeData();
            if (mime != nullptr
                && (mime->hasUrls() || mime->hasFormat(QString::fromLatin1(MIME_FILEMARKER_FILES)))
                && !mime->hasFormat(QString::fromLatin1(MIME_FILEMARKER_LABEL))) {
                // 只要拖的是文件就接受，不要求鼠标当下正压在某个标签行上 ——
                // 否则从树的空白处进入时 Qt 会判定"不接受"，之后光标一直是禁止的样子。
                // 真正落到哪一行由 Drop 时再判断；悬停到某一行时顺手高亮它做提示。
                if (event->type() == QEvent::DragEnter)
                    m_dragEnterLabels = getSelLabels();

                // 只更新"当前行"用来做落点提示，且必须带 NoUpdate：
                // setCurrentIndex() 默认会顺手选中该行，把用户多选的标签全部顶掉 ——
                // 这就是"鼠标滑过标签时选中状态消失"的原因。
                const QModelIndex index = m_labelsTree->indexAt(dragEvent->position().toPoint());
                m_dropHoverRow = index.row();
                if (index.isValid() && index != m_labelsTree->currentIndex()) {
                    m_labelsTree->selectionModel()->setCurrentIndex(
                        index, QItemSelectionModel::NoUpdate);
                }

                dragEvent->setDropAction(Qt::CopyAction);
                dragEvent->accept();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto *dropEvent = static_cast<QDropEvent *>(event);
            const QMimeData *mime = dropEvent->mimeData();
            if (mime != nullptr
                && (mime->hasUrls() || mime->hasFormat(QString::fromLatin1(MIME_FILEMARKER_FILES)))
                && !mime->hasFormat(QString::fromLatin1(MIME_FILEMARKER_LABEL))) {
                QModelIndex index = m_labelsTree->indexAt(dropEvent->position().toPoint());
                if (!index.isValid() && m_dropHoverRow >= 0) {
                    // 落在空白处：用拖拽过程中鼠标经过的最后一行
                    index = m_labelsTree->model()->index(m_dropHoverRow, 0,
                                                         m_labelsTree->rootIndex());
                }
                if (!index.isValid()) {
                    m_dropHoverRow = -1;
                    return false;
                }
                m_dropHoverRow = -1;
                QStringList qLFiles;
                if (mime->hasFormat(QString::fromLatin1(MIME_FILEMARKER_FILES))) {
                    // 本程序自己拖的文件列表（多选时也完整）
                    const QString sPayload = QString::fromUtf8(
                        mime->data(QString::fromLatin1(MIME_FILEMARKER_FILES)));
                    for (const QString &sLine : sPayload.split(QLatin1Char('\n'))) {
                        const QString sPath = QDir::cleanPath(sLine.trimmed());
                        if (!sPath.isEmpty() && QFileInfo::exists(sPath) && !qLFiles.contains(sPath))
                            qLFiles << sPath;
                    }
                } else {
                    for (const QUrl &url : mime->urls()) {
                        if (!url.isLocalFile())
                            continue;
                        // 统一用正斜杠（QDir::cleanPath），和 QFileSystemModel::filePath() 保持一致，
                        // 否则文件页按路径清缓存会清不掉，界面上的标签列不刷新。
                        const QString sPath = QDir::cleanPath(url.toLocalFile());
                        if (QFileInfo::exists(sPath) && !qLFiles.contains(sPath))
                            qLFiles << sPath;
                    }
                }
                if (qLFiles.isEmpty())
                    return false;
                const QString sLabel = index.data(Qt::DisplayRole).toString().trimmed();
                if (sLabel.isEmpty())
                    return false;

                // 拖到的这一行本身在多选范围内 → 把选中的标签全都加上（和资源管理器一致）；
                // 否则只加这一行的标签。
                QStringList qLLabels;
                const QStringList qLSelLabels = m_dragEnterLabels;
                m_dragEnterLabels.clear();
                if (qLSelLabels.size() > 1 && qLSelLabels.contains(sLabel))
                    qLLabels = qLSelLabels;
                else
                    qLLabels << sLabel;

                dropEvent->setDropAction(Qt::CopyAction);
                dropEvent->accept();
                emit filesDroppedOnLabel(qLFiles, qLLabels);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

/** 把当前选中的标签打包成拖拽数据，交给文件列表那边处理。 */
void FormLabels::startLabelDrag()
{
    QStringList qLLabels = m_pendingDragLabels;
    if (qLLabels.isEmpty())
        qLLabels = getSelLabels();
    if (qLLabels.isEmpty()) {
        const QModelIndex index = m_labelsTree->currentIndex();
        if (index.isValid())
            qLLabels << index.data(Qt::DisplayRole).toString();
    }
    qLLabels.removeAll(QString());
    if (qLLabels.isEmpty())
        return;

    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(MIME_FILEMARKER_LABEL), qLLabels.join("\n").toUtf8());
    mime->setText(qLLabels.join(","));

    auto *drag = new QDrag(m_labelsTree);
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction);
}

/** 方案A：标签库的命令不再放到主窗口菜单里，改为本面板的右键菜单（外加 dock 上的工具条）。 */
void FormLabels::addContextMenuAction(QAction *action)
{
    if (action != nullptr && !m_extraContextActions.contains(action))
        m_extraContextActions.append(action);
}

void FormLabels::onTreeContextMenuRequested(const QPoint &pos)
{
    // 右键点在哪一行就选中哪一行，避免"菜单操作的是另一行"
    const QModelIndex index = m_labelsTree->indexAt(pos);
    if (index.isValid())
        m_labelsTree->setCurrentIndex(index);

    QMenu menu(this);
    menu.addAction(ui->actionAdd);
    menu.addAction(ui->actionAddChild);
    menu.addSeparator();
    menu.addAction(ui->actionDelete);
    menu.addSeparator();
    menu.addAction(ui->actionMoveUp);
    menu.addAction(ui->actionMoveDown);
    menu.addAction(ui->actionMoveLeft);
    menu.addAction(ui->actionMoveRight);

    if (!m_extraContextActions.isEmpty()) {
        menu.addSeparator();
        for (QAction *menuAction : m_extraContextActions)
            menu.addAction(menuAction);
    }

    // 恢复"发现新标签，是否加入标签库"的询问
    menu.addSeparator();
    QAction *actionResetPrompt = menu.addAction(tr("恢复“新标签”询问"));
    connect(actionResetPrompt, &QAction::triggered, this, []() {
        QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
        settings.setValue(QStringLiteral("labels/autoAddFromFiles"), 0);
    });

    menu.exec(m_labelsTree->viewport()->mapToGlobal(pos));
}

void FormLabels::setToolButtonDefaultAction()
{
    ui->btnAdd->setDefaultAction(ui->actionAdd);
    ui->btnAddChild->setDefaultAction(ui->actionAddChild);
    ui->btnDelete->setDefaultAction(ui->actionDelete);
    ui->btnMoveUp->setDefaultAction(ui->actionMoveUp);
    ui->btnMoveDown->setDefaultAction(ui->actionMoveDown);
    ui->btnMoveLeft->setDefaultAction(ui->actionMoveLeft);
    ui->btnMoveRight->setDefaultAction(ui->actionMoveRight);
}

void FormLabels::onTreeViewClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    // 复用 getSelLabels，避免与 getSelLabels 重复逻辑，且同样去除 selectedIndexes 多列重复。
    sendSelLabels(getSelLabels());
}

void FormLabels::onRecvLabels(const QString &sLabels)
{
    // 支持中英文逗号/分号分隔，splitLabels 已做 trim、去空、去重
    QStringList qLNewLabels;
    for (const QString &sLabel : Utility::splitLabels(sLabels))
    {
        if (!labelExistsCaseInsensitive(m_setLabels, sLabel) && !qLNewLabels.contains(sLabel))
            qLNewLabels << sLabel;
    }

    if (qLNewLabels.isEmpty())
        return;

    // 排队弹窗/入库：不要在 model->setData() 的调用栈里直接 exec()，避免重入
    QTimer::singleShot(0, this, [this, qLNewLabels]() { promptAddNewLabels(qLNewLabels); });
}

void FormLabels::promptAddNewLabels(const QStringList &qLInputLabels)
{
    if (m_promptShowing)
        return;

    // 排队期间标签可能已经被前一个弹窗加入，显示前再过滤一次
    QStringList qLNewLabels;
    for (const QString &sLabel : qLInputLabels) {
        if (!labelExistsCaseInsensitive(m_setLabels, sLabel)
                && !qLNewLabels.contains(sLabel)) {
            qLNewLabels << sLabel;
        }
    }
    if (qLNewLabels.isEmpty())
        return;

    struct PromptGuard {
        bool &b;
        explicit PromptGuard(bool &value) : b(value) { b = true; }
        ~PromptGuard() { b = false; }
    } guard(m_promptShowing);

    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
    // 0=每次询问，1=总是加入，2=从不加入
    const int nMode = settings.value(QStringLiteral("labels/autoAddFromFiles"), 0).toInt();
    if (nMode == 2)
        return;

    bool bAdd = (nMode == 1);
    if (nMode == 0)
    {
        QString sPreview = qLNewLabels.mid(0, 10).join("，");
        if (qLNewLabels.size() > 10)
            sPreview += tr("…等 %1 个").arg(qLNewLabels.size());

        QMessageBox box(this);
        box.setWindowTitle(tr("标签库"));
        box.setIcon(QMessageBox::Question);
        box.setText(tr("发现 %1 个新标签，是否加入标签库？\n\n%2").arg(qLNewLabels.size()).arg(sPreview));
        QPushButton *addButton = box.addButton(tr("加入"), QMessageBox::AcceptRole);
        QPushButton *btnSkip = box.addButton(tr("不加入"), QMessageBox::RejectRole);
        QCheckBox *remember = new QCheckBox(tr("记住我的选择"), &box);
        box.setCheckBox(remember);
        box.setDefaultButton(addButton);
        box.setEscapeButton(btnSkip);
        box.exec();

        bAdd = (box.clickedButton() == addButton);
        if (remember->isChecked())
            settings.setValue(QStringLiteral("labels/autoAddFromFiles"), bAdd ? 1 : 2);
    }

    if (!bAdd)
        return;

    // 弹窗期间标签库可能已经变化，再检查一次
    for (const QString &sLabel : qLNewLabels) {
        if (labelExistsCaseInsensitive(m_setLabels, sLabel))
            continue;
        QStandardItem *newItem = createLabelItem(sLabel);
        m_modelAllLabels->appendRow(newItem);
        registerItem(newItem);
    }
    saveLabelToXmlFile();   // #11 外部写入标签后立即持久化
}

void FormLabels::onRecvLabelsGeneratedByAI(const QString &sLabels)
{
    bool bAlreadyHaveItem = false;
    QStandardItem *itemLabelsGeneratedByAI = nullptr;
    int nRowCount = m_modelAllLabels->rowCount();
    for (int i = 0; i < nRowCount; i++)
    {
        QString sLabelName = m_modelAllLabels->item(i)->text();
        if (QString::compare(sLabelName, LABEL_NAME_AI) == 0)
        {
            itemLabelsGeneratedByAI = m_modelAllLabels->item(i);
            bAlreadyHaveItem = true;
        }
    }
    if (!bAlreadyHaveItem)
    {
        itemLabelsGeneratedByAI = createLabelItem(LABEL_NAME_AI);
        itemLabelsGeneratedByAI->setEditable(false);
        m_modelAllLabels->appendRow(itemLabelsGeneratedByAI);
        registerItem(itemLabelsGeneratedByAI);
    }

    // 支持中英文逗号/分号分隔，splitLabels 已做 trim、去空、去重；
    // AI 返回的名字还要按磁盘规则净化，否则标签库与磁盘会不一致。
    for (const QString &sLabel : Utility::splitLabels(sLabels))
    {
        const QString sSanitized = ADSOperation::sanitizeTagName(sLabel);
        if (sSanitized.isEmpty())
            continue;

        bool bExists = false;
        for (const QString &sExisting : m_setLabels) {
            if (QString::compare(sExisting, sSanitized, Qt::CaseInsensitive) == 0) {
                bExists = true;
                break;
            }
        }
        if (bExists)
            continue;

        QStandardItem *newItem = createLabelItem(sSanitized);
        itemLabelsGeneratedByAI->appendRow(newItem);
        registerItem(newItem);
    }
    saveLabelToXmlFile();   // #11 AI 生成标签后立即持久化
}
