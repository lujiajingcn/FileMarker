#include "formlabels.h"
#include "ui_formlabels.h"

#include <QToolButton>
#include <QSpacerItem>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QAction>
#include <QMenu>
#include <functional>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include "common.h"
#include "utility.h"

FormLabels::FormLabels(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormLabels)
{
    ui->setupUi(this);

    m_tvAllLabels = ui->treeView;
    m_modelAllLabels = new QStandardItemModel(this);
    m_tvAllLabels->setModel(m_modelAllLabels);

    setToolButtonDefaultAction();
    initLabels();

    connect(m_modelAllLabels, &QStandardItemModel::itemChanged, this, &FormLabels::onItemChanged);
}

FormLabels::~FormLabels()
{
    saveLabelToXmlFile();
    delete ui;
}

void FormLabels::addToolButtonsToMenu(QMenu *menuLabelManage)
{
    menuLabelManage->addAction(ui->actionAdd);
    menuLabelManage->addAction(ui->actionAddChild);
    menuLabelManage->addAction(ui->actionDelete);
    menuLabelManage->addAction(ui->actionMoveUp);
    menuLabelManage->addAction(ui->actionMoveDown);
    menuLabelManage->addAction(ui->actionMoveLeft);
    menuLabelManage->addAction(ui->actionMoveRight);
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
        // #1：保留从文件读到的 UUID（为空则新生成），避免保存时 UUID 被清空。
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
    // P1-10：selectedIndexes() 会为同一选中行的每一列都返回一个索引，若不只取第 0 列，
    // 会导致同一标签被重复返回多次（搜索时会重复匹配）。这里用 QSet 去重。
    QSet<QString> stLabels;
    QModelIndexList selectedIndexes = selectionModel->selectedRows(0);
    foreach (const QModelIndex &index, selectedIndexes)
    {
        QStandardItem *item = m_modelAllLabels->itemFromIndex(index);
        if (item != nullptr)
            stLabels.insert(item->text());
    }
    qLLabels << stLabels.values();
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
    saveLabelToXmlFile();   // #11 新增标签后立即持久化
}

void FormLabels::on_actionAdd_triggered()
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

void FormLabels::on_actionAddChild_triggered()
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

void FormLabels::on_actionDelete_triggered()
{
    QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if(!curIdx.isValid())
    {
        return;
    }
    int nCurRow = curIdx.row();
    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    QStandardItem *parentItem = curItem->parent();
    if(parentItem == nullptr)
    {
        m_modelAllLabels->removeRow(nCurRow);
    }
    else
    {
        parentItem->removeRow(nCurRow);
        m_tvAllLabels->setCurrentIndex(parentItem->index());
    }
    // #3：删除可能包含整棵子树，删除“之后”按当前模型重新同步去重集合，
    // 必须在 removeRow 之后调用，否则 m_setLabels 会残留被删标签名，
    // 导致之后同名的外部/AI 标签被 m_setLabels.contains() 误判为已存在而静默丢弃（P2-1）。
    resyncSets();
    saveLabelToXmlFile();   // #11 删除后立即持久化
}

void FormLabels::on_actionMoveUp_triggered()
{
    QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if(!curIdx.isValid())
    {
        return;
    }

    int nCurRow = curIdx.row();
    if(curIdx.row() == 0)
    {
        return;
    }

    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    QStandardItem *parentItem = curItem->parent();
    if(parentItem == nullptr)
    {
        // P2-19：移除内部 curItem 的类型声明，避免遮蔽外层变量。
        curItem = m_modelAllLabels->takeItem(nCurRow);
        if(curItem == nullptr)
            return;

        QStandardItem *lastUpItem = m_modelAllLabels->takeItem(nCurRow - 1);

        m_modelAllLabels->setItem(nCurRow - 1, curItem);
        m_modelAllLabels->setItem(nCurRow, lastUpItem);

        QModelIndex index = m_modelAllLabels->index(nCurRow - 1, 0, QModelIndex());
        m_tvAllLabels->setCurrentIndex(index);
    }
    else
    {
        curItem = parentItem->takeChild(nCurRow);
        if(curItem == nullptr)
            return;

        QStandardItem *lastUpItem = parentItem->takeChild(nCurRow - 1);

        parentItem->setChild(nCurRow - 1, curItem);
        parentItem->setChild(nCurRow, lastUpItem);

        QModelIndex index = curItem->index();
        m_tvAllLabels->setCurrentIndex(index);
    }
    saveLabelToXmlFile();   // #11 移动后立即持久化
}

void FormLabels::on_actionMoveDown_triggered()
{
    QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if(!curIdx.isValid())
    {
        return;
    }

    int nCurRow = curIdx.row();

    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    QStandardItem *parentItem = curItem->parent();
    if(parentItem == nullptr)
    {
        if(nCurRow + 1 == m_modelAllLabels->rowCount())
        {
            return;
        }

        // P2-19：移除内部 curItem 的类型声明，避免遮蔽外层变量。
        curItem = m_modelAllLabels->takeItem(nCurRow);
        if(curItem == nullptr)
            return;

        QStandardItem *lastDownItem = m_modelAllLabels->takeItem(nCurRow + 1);

        m_modelAllLabels->setItem(nCurRow + 1, curItem);
        m_modelAllLabels->setItem(nCurRow, lastDownItem);

        QModelIndex index = m_modelAllLabels->index(nCurRow + 1, 0, QModelIndex());
        m_tvAllLabels->setCurrentIndex(index);
    }
    else
    {
        if(nCurRow + 1 == parentItem->rowCount())
        {
            return;
        }

        curItem = parentItem->takeChild(nCurRow);
        if(curItem == nullptr)
            return;

        QStandardItem *lastDownItem = parentItem->takeChild(nCurRow + 1);

        parentItem->setChild(nCurRow + 1, curItem);
        parentItem->setChild(nCurRow, lastDownItem);

        QModelIndex index = curItem->index();
        m_tvAllLabels->setCurrentIndex(index);
    }
    saveLabelToXmlFile();   // #11 移动后立即持久化
}

void FormLabels::on_actionMoveLeft_triggered()
{
    QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if(!curIdx.isValid())
    {
        return;
    }

    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    QStandardItem *parentItem = curItem->parent();
    if(parentItem == nullptr)
    {
        return;
    }
    else
    {
        // P0-1：takeChild 已将该节点从父节点移除并返回，这里再次 removeRow 会误删
        // 原本位于 curIdx.row() 之后的相邻节点。移除冗余的 removeRow 调用。
        curItem = parentItem->takeChild(curIdx.row());
        QStandardItem *pParentItem = parentItem->parent();
        if(pParentItem == nullptr)
        {
            m_modelAllLabels->insertRow(parentItem->index().row() + 1, curItem);
        }
        else
        {
            pParentItem->insertRow(parentItem->index().row() + 1, curItem);
        }
    }

    QModelIndex index = curItem->index();
    m_tvAllLabels->setCurrentIndex(index);
    saveLabelToXmlFile();   // #11 移动后立即持久化

}

// 作为其上方的节点的子节点
void FormLabels::on_actionMoveRight_triggered()
{
    QModelIndex curIdx = m_tvAllLabels->currentIndex();
    if(!curIdx.isValid() || m_modelAllLabels->rowCount() <= 1)
    {
        return;
    }

    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIdx);
    QStandardItem *parentItem = curItem->parent();
    if(parentItem == nullptr)
    {
       QStandardItem *upItem = m_modelAllLabels->item(curIdx.row() - 1);
       if(upItem == nullptr)
       {
           return;
       }
       else
       {
            // P0-2：takeItem 已将该节点移除并返回，这里再次 removeRow 会误删相邻节点。
            curItem = m_modelAllLabels->takeItem(curIdx.row());
            upItem->appendRow(curItem);
       }
    }
    else
    {
        QStandardItem *upItem = parentItem->child(curIdx.row() - 1);
        if(upItem == nullptr)
        {
            return;
        }
        else
        {
             // P0-2：takeChild 已将该节点移除并返回，这里再次 removeRow 会误删相邻节点。
             curItem = parentItem->takeChild(curIdx.row());
             upItem->appendRow(curItem);
        }
    }

    QModelIndex index = curItem->index();
    m_tvAllLabels->setCurrentIndex(index);
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
    item->setText(sLabel);

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
}


QStandardItem* FormLabels::createLabelItem(const QString &name, const QString &uuid)
{
    QStandardItem *item = new QStandardItem(name);
    // #1：每个标签节点携带稳定 UUID，保存时回写，避免退出保存后标识全部丢失。
    item->setData(uuid.isEmpty() ? Utility::createUuid() : uuid, LabelUuidRole);
    return item;
}

void FormLabels::registerItem(QStandardItem *item)
{
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
        for (int i = 0; i < it->rowCount(); ++i) {
            QStandardItem *child = it->child(i);
            if (child->text() == name)
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
        // #1：写子节点时同样保留 UUID（为空则新生成）。
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
    int nRowCount = item->rowCount();
    for(int i = 0; i < nRowCount; i++)
    {
        QStandardItem *itemChild = item->child(i);
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

void FormLabels::on_treeView_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    // P1-10/P3-32：复用 getSelLabels，避免与 getSelLabels 重复逻辑，且同样去除 selectedIndexes 多列重复。
    sendSelLabels(getSelLabels());
}

void FormLabels::onRecvLabels(QString sLabels)
{
    QStringList qLLabels = sLabels.split(",");
    foreach (QString sLabel , qLLabels)
    {
        sLabel = sLabel.trimmed();
        if (sLabel.isEmpty())
            continue;
        if(!m_setLabels.contains(sLabel))
        {
            QStandardItem *newItem = createLabelItem(sLabel);
            m_modelAllLabels->appendRow(newItem);
            registerItem(newItem);
        }
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

    QStringList qLLabels = sLabels.split(",");
    foreach (QString sLabel , qLLabels)
    {
        sLabel = sLabel.trimmed();
        if (sLabel.isEmpty())
            continue;
        if(!m_setLabels.contains(sLabel))
        {
            QStandardItem *newItem = createLabelItem(sLabel);
            itemLabelsGeneratedByAI->appendRow(newItem);
            registerItem(newItem);
        }
    }
    saveLabelToXmlFile();   // #11 AI 生成标签后立即持久化
}
