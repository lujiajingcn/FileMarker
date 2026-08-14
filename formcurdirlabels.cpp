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
    for (QMap<QString, QSet<QString>>::const_iterator cItr = mapDirAndLabels.begin(); cItr != mapDirAndLabels.end(); cItr++) {
        QString dir = cItr.key();
        QStandardItem *item = getItem(dir);
        if(nullptr != item)
            m_modelLabels->removeRow(item->row());

        QStandardItem *itemDir = new QStandardItem(dir);
        m_modelLabels->appendRow(itemDir);

        QSet<QString> setLabels = cItr.value();
        foreach (const QString &sLabel, setLabels)
            itemDir->appendRow(new QStandardItem(sLabel));
    }
}

void FormCurDirLabels::on_treeView_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);

    // P1-11：selectedIndexes() 会为同一行的每一列都返回索引，导致同一标签被重复返回。
    // 用 selectedRows(0) 只取每行第 0 列，并用 QSet 去重。
    QStringList qLLabels;
    QSet<QString> stLabels;
    QItemSelectionModel *selectionModel = ui->treeView->selectionModel();
    QModelIndexList selectedIndexes = selectionModel->selectedRows(0);
    foreach (const QModelIndex &idx, selectedIndexes)
    {
        QStandardItem *item = m_modelLabels->itemFromIndex(idx);
        if (item != nullptr)
            stLabels.insert(item->text());
    }
    qLLabels << stLabels.values();

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
