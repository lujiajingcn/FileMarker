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
        QStandardItem *itemDir = new QStandardItem(cItr.key());
        m_modelLabels->appendRow(itemDir);

        QSet<QString> setLabels = cItr.value();
        foreach (const QString &sLabel, setLabels)
        {
            QStandardItem *itemLabel = new QStandardItem(sLabel);
            itemDir->appendRow(itemLabel);
        }
    }
}
