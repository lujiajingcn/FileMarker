// FormLabels 的树操作实现：新建/删除/移动/改名，以及改名后同步到文件。

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

void FormLabels::onActionAddTriggered()
{
    QModelIndex curIdx = m_labelsTree->currentIndex();
    if (!curIdx.isValid())
    {
        addItem();
    }
    else
    {
        QStandardItem *parentItem = m_modelAllLabels->itemFromIndex(curIdx)->parent();
        if (parentItem == nullptr)
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
    QModelIndex curIdx = m_labelsTree->currentIndex();
    if (!curIdx.isValid())
    {
        QMessageBox::information(this, "提示", "未选中父节点！");
        return;
    }
    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    addItem(curItem);
}

void FormLabels::onActionDeleteTriggered()
{
    QModelIndex curIdx = m_labelsTree->currentIndex();
    if (!curIdx.isValid())
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
    QPushButton *alsoFiles = qLFiles.isEmpty() ? nullptr
                                                : box.addButton(tr("一并从文件移除"), QMessageBox::ActionRole);
    QPushButton *cancel = box.addButton(tr("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(cancel);
    box.exec();

    if (box.clickedButton() == cancel || box.clickedButton() == nullptr)
        return;

    if (alsoFiles != nullptr && box.clickedButton() == alsoFiles) {
        int nFailed = 0;
        const int nOk = syncTagToFiles(false, sLabel, QString(), nFailed);
        if (nFailed > 0) {
            QMessageBox::warning(this, tr("从标签库删除"),
                                 tr("已从 %1 个文件移除，%2 个文件失败（可能被占用或只读），"
                                    "失败的仍保留该标签。").arg(nOk).arg(nFailed));
        }
    }

    if (parentItem == nullptr)
    {
        m_modelAllLabels->removeRow(nCurRow);
    }
    else
    {
        parentItem->removeRow(nCurRow);
        m_labelsTree->setCurrentIndex(parentItem->index());
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
    const QModelIndex curIdx = m_labelsTree->currentIndex();
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
        m_labelsTree->setCurrentIndex(movedItem->index());
    resyncSets();
    saveLabelToXmlFile();   // #11 移动后立即持久化
}

void FormLabels::onActionMoveDownTriggered()
{
    const QModelIndex curIdx = m_labelsTree->currentIndex();
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
        m_labelsTree->setCurrentIndex(movedItem->index());
    resyncSets();
    saveLabelToXmlFile();   // #11 移动后立即持久化
}

void FormLabels::onActionMoveLeftTriggered()
{
    const QModelIndex curIdx = m_labelsTree->currentIndex();
    if (!curIdx.isValid())
        return;

    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    if (curItem == nullptr)
        return;

    QStandardItem *parentItem = curItem->parent();
    if (parentItem == nullptr)
        return;   // 顶层节点没有"左移"

    QStandardItem *parentLabel = parentItem->parent();
    const int nInsertRow = parentItem->index().row() + 1;
    const QList<QStandardItem *> rowCur = parentItem->takeRow(curIdx.row());
    if (rowCur.isEmpty())
        return;

    if (parentLabel == nullptr)
        m_modelAllLabels->insertRow(nInsertRow, rowCur);
    else
        parentLabel->insertRow(nInsertRow, rowCur);

    QStandardItem *movedItem = rowCur.value(0);
    if (movedItem != nullptr)
        m_labelsTree->setCurrentIndex(movedItem->index());
    resyncSets();
    saveLabelToXmlFile();   // #11 移动后立即持久化
}

// 作为其上方的节点的子节点
void FormLabels::onActionMoveRightTriggered()
{
    const QModelIndex curIdx = m_labelsTree->currentIndex();
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
        m_labelsTree->setCurrentIndex(movedItem->index());
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
    if (m_namingNewLabel) {
        // 新建标签时的"起名"：只是给新节点取名字，不需要问要不要同步到文件
        m_namingNewLabel = false;
    } else if (!old.isEmpty() && !m_suppressSyncPrompt && m_sqlOperation != nullptr) {
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

    QPushButton *libOnly = box.addButton(tr("只改标签库"), QMessageBox::AcceptRole);
    QPushButton *sync = qLFiles.isEmpty() ? nullptr : box.addButton(tr("同时改文件上的标签"), QMessageBox::ActionRole);
    QPushButton *undo = box.addButton(tr("撤销改名"), QMessageBox::RejectRole);
    box.setDefaultButton(libOnly);
    box.exec();

    if (box.clickedButton() == undo) {
        // 程序化改回旧名：期间不要再弹同步询问。
        m_suppressSyncPrompt = true;
        const QList<QStandardItem *> items = m_modelAllLabels->findItems(sNewTag, Qt::MatchExactly | Qt::MatchRecursive);
        for (QStandardItem *labelItem : items) {
            if (labelItem != nullptr && labelItem->text() == sNewTag) {
                labelItem->setText(sOldTag);
                break;
            }
        }
        m_suppressSyncPrompt = false;
        return;
    }

    if (sync != nullptr && box.clickedButton() == sync) {
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
    for (const QString &sFilePath : qLFiles) {
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

/**
 * 新建标签时自动取一个没被占用的名字：
 * 第一次是"新标签"，之后依次是"新标签(1)"、"新标签(2)"……
 * （"新标签"本身也是标签库里的普通名字，所以按整棵树的现有名字去重，忽略大小写。）
 */
QString FormLabels::nextNewLabelName() const
{
    QSet<QString> setNames;
    for (int i = 0; i < m_modelAllLabels->rowCount(); ++i) {
        const QStandardItem *labelItem = m_modelAllLabels->item(i);
        if (labelItem == nullptr)
            continue;
        QList<const QStandardItem *> lstStack;
        lstStack << labelItem;
        while (!lstStack.isEmpty()) {
            const QStandardItem *cur = lstStack.takeFirst();
            if (cur == nullptr)
                continue;
            setNames.insert(cur->text().toCaseFolded());
            for (int r = 0; r < cur->rowCount(); ++r)
                lstStack << cur->child(r);
        }
    }

    const QString sBase = QString::fromUtf8(NEW_LABEL_NAME);
    if (!setNames.contains(sBase.toCaseFolded()))
        return sBase;

    for (int n = 1; n < 100000; ++n) {
        const QString sCandidate = QStringLiteral("%1(%2)").arg(sBase).arg(n);
        if (!setNames.contains(sCandidate.toCaseFolded()))
            return sCandidate;
    }
    return sBase;
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
