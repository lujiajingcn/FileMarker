#include "formcurdirlabels.h"
#include "ui_formcurdirlabels.h"

FormCurDirLabels::FormCurDirLabels(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormCurDirLabels)
{
    ui->setupUi(this);

    m_modelLabels = new QStandardItemModel(this);
    ui->treeView->setModel(m_modelLabels);
}

FormCurDirLabels::~FormCurDirLabels()
{
    delete ui;
}

void FormCurDirLabels::onRecvDirAndLabels(QMap<QString, QSet<QString>> mapDirAndLabels)
{
    for (QMap<QString, QSet<QString>>::const_iterator cItr = mapDirAndLabels.begin(); cItr != mapDirAndLabels.end(); cItr++)
    {
        QString dir = cItr.key();
        QStandardItem *item = getItem(dir);
        if(nullptr != item)
            m_modelLabels->removeRow(item->row());

        QStandardItem *itemDir = new QStandardItem(dir);
        m_modelLabels->appendRow(itemDir);

        QSet<QString> setLabels = cItr.value();
        foreach (const QString &sLabel, setLabels)
        {
            QStandardItem *itemLabel = new QStandardItem(sLabel);
            itemDir->appendRow(itemLabel);
        }
    }
}

void FormCurDirLabels::on_treeView_clicked(const QModelIndex &index)
{
    QStringList qLLabels;
    QItemSelectionModel *selectionModel = ui->treeView->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
    foreach (const QModelIndex &index, selectedIndexes)
    {
        qLLabels<<m_modelLabels->itemFromIndex(index)->text();
    }
    sendSelLabels(qLLabels);
}

QStandardItem *FormCurDirLabels::getItem(QString text)
{
    int rowCount = m_modelLabels->rowCount();
    for(int i = 0; i < rowCount; i++){
        QString itemText = m_modelLabels->item(i)->text();
        if(QString::compare(text, itemText) == 0)
            return m_modelLabels->item(i);
    }
    return nullptr;
}
