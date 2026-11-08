#include "formpageprogressinfo.h"
#include "ui_formpageprogressinfo.h"

#include <QPushButton>

FormPageProgressInfo::FormPageProgressInfo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormPageProgressInfo)
{
    ui->setupUi(this);

    ui->plainTextEdit->setMaximumBlockCount(100);

    // 显式连接停止按钮（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(ui->btnStop, &QPushButton::clicked, this, &FormPageProgressInfo::onBtnStopClicked);
}

FormPageProgressInfo::~FormPageProgressInfo()
{
    delete ui;
}

void FormPageProgressInfo::onRecvProgress(QString sProcessedFilePath)
{
    ui->plainTextEdit->appendPlainText(sProcessedFilePath);
    ui->plainTextEdit->moveCursor(QTextCursor::End);
}

void FormPageProgressInfo::onBtnStopClicked()
{
    emit sendStop();
}

void FormPageProgressInfo::clear()
{
    ui->plainTextEdit->clear();
}
