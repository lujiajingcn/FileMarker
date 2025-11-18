#include "formlabels.h"
#include "ui_formlabels.h"

#include <QToolButton>
#include <QSpacerItem>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QAction>
#include <QMenu>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include "common.h"

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
        QStandardItem *item = new QStandardItem(sName);
        if((*cItr).lstChild.count() > 0)
        {
            traverseWriteLabel(item, (*cItr).lstChild);
        }
        m_modelAllLabels->appendRow(item);
    }
}

QStringList FormLabels::getSelLabels()
{
    QStringList qLLabels;
    QItemSelectionModel *selectionModel = m_tvAllLabels->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
    foreach (const QModelIndex &index, selectedIndexes)
    {
        qLLabels<<m_modelAllLabels->itemFromIndex(index)->text();
    }
    return qLLabels;
}

void FormLabels::addItem(QStandardItem *parentItem)
{
    QStandardItem *newItem = new QStandardItem(NEW_LABEL_NAME);
    if(nullptr == parentItem)
    {
        m_modelAllLabels->appendRow(newItem);
    }
    else
    {
        parentItem->appendRow(newItem);
    }
    m_tvAllLabels->setCurrentIndex(newItem->index());
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
        QStandardItem *curItem = m_modelAllLabels->takeItem(nCurRow);
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
        QStandardItem *curItem = parentItem->takeChild(nCurRow);
        if(curItem == nullptr)
            return;

        QStandardItem *lastUpItem = parentItem->takeChild(nCurRow - 1);

        parentItem->setChild(nCurRow - 1, curItem);
        parentItem->setChild(nCurRow, lastUpItem);

        QModelIndex index = curItem->index();
        m_tvAllLabels->setCurrentIndex(index);
    }
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

        QStandardItem *curItem = m_modelAllLabels->takeItem(nCurRow);
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

        QStandardItem *curItem = parentItem->takeChild(nCurRow);
        if(curItem == nullptr)
            return;

        QStandardItem *lastDownItem = parentItem->takeChild(nCurRow + 1);

        parentItem->setChild(nCurRow + 1, curItem);
        parentItem->setChild(nCurRow, lastDownItem);

        QModelIndex index = curItem->index();
        m_tvAllLabels->setCurrentIndex(index);
    }
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
        curItem = parentItem->takeChild(curIdx.row());
        parentItem->removeRow(curIdx.row());
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
            curItem = m_modelAllLabels->takeItem(curIdx.row());
            m_modelAllLabels->removeRow(curIdx.row());
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
             curItem = parentItem->takeChild(curIdx.row());
             parentItem->removeRow(curIdx.row());
             upItem->appendRow(curItem);
        }
    }

    QModelIndex index = curItem->index();
    m_tvAllLabels->setCurrentIndex(index);

}

// todo 这个判断是在节点的文本内容已经修改结束时执行，如果标签重复，则应该叫节点内容改回旧值。
void FormLabels::onItemChanged(QStandardItem *item)
{
    QString sLabel = item->text();
    if(m_setLabels.contains(sLabel))
    {
        QMessageBox::information(this, "提示", QString("标签【%1】已经存在").arg(sLabel));
    }
    else
    {
        m_setLabels.insert(sLabel);
    }
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
        QStandardItem *itemChild = new QStandardItem((*cItr).sName);
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
        labelInfo.sName = itemChild->text();
        traverseReadLabel(itemChild, labelInfo.lstChild);
        lstLabelInfo.push_back(labelInfo);
    }
}

void FormLabels::updateAllLabelList(QVector<QString> vtAllLabels)
{
    m_modelAllLabels->removeRows(0, m_modelAllLabels->rowCount());
    QStandardItem *item = nullptr;
    for(QVector<QString>::iterator it = vtAllLabels.begin(); it != vtAllLabels.end(); it++)
    {
        item = new QStandardItem(*it);
        m_modelAllLabels->appendRow(item);
    }
}

void FormLabels::on_treeView_clicked(const QModelIndex &index)
{
    QStringList qLLabels;
    QItemSelectionModel *selectionModel = m_tvAllLabels->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
    foreach (const QModelIndex &index, selectedIndexes)
    {
        qLLabels<<m_modelAllLabels->itemFromIndex(index)->text();
    }
    sendSelLabels(qLLabels);
}
