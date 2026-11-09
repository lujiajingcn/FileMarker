#include "dlgsearchconfig.h"
#include "ui_dlgsearchconfig.h"

#include <QPushButton>

#include "common.h"
#include "configoperation.h"

DlgSearchConfig::DlgSearchConfig(QWidget *parent, QString sConfigFilePath) :
    QDialog(parent),
    m_sConfigFilePath(sConfigFilePath),
    ui(new Ui::DlgSearchConfig)
{
    ui->setupUi(this);

    // 显式连接按钮信号（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(ui->btnOk, &QPushButton::clicked, this, &DlgSearchConfig::onBtnOkClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &DlgSearchConfig::onBtnCancelClicked);

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

    // 改用正向逻辑明确设置每个字段的值，不再依赖 SearchConfig 构造函数的默认值
    // 碰巧匹配"选中"情况。若默认值日后改变，此处的负逻辑将静默出错。
    sc.bFromCache = ui->rbFromCache->isChecked();

    if (ui->rbLogicAnd->isChecked())
        sc.logic = labelLogic::AND;
    else
        sc.logic = labelLogic::OR;

    ConfigOperation::writeSearchConfig(m_sConfigFilePath, sc);
}

void DlgSearchConfig::onBtnOkClicked()
{
    writeSearchConfig();
    accept();
}

void DlgSearchConfig::onBtnCancelClicked()
{
    reject();
}
