#include "dlglabel.h"
#include "ui_dlglabel.h"
#include "sqliteoperation.h"

DlgLabel::DlgLabel(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgLabel)
{
    ui->setupUi(this);

    // 去掉对话框问号
    setWindowFlags(windowFlags()&~Qt::WindowContextHelpButtonHint);

    m_lvAllLabels = ui->lvAllLabels;
    m_modelAllLabels = new QStandardItemModel;
    m_lvAllLabels->setModel(m_modelAllLabels);
    m_lvAllLabels->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_lvSelLabels = ui->lvSelLabels;
    m_modelSelLabels = new QStandardItemModel;
    m_lvSelLabels->setModel(m_modelSelLabels);
    m_lvSelLabels->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

DlgLabel::~DlgLabel()
{
    delete ui;
}

void DlgLabel::setIsDir(bool bIsDir)
{
    if(!bIsDir)
    {
        ui->cbSetAllFiles->hide();
    }
}

void DlgLabel::setAllLabels(QVector<QString> vtAllLabels)
{
    m_modelAllLabels->removeRows(0, m_modelAllLabels->rowCount());
    QStandardItem *item = nullptr;
    for(QVector<QString>::iterator it = vtAllLabels.begin(); it != vtAllLabels.end(); it++)
    {
        item = new QStandardItem(*it);
        m_modelAllLabels->appendRow(item);
    }
}

void DlgLabel::setSelLabels(QStringList labels)
{
    m_modelSelLabels->removeRows(0, m_modelSelLabels->rowCount());
    QStandardItem *item = nullptr;
    for(int i = 0; i < labels.count(); i++)
    {
        item = new QStandardItem(labels.at(i));
        m_modelSelLabels->appendRow(item);
    }
}

void DlgLabel::setLabel(QString sLabel)
{
    ui->leLabel->setText(sLabel);
}

QStringList DlgLabel::getLabels()
{
    return m_sLabels;
}

QStringList DlgLabel::getAllLabels()
{
    return m_sAllLabels;
}

bool DlgLabel::isSetAllFiles()
{
    return ui->cbSetAllFiles->checkState() == Qt::Checked ? true : false;
}

void DlgLabel::setSqlOperation(SqliteOperation *sqlOperation)
{
    m_sqlOperation = sqlOperation;
}

void DlgLabel::on_btnOk_clicked()
{
    int nRowCount = m_modelAllLabels->rowCount();
    for (int i = 0; i < nRowCount; i++)
    {
        m_sAllLabels << m_modelAllLabels->item(i)->text();
    }

    nRowCount = m_modelSelLabels->rowCount();
    for (int i = 0; i < nRowCount; i++)
    {
        m_sLabels << m_modelSelLabels->item(i)->text();
        m_sAllLabels << m_modelSelLabels->item(i)->text();
    }
    accept();
}

void DlgLabel::on_btnCancel_clicked()
{
    reject();
}

void DlgLabel::on_lvAllLabels_clicked(const QModelIndex &index)
{
    QString sLabel = m_modelAllLabels->item(index.row())->text();
    ui->leLabel->setText(sLabel);
}

void DlgLabel::on_btnMoveRight_clicked()
{
    QModelIndex curIndex = m_lvAllLabels->currentIndex();
    if(curIndex.row() == -1)
    {
        return;
    }
    QStandardItem *curItem = m_modelAllLabels->itemFromIndex(curIndex);
    QString sCurLabel = curItem->text();
    if(isItemExist(m_modelSelLabels,sCurLabel))
    {
        return;
    }

    QStandardItem *item = new QStandardItem;
    item->setText(sCurLabel);
    m_modelSelLabels->appendRow(item);

    m_modelAllLabels->removeRow(curIndex.row());
}

void DlgLabel::on_btnMoveLeft_clicked()
{
    QModelIndex curIndex = m_lvSelLabels->currentIndex();
    if(curIndex.row() == -1)
    {
        return;
    }
    QStandardItem *curItem = m_modelSelLabels->itemFromIndex(curIndex);
    QString sCurLabel = curItem->text();
    if(isItemExist(m_modelAllLabels,sCurLabel))
    {
        return;
    }

    QStandardItem *item = new QStandardItem;
    item->setText(sCurLabel);
    m_modelAllLabels->appendRow(item);

    m_modelSelLabels->removeRow(curIndex.row());
}

bool DlgLabel::isItemExist(QStandardItemModel *model, QString sText)
{
    int nRowCount = model->rowCount();
    QString sCurText;
    for(int i = 0; i < nRowCount; i++)
    {
        sCurText = model->item(i)->text();
        if(QString::compare(sCurText, sText) == 0)
        {
            return true;
        }
    }
    return false;
}

void DlgLabel::on_lvAllLabels_doubleClicked(const QModelIndex &index)
{
    on_btnMoveRight_clicked();
}

void DlgLabel::on_lvSelLabels_doubleClicked(const QModelIndex &index)
{
    on_btnMoveLeft_clicked();
}

void DlgLabel::on_btnAddLable_clicked()
{
    QString sLabel = ui->leLabel->text();
    if(isItemExist(m_modelAllLabels, sLabel))
    {
        return;
    }
    QStandardItem *item = new QStandardItem;
    item->setText(sLabel);
    m_modelAllLabels->appendRow(item);

//    m_sqlOperation->addLabel(sLabel);
}

void DlgLabel::on_btnDelLabel_clicked()
{
    QModelIndex curIndex = m_lvAllLabels->currentIndex();
    if(curIndex.row() == -1)
    {
        return;
    }

    QString sLabel = m_modelAllLabels->item(curIndex.row())->text();
    m_modelAllLabels->removeRow(curIndex.row());

//    m_sqlOperation->delLabel(sLabel);
}

void DlgLabel::on_btnModLabel_clicked()
{
    QString sNewLabel = ui->leLabel->text();
    if(isItemExist(m_modelAllLabels, sNewLabel))
    {
        return;
    }
    QModelIndex curIndex = m_lvAllLabels->currentIndex();
    if(curIndex.row() == -1)
    {
        return;
    }
    QStandardItem *item = m_modelAllLabels->item(curIndex.row());
    QString sOldLabel = item->text();
    item->setText(sNewLabel);

//    m_sqlOperation->modLabel(sOldLabel, sNewLabel);
}
