#include "formlabels.h"
#include "ui_formlabels.h"

#include <QToolButton>
#include <QSpacerItem>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QDebug>
#include <QAction>
#include <QMenu>
#include <QCheckBox>
#include <QSettings>
#include <QTimer>
#include <functional>
#include <algorithm>
#include "adsoperation.h"
#include "sqliteoperation.h"

#include "common.h"
#include "utility.h"

namespace {
bool labelExistsCaseInsensitive(const QSet<QString> &set, const QString &sLabel)
{
    for (const QString &sExisting : set) {
        if (QString::compare(sExisting, sLabel, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}
}

FormLabels::FormLabels(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormLabels)
{
    ui->setupUi(this);

    m_bPromptShowing = false;
    m_tvAllLabels = ui->treeView;
    m_modelAllLabels = new QStandardItemModel(this);
    m_tvAllLabels->setModel(m_modelAllLabels);

    // 方案A：标签库的命令收在本面板里 —— dock 工具条 + 右键菜单
    m_tvAllLabels->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tvAllLabels, &QTreeView::customContextMenuRequested,
            this, &FormLabels::onTreeContextMenuRequested);

    setToolButtonDefaultAction();

    // 显式连接标签管理动作与标签树点击（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(ui->actionAdd, &QAction::triggered, this, &FormLabels::onActionAddTriggered);
    connect(ui->actionAddChild, &QAction::triggered, this, &FormLabels::onActionAddChildTriggered);
    connect(ui->actionDelete, &QAction::triggered, this, &FormLabels::onActionDeleteTriggered);
    connect(ui->actionMoveUp, &QAction::triggered, this, &FormLabels::onActionMoveUpTriggered);
    connect(ui->actionMoveDown, &QAction::triggered, this, &FormLabels::onActionMoveDownTriggered);
    connect(ui->actionMoveLeft, &QAction::triggered, this, &FormLabels::onActionMoveLeftTriggered);
    connect(ui->actionMoveRight, &QAction::triggered, this, &FormLabels::onActionMoveRightTriggered);
    connect(m_tvAllLabels, &QTreeView::clicked, this, &FormLabels::onTreeViewClicked);

    initLabels();

    connect(m_modelAllLabels, &QStandardItemModel::itemChanged, this, &FormLabels::onItemChanged);
}

FormLabels::~FormLabels()
{
    saveLabelToXmlFile();
    delete ui;
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

QStringList FormLabels::allLabels() const
{
    QStringList qLResult;
    std::function<void(QStandardItem*)> collect = [&](QStandardItem *parent) {
        if (parent == nullptr)
            return;
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem *item = parent->child(i);
            if (item == nullptr)
                continue;
            // AI 分组只是界面节点，不参与“未使用标签”清理
            if (QString::compare(item->text(), LABEL_NAME_AI) != 0)
                qLResult << item->text();
            collect(item);
        }
    };
    collect(m_modelAllLabels->invisibleRootItem());
    return qLResult;
}

bool FormLabels::removeLabels(const QStringList &qLLabels)
{
    if (qLLabels.isEmpty())
        return false;

    std::function<bool(QStandardItem*)> removeFrom = [&](QStandardItem *parent) {
        bool bRemoved = false;
        for (int i = parent->rowCount() - 1; i >= 0; --i) {
            QStandardItem *item = parent->child(i);
            if (item == nullptr)
                continue;

            bool bMatch = false;
            for (const QString &sLabel : qLLabels) {
                if (QString::compare(item->text(), sLabel, Qt::CaseInsensitive) == 0) {
                    bMatch = true;
                    break;
                }
            }
            if (bMatch) {
                parent->removeRow(i);
                bRemoved = true;
            } else if (removeFrom(item)) {
                bRemoved = true;
            }
        }
        return bRemoved;
    };

    const bool bRemoved = removeFrom(m_modelAllLabels->invisibleRootItem());
    if (bRemoved) {
        resyncSets();
        saveLabelToXmlFile();
    }
    return bRemoved;
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

void FormLabels::initLabels()
{
    XmlOperation xmlOperation;
    QString sXmlFile = g_sAppDir + "/" + LABEL_FILE;
    QList<LabelInfo> lstLabelInfo = xmlOperation.readLabelFromXmlFile(sXmlFile.toStdString().c_str());
    for(QList<LabelInfo>::const_iterator cItr = lstLabelInfo.begin(); cItr != lstLabelInfo.end(); cItr++)
    {
        QString sName = (*cItr).sName;
        // 保留从文件读到的 UUID（为空则新生成），避免保存时 UUID 被清空。
        QString sUuid = (*cItr).sUuid.isEmpty() ? Utility::createUuid() : (*cItr).sUuid;
        QStandardItem *item = createLabelItem(sName, sUuid);
        if((*cItr).lstChild.count() > 0)
        {
            traverseWriteLabel(item, (*cItr).lstChild);
        }
        m_modelAllLabels->appendRow(item);
        registerItem(item);
    }
}

QStringList FormLabels::getSelLabels()
{
    QStringList qLLabels;
    QItemSelectionModel *selectionModel = m_tvAllLabels->selectionModel();
    // selectedIndexes() 会为同一选中行的每一列都返回一个索引，若不只取第 0 列，
    // 会导致同一标签被重复返回多次（搜索时会重复匹配）。
    // 这里按选中行的顺序逐个追加并去重，保证返回顺序与树上看到的顺序一致
    // （原先用 QSet::values() 会打乱顺序，导致批量添加标签时顺序随机）。
    QModelIndexList selectedIndexes = selectionModel->selectedRows(0);
    // Qt 不保证 selectedRows 的顺序；按界面可见位置排序，保证批量添加顺序稳定
    std::sort(selectedIndexes.begin(), selectedIndexes.end(),
              [this](const QModelIndex &a, const QModelIndex &b) {
                  return m_tvAllLabels->visualRect(a).top() < m_tvAllLabels->visualRect(b).top();
              });
    foreach (const QModelIndex &index, selectedIndexes)
    {
        QStandardItem *item = m_modelAllLabels->itemFromIndex(index);
        if (item == nullptr)
            continue;

        const QString sLabel = item->text();
        if (!qLLabels.contains(sLabel))
            qLLabels << sLabel;
    }
    return qLLabels;
}

void FormLabels::addItem(QStandardItem *parentItem)
{
    QStandardItem *newItem = createLabelItem(NEW_LABEL_NAME);
    if(nullptr == parentItem)
    {
        m_modelAllLabels->appendRow(newItem);
    }
    else
    {
        parentItem->appendRow(newItem);
    }
    registerItem(newItem);
    m_tvAllLabels->setCurrentIndex(newItem->index());
    // 新建的节点直接进入改名状态：默认名是"新标签"，让用户马上就改，
    // 而不是点完"新建标签"看到多了一行"新标签"还要再双击一次。
    m_bNamingNewLabel = true;          // 这次改名只是给新标签起名，不要弹同步询问
    m_tvAllLabels->edit(newItem->index());
    saveLabelToXmlFile();   // #11 新增标签后立即持久化
}

// 把选中的标签展开成"组"（组 = 标签 + 全部子标签），供搜索使用。
QList<QSet<QString>> FormLabels::getSelLabelGroups() const
{
    QList<QSet<QString>> qLGroups;
    QItemSelectionModel *selectionModel = m_tvAllLabels->selectionModel();
    if (selectionModel == nullptr)
        return qLGroups;

    QModelIndexList selectedIndexes = selectionModel->selectedRows(0);
    std::sort(selectedIndexes.begin(), selectedIndexes.end(),
              [this](const QModelIndex &a, const QModelIndex &b) {
                  return m_tvAllLabels->visualRect(a).top() < m_tvAllLabels->visualRect(b).top();
              });

    std::function<void(QStandardItem *, QSet<QString> &)> collect =
        [&collect](QStandardItem *pItem, QSet<QString> &setNames) {
            if (pItem == nullptr)
                return;
            setNames.insert(pItem->text());
            for (int i = 0; i < pItem->rowCount(); ++i)
                collect(pItem->child(i), setNames);
        };

    foreach (const QModelIndex &index, selectedIndexes) {
        QStandardItem *pItem = m_modelAllLabels->itemFromIndex(index);
        if (pItem == nullptr)
            continue;
        QSet<QString> setNames;
        collect(pItem, setNames);
        if (!setNames.isEmpty())
            qLGroups << setNames;
    }
    return qLGroups;
}

void FormLabels::onActionAddTriggered()
{
    QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if(!curIdx.isValid())
    {
        addItem();
    }
    else
    {
        QStandardItem *parentItem = m_modelAllLabels->itemFromIndex(curIdx)->parent();
        if(parentItem == nullptr)
        {
            addItem();
        }
        else
        {
            addItem(parentItem);
        }
    }
}

void FormLabels::onActionAddChildTriggered()
{
    QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if(!curIdx.isValid())
    {
        QMessageBox::information(this, "提示", "未选中父节点！");
        return;
    }
    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    addItem(curItem);
}

void FormLabels::onActionDeleteTriggered()
{
    QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if(!curIdx.isValid())
    {
        return;
    }
    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    if (curItem == nullptr)
        return;

    const QString sLabel = curItem->text();
    const int nCurRow = curIdx.row();
    QStandardItem *parentItem = curItem->parent();

    // 删除只影响标签库，文件上的标签不会自动消失 —— 先问清楚，并给出"一并移除"的选项。
    QStringList qLFiles;
    if (m_sqlOperation != nullptr)
        qLFiles = m_sqlOperation->filesUsingLabel(sLabel);

    QMessageBox box(this);
    box.setWindowTitle(tr("从标签库删除"));
    box.setIcon(QMessageBox::Warning);
    box.setText(tr("要从【标签库】删除标签【%1】吗？").arg(sLabel));
    if (qLFiles.isEmpty()) {
        box.setInformativeText(tr("缓存里没有记录到正在使用它的文件（只统计执行过【遍历文件】的目录）。\n"
                                  "文件上的标签不会被删除。"));
    } else {
        box.setInformativeText(
            tr("缓存中有 %1 个文件正在使用它（只统计遍历过的目录）。\n\n"
               "「只从标签库删除」：这些文件上的标签会保留，之后显示为“库外标签”；\n"
               "「一并从文件移除」：会把这些文件上的该标签也删掉。").arg(qLFiles.size()));
    }
    box.addButton(tr("只从标签库删除"), QMessageBox::AcceptRole);
    QPushButton *pAlsoFiles = qLFiles.isEmpty() ? nullptr
                                                : box.addButton(tr("一并从文件移除"), QMessageBox::ActionRole);
    QPushButton *pCancel = box.addButton(tr("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(pCancel);
    box.exec();

    if (box.clickedButton() == pCancel || box.clickedButton() == nullptr)
        return;

    if (pAlsoFiles != nullptr && box.clickedButton() == pAlsoFiles) {
        int nFailed = 0;
        const int nOk = syncTagToFiles(false, sLabel, QString(), nFailed);
        if (nFailed > 0) {
            QMessageBox::warning(this, tr("从标签库删除"),
                                 tr("已从 %1 个文件移除，%2 个文件失败（可能被占用或只读），"
                                    "失败的仍保留该标签。").arg(nOk).arg(nFailed));
        }
    }

    if(parentItem == nullptr)
    {
        m_modelAllLabels->removeRow(nCurRow);
    }
    else
    {
        parentItem->removeRow(nCurRow);
        m_tvAllLabels->setCurrentIndex(parentItem->index());
    }
    // 删除可能包含整棵子树，删除“之后”按当前模型重新同步去重集合，
    // 必须在 removeRow 之后调用，否则 m_setLabels 会残留被删标签名，
    // 导致之后同名的外部/AI 标签被 m_setLabels.contains() 误判为已存在而静默丢弃。
    resyncSets();
    saveLabelToXmlFile();   // #11 删除后立即持久化
}

// 【2026-11-09 D1 修复】原来四个移动函数用 takeItem()/takeChild() 摘节点，
// 这两个 API 只把单元格置空、不减少行数（真正删行的是 takeRow()），于是留下 null 行；
// 函数末尾的 saveLabelToXmlFile() 遍历到 null 就解引用崩溃（实测 0xC0000005）。
// 统一改为 takeRow()/insertRow()：整行摘走、整行插回，模型里不会出现 null 行。
// 另外每次结构性变更后 resyncSets()，避免 QPersistentModelIndex 失效导致改名时清空名字。
void FormLabels::onActionMoveUpTriggered()
{
    const QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if (!curIdx.isValid() || curIdx.row() <= 0)
        return;

    const int nCurRow = curIdx.row();
    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    if (curItem == nullptr)
        return;

    QStandardItem *parentItem = curItem->parent();
    QList<QStandardItem *> rowCur;
    QList<QStandardItem *> rowUp;
    if (parentItem == nullptr) {
        rowCur = m_modelAllLabels->takeRow(nCurRow);
        rowUp = m_modelAllLabels->takeRow(nCurRow - 1);
        if (rowCur.isEmpty())
            return;
        // 先插被移动项，再插原来的上一项，否则顺序不变（等于没动）
        m_modelAllLabels->insertRow(nCurRow - 1, rowCur);
        m_modelAllLabels->insertRow(nCurRow, rowUp);
    } else {
        rowCur = parentItem->takeRow(nCurRow);
        rowUp = parentItem->takeRow(nCurRow - 1);
        if (rowCur.isEmpty())
            return;
        parentItem->insertRow(nCurRow - 1, rowCur);
        parentItem->insertRow(nCurRow, rowUp);
    }

    QStandardItem *movedItem = rowCur.value(0);
    if (movedItem != nullptr)
        m_tvAllLabels->setCurrentIndex(movedItem->index());
    resyncSets();
    saveLabelToXmlFile();   // #11 移动后立即持久化
}

void FormLabels::onActionMoveDownTriggered()
{
    const QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if (!curIdx.isValid())
        return;

    const int nCurRow = curIdx.row();
    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    if (curItem == nullptr)
        return;

    QStandardItem *parentItem = curItem->parent();
    QList<QStandardItem *> rowCur;
    QList<QStandardItem *> rowDown;
    if (parentItem == nullptr) {
        if (nCurRow + 1 >= m_modelAllLabels->rowCount())
            return;
        rowCur = m_modelAllLabels->takeRow(nCurRow);
        rowDown = m_modelAllLabels->takeRow(nCurRow);
        if (rowCur.isEmpty())
            return;
        m_modelAllLabels->insertRow(nCurRow, rowDown);
        m_modelAllLabels->insertRow(nCurRow + 1, rowCur);
    } else {
        if (nCurRow + 1 >= parentItem->rowCount())
            return;
        rowCur = parentItem->takeRow(nCurRow);
        rowDown = parentItem->takeRow(nCurRow);
        if (rowCur.isEmpty())
            return;
        parentItem->insertRow(nCurRow, rowDown);
        parentItem->insertRow(nCurRow + 1, rowCur);
    }

    QStandardItem *movedItem = rowCur.value(0);
    if (movedItem != nullptr)
        m_tvAllLabels->setCurrentIndex(movedItem->index());
    resyncSets();
    saveLabelToXmlFile();   // #11 移动后立即持久化
}

void FormLabels::onActionMoveLeftTriggered()
{
    const QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if (!curIdx.isValid())
        return;

    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    if (curItem == nullptr)
        return;

    QStandardItem *parentItem = curItem->parent();
    if (parentItem == nullptr)
        return;   // 顶层节点没有"左移"

    QStandardItem *pParentItem = parentItem->parent();
    const int nInsertRow = parentItem->index().row() + 1;
    const QList<QStandardItem *> rowCur = parentItem->takeRow(curIdx.row());
    if (rowCur.isEmpty())
        return;

    if (pParentItem == nullptr)
        m_modelAllLabels->insertRow(nInsertRow, rowCur);
    else
        pParentItem->insertRow(nInsertRow, rowCur);

    QStandardItem *movedItem = rowCur.value(0);
    if (movedItem != nullptr)
        m_tvAllLabels->setCurrentIndex(movedItem->index());
    resyncSets();
    saveLabelToXmlFile();   // #11 移动后立即持久化
}

// 作为其上方的节点的子节点
void FormLabels::onActionMoveRightTriggered()
{
    const QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if (!curIdx.isValid() || curIdx.row() <= 0)
        return;

    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    if (curItem == nullptr)
        return;

    QStandardItem *parentItem = curItem->parent();
    QStandardItem *upItem = (parentItem == nullptr)
                                ? m_modelAllLabels->item(curIdx.row() - 1)
                                : parentItem->child(curIdx.row() - 1);
    if (upItem == nullptr)
        return;

    const QList<QStandardItem *> rowCur = (parentItem == nullptr)
                                             ? m_modelAllLabels->takeRow(curIdx.row())
                                             : parentItem->takeRow(curIdx.row());
    if (rowCur.isEmpty())
        return;
    upItem->appendRow(rowCur);

    QStandardItem *movedItem = rowCur.value(0);
    if (movedItem != nullptr)
        m_tvAllLabels->setCurrentIndex(movedItem->index());
    resyncSets();
    saveLabelToXmlFile();   // #11 移动后立即持久化
}

// 节点文本编辑结束时触发：处理去重、回退旧值，并保持 m_setLabels / m_committedText 同步。
void FormLabels::onItemChanged(QStandardItem *item)
{
    QString sLabel = item->text().trimmed();
    QPersistentModelIndex pidx(item->index());

    // 空名称不允许，回退到上次提交的值。
    if (sLabel.isEmpty()) {
        item->setText(m_committedText.value(pidx));
        return;
    }

    // 标签库里的名字必须和磁盘流名规则一致，否则库里是 "a:b"、磁盘是 "a_b"，
    // 后续按库中标签搜索会搜不到。
    const QString sSanitized = ADSOperation::sanitizeTagName(sLabel);
    if (sSanitized.isEmpty()) {
        item->setText(m_committedText.value(pidx));
        return;
    }
    if (sSanitized != item->text()) {
        // 触发一次递归 itemChanged，在那次调用里统一做重复检查/提交。
        item->setText(sSanitized);
        return;
    }
    sLabel = sSanitized;

    QString old = m_committedText.value(pidx);
    if (sLabel == old)
        return; // 实际无变化

    // 若整棵树中该名称已出现多次，说明产生了重复，回退原名。
    if (countLabelOccurrences(sLabel) > 1) {
        QMessageBox::information(this, "提示", QString("标签【%1】已经存在，已恢复原名").arg(sLabel));
        item->setText(old);
        return;
    }

    // 更新去重集合与“已提交文本”，使重命名后旧键被移除、新键被加入。
    if (!old.isEmpty())
        m_setLabels.remove(old);
    m_setLabels.insert(sLabel);
    m_committedText[pidx] = sLabel;

    saveLabelToXmlFile();   // #11 重命名后立即持久化

    // 改名只改了标签库，文件上的旧标签不会自动跟着变。
    // 这里排队弹一次询问，让用户明确选择"只改库"还是"连文件一起改"（可以撤销）。
    if (m_bNamingNewLabel) {
        // 新建标签时的"起名"：只是给新节点取名字，不需要问要不要同步到文件
        m_bNamingNewLabel = false;
    } else if (!old.isEmpty() && !m_bSuppressSyncPrompt && m_sqlOperation != nullptr) {
        QTimer::singleShot(0, this, [this, old, sLabel]() { promptSyncRenameToFiles(old, sLabel); });
    }
}

/** 改名后询问是否同步到文件；可以撤销改名。 */
void FormLabels::promptSyncRenameToFiles(const QString &sOldTag, const QString &sNewTag)
{
    if (m_sqlOperation == nullptr || sOldTag.isEmpty())
        return;

    const QStringList qLFiles = m_sqlOperation->filesUsingLabel(sOldTag);

    QMessageBox box(this);
    box.setWindowTitle(tr("重命名标签"));
    box.setIcon(QMessageBox::Question);
    box.setText(tr("标签【%1】已改名为【%2】。").arg(sOldTag, sNewTag));
    if (qLFiles.isEmpty()) {
        box.setInformativeText(
            tr("注意：这次只改了【标签库】，文件上的【%1】不会被改名。\n\n"
               "缓存里没有记录到正在使用它的文件（只统计执行过【遍历文件】的目录）。").arg(sOldTag));
    } else {
        box.setInformativeText(
            tr("注意：现在只改了【标签库】，文件上的【%1】还在（缓存中记录到 %2 个文件在用，只统计遍历过的目录）。\n\n"
               "要不要把这些文件上的【%1】也改成【%3】？").arg(sOldTag).arg(qLFiles.size()).arg(sNewTag));
    }

    QPushButton *pLibOnly = box.addButton(tr("只改标签库"), QMessageBox::AcceptRole);
    QPushButton *pSync = qLFiles.isEmpty() ? nullptr : box.addButton(tr("同时改文件上的标签"), QMessageBox::ActionRole);
    QPushButton *pUndo = box.addButton(tr("撤销改名"), QMessageBox::RejectRole);
    box.setDefaultButton(pLibOnly);
    box.exec();

    if (box.clickedButton() == pUndo) {
        // 程序化改回旧名：期间不要再弹同步询问。
        m_bSuppressSyncPrompt = true;
        const QList<QStandardItem *> items = m_modelAllLabels->findItems(sNewTag, Qt::MatchExactly | Qt::MatchRecursive);
        for (QStandardItem *pItem : items) {
            if (pItem != nullptr && pItem->text() == sNewTag) {
                pItem->setText(sOldTag);
                break;
            }
        }
        m_bSuppressSyncPrompt = false;
        return;
    }

    if (pSync != nullptr && box.clickedButton() == pSync) {
        int nFailed = 0;
        const int nOk = syncTagToFiles(true, sOldTag, sNewTag, nFailed);
        QMessageBox::information(this, tr("重命名标签"),
                                 nFailed == 0
                                     ? tr("已把 %1 个文件上的【%2】改名为【%3】。").arg(nOk).arg(sOldTag, sNewTag)
                                     : tr("已处理 %1 个文件，其中 %2 个失败（可能被占用或只读）。\n"
                                          "失败的仍保留旧标签，可稍后重试。").arg(nOk).arg(nFailed));
    }
}

/** 把改名/删除应用到文件；返回成功数，失败数累加到 nFailed。 */
int FormLabels::syncTagToFiles(bool bRename, const QString &sOldTag, const QString &sNewTag, int &nFailed)
{
    nFailed = 0;
    if (m_sqlOperation == nullptr)
        return 0;

    const QStringList qLFiles = m_sqlOperation->filesUsingLabel(sOldTag);
    int nOk = 0;
    foreach (const QString &sFilePath, qLFiles) {
        const bool bOk = bRename ? ADSOperation::renameTag(sFilePath, sOldTag, sNewTag)
                                 : ADSOperation::removeTag(sFilePath, sOldTag);
        if (bOk) {
            ++nOk;
            m_sqlOperation->invalidateCacheForFile(sFilePath);
        } else {
            ++nFailed;
        }
    }
    return nOk;
}


QStandardItem* FormLabels::createLabelItem(const QString &name, const QString &uuid)
{
    QStandardItem *item = new QStandardItem(name);
    // 每个标签节点携带稳定 UUID，保存时回写，避免退出保存后标识全部丢失。
    item->setData(uuid.isEmpty() ? Utility::createUuid() : uuid, LabelUuidRole);
    return item;
}

void FormLabels::registerItem(QStandardItem *item)
{
    if (item == nullptr)
        return;
    m_setLabels.insert(item->text());
    m_committedText[QPersistentModelIndex(item->index())] = item->text();
    // 递归登记子树
    for (int i = 0; i < item->rowCount(); ++i)
        registerItem(item->child(i));
}

int FormLabels::countLabelOccurrences(const QString &name) const
{
    int n = 0;
    std::function<void(QStandardItem*)> walk = [&](QStandardItem *it) {
        if (it == nullptr)
            return;
        for (int i = 0; i < it->rowCount(); ++i) {
            QStandardItem *child = it->child(i);
            if (child == nullptr)
                continue;
            if (QString::compare(child->text(), name, Qt::CaseInsensitive) == 0)
                ++n;
            walk(child);
        }
    };
    walk(m_modelAllLabels->invisibleRootItem());
    return n;
}

// 依据当前模型重建去重集合与“已提交文本”映射（用于删除等结构性变更后保持一致）。
void FormLabels::resyncSets()
{
    m_setLabels.clear();
    m_committedText.clear();
    for (int i = 0; i < m_modelAllLabels->rowCount(); ++i)
        registerItem(m_modelAllLabels->item(i));
}

void FormLabels::saveLabelToXmlFile()
{
    XmlOperation xOperation;
    QList<LabelInfo> lstLabelInfo;
    int nRowCount = m_modelAllLabels->rowCount();
    for (int i = 0; i < nRowCount; i++)
    {
        LabelInfo labelInfo;
        QStandardItem *item = m_modelAllLabels->item(i);
        if (item == nullptr)   // 模型里不该再出现 null 行，真出现也不能崩
            continue;
        labelInfo.sUuid = item->data(LabelUuidRole).toString();
        if (labelInfo.sUuid.isEmpty())
            labelInfo.sUuid = Utility::createUuid();
        traverseReadLabel(item, labelInfo.lstChild);
        labelInfo.sName = item->text();
        lstLabelInfo.push_back(labelInfo);
    }
    QString sXmlFile = g_sAppDir + "/" + LABEL_FILE;
    xOperation.saveXml(sXmlFile.toStdString().c_str(), lstLabelInfo);
}

void FormLabels::traverseWriteLabel(QStandardItem *item, const QList<LabelInfo> &lstLabelInfo)
{
    for (QList<LabelInfo>::const_iterator cItr = lstLabelInfo.begin(); cItr != lstLabelInfo.end(); cItr++)
    {
        // 写子节点时同样保留 UUID（为空则新生成）。
        QString sUuid = (*cItr).sUuid.isEmpty() ? Utility::createUuid() : (*cItr).sUuid;
        QStandardItem *itemChild = createLabelItem((*cItr).sName, sUuid);
        if((*cItr).lstChild.count() > 0)
        {
            traverseWriteLabel(itemChild, (*cItr).lstChild);
        }
        item->appendRow(itemChild);
    }
}

void FormLabels::traverseReadLabel(QStandardItem *item, QList<LabelInfo> &lstLabelInfo)
{
    if (item == nullptr)
        return;
    int nRowCount = item->rowCount();
    for(int i = 0; i < nRowCount; i++)
    {
        QStandardItem *itemChild = item->child(i);
        if (itemChild == nullptr)   // 跳过 null 子项
            continue;
        LabelInfo labelInfo;
        labelInfo.sUuid = itemChild->data(LabelUuidRole).toString();
        if (labelInfo.sUuid.isEmpty())
            labelInfo.sUuid = Utility::createUuid();
        labelInfo.sName = itemChild->text();
        traverseReadLabel(itemChild, labelInfo.lstChild);
        lstLabelInfo.push_back(labelInfo);
    }
}

void FormLabels::updateAllLabelList(QVector<QString> vtAllLabels)
{
    m_modelAllLabels->removeRows(0, m_modelAllLabels->rowCount());
    m_setLabels.clear();
    m_committedText.clear();
    for(QVector<QString>::iterator it = vtAllLabels.begin(); it != vtAllLabels.end(); it++)
    {
        QStandardItem *item = createLabelItem(*it);
        m_modelAllLabels->appendRow(item);
        registerItem(item);
    }
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
