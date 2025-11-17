#include "dlgsearchconfig.h"
#include "ui_dlgsearchconfig.h"

#include "common.h"
#include "configoperation.h"

DlgSearchConfig::DlgSearchConfig(QWidget *parent, QString sConfigFilePath) :
    QDialog(parent),
    m_sConfigFilePath(sConfigFilePath),
    ui(new Ui::DlgSearchConfig)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    readSearchConfig();
}

DlgSearchConfig::~DlgSearchConfig()
{
    delete ui;
}

void DlgSearchConfig::readSearchConfig()
{
    SearchConfig sc = ConfigOperation::readSearchConfig(m_sConfigFilePath);

    if(sc.bFromCache)
    {
        ui->rbFromCache->setChecked(true);
        ui->rbFromDisk->setChecked(false);
    }
    else
    {
        ui->rbFromCache->setChecked(false);
        ui->rbFromDisk->setChecked(true);
    }

    if(sc.logic == labelLogic::AND)
    {
        ui->rbLogicAnd->setChecked(true);
        ui->rbLogicOr->setChecked(false);
    }
    else if(sc.logic == labelLogic::OR)
    {
        ui->rbLogicAnd->setChecked(false);
        ui->rbLogicOr->setChecked(true);
    }
}

void DlgSearchConfig::writeSearchConfig()
{
    SearchConfig sc;

    if(!ui->rbFromCache->isChecked())
        sc.bFromCache = false;

    if(!ui->rbLogicAnd->isChecked())
        sc.logic = labelLogic::OR;

    ConfigOperation::writeSearchConfig(m_sConfigFilePath, sc);
}

void DlgSearchConfig::on_btnOk_clicked()
{
    writeSearchConfig();
    accept();
}

void DlgSearchConfig::on_btnCancel_clicked()
{
    reject();
}
