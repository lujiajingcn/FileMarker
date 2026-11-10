#include "formlabels.h"
#include "ui_formlabels.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDrag>
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
    if (watched == m_tvAllLabels->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *pMouseEvent = static_cast<QMouseEvent *>(event);
            if (pMouseEvent->button() == Qt::LeftButton) {
                m_ptDragStart = pMouseEvent->pos();
                m_bMayStartDrag = true;
            }
        } else if (event->type() == QEvent::MouseMove && m_bMayStartDrag) {
            auto *pMouseEvent = static_cast<QMouseEvent *>(event);
            if ((pMouseEvent->buttons() & Qt::LeftButton)
                && (pMouseEvent->pos() - m_ptDragStart).manhattanLength()
                       >= QApplication::startDragDistance()) {
                m_bMayStartDrag = false;
                startLabelDrag();
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_bMayStartDrag = false;
        }
    }
    return QWidget::eventFilter(watched, event);
}

/** 把当前选中的标签打包成拖拽数据，交给文件列表那边处理。 */
void FormLabels::startLabelDrag()
{
    QStringList qLLabels = getSelLabels();
    if (qLLabels.isEmpty()) {
        const QModelIndex index = m_tvAllLabels->currentIndex();
        if (index.isValid())
            qLLabels << index.data(Qt::DisplayRole).toString();
    }
    qLLabels.removeAll(QString());
    if (qLLabels.isEmpty())
        return;

    auto *pMime = new QMimeData;
    pMime->setData(QString::fromLatin1(MIME_FILEMARKER_LABEL), qLLabels.join("\n").toUtf8());
    pMime->setText(qLLabels.join(","));

    auto *pDrag = new QDrag(m_tvAllLabels);
    pDrag->setMimeData(pMime);
    pDrag->exec(Qt::CopyAction);
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
    const QModelIndex index = m_tvAllLabels->indexAt(pos);
    if (index.isValid())
        m_tvAllLabels->setCurrentIndex(index);

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
        for (QAction *pAction : m_extraContextActions)
            menu.addAction(pAction);
    }

    // 恢复"发现新标签，是否加入标签库"的询问
    menu.addSeparator();
    QAction *actionResetPrompt = menu.addAction(tr("恢复“新标签”询问"));
    connect(actionResetPrompt, &QAction::triggered, this, []() {
        QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        settings.setIniCodec("UTF-8");
#endif
        settings.setValue(QStringLiteral("labels/autoAddFromFiles"), 0);
    });

    menu.exec(m_tvAllLabels->viewport()->mapToGlobal(pos));
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

void FormLabels::onRecvLabels(QString sLabels)
{
    // 支持中英文逗号/分号分隔，splitLabels 已做 trim、去空、去重
    QStringList qLNewLabels;
    foreach (const QString &sLabel, Utility::splitLabels(sLabels))
    {
        if(!labelExistsCaseInsensitive(m_setLabels, sLabel) && !qLNewLabels.contains(sLabel))
            qLNewLabels << sLabel;
    }

    if (qLNewLabels.isEmpty())
        return;

    // 排队弹窗/入库：不要在 model->setData() 的调用栈里直接 exec()，避免重入
    QTimer::singleShot(0, this, [this, qLNewLabels]() { promptAddNewLabels(qLNewLabels); });
}

void FormLabels::promptAddNewLabels(const QStringList &qLInputLabels)
{
    if (m_bPromptShowing)
        return;

    // 排队期间标签可能已经被前一个弹窗加入，显示前再过滤一次
    QStringList qLNewLabels;
    foreach (const QString &sLabel, qLInputLabels) {
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
    } guard(m_bPromptShowing);

    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
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
        QPushButton *pBtnAdd = box.addButton(tr("加入"), QMessageBox::AcceptRole);
        QPushButton *pBtnSkip = box.addButton(tr("不加入"), QMessageBox::RejectRole);
        QCheckBox *pRemember = new QCheckBox(tr("记住我的选择"), &box);
        box.setCheckBox(pRemember);
        box.setDefaultButton(pBtnAdd);
        box.setEscapeButton(pBtnSkip);
        box.exec();

        bAdd = (box.clickedButton() == pBtnAdd);
        if (pRemember->isChecked())
            settings.setValue(QStringLiteral("labels/autoAddFromFiles"), bAdd ? 1 : 2);
    }

    if (!bAdd)
        return;

    // 弹窗期间标签库可能已经变化，再检查一次
    foreach (const QString &sLabel, qLNewLabels)
    {
        if (labelExistsCaseInsensitive(m_setLabels, sLabel))
            continue;
        QStandardItem *newItem = createLabelItem(sLabel);
        m_modelAllLabels->appendRow(newItem);
        registerItem(newItem);
    }
    saveLabelToXmlFile();   // #11 外部写入标签后立即持久化
}

void FormLabels::onRecvLabelsGeneratedByAI(QString sLabels)
{
    bool bAlreadyHaveItem = false;
    QStandardItem *itemLabelsGeneratedByAI = nullptr;
    int nRowCount = m_modelAllLabels->rowCount();
    for(int i = 0; i < nRowCount; i++)
    {
        QString sLabelName = m_modelAllLabels->item(i)->text();
        if(QString::compare(sLabelName, LABEL_NAME_AI) == 0)
        {
            itemLabelsGeneratedByAI = m_modelAllLabels->item(i);
            bAlreadyHaveItem = true;
        }
    }
    if(!bAlreadyHaveItem)
    {
        itemLabelsGeneratedByAI = createLabelItem(LABEL_NAME_AI);
        itemLabelsGeneratedByAI->setEditable(false);
        m_modelAllLabels->appendRow(itemLabelsGeneratedByAI);
        registerItem(itemLabelsGeneratedByAI);
    }

    // 支持中英文逗号/分号分隔，splitLabels 已做 trim、去空、去重；
    // AI 返回的名字还要按磁盘规则净化，否则标签库与磁盘会不一致。
    foreach (const QString &sLabel, Utility::splitLabels(sLabels))
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
