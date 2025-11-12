#include "formpageprogressinfo.h"
#include "ui_formpageprogressinfo.h"

FormPageProgressInfo::FormPageProgressInfo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormPageProgressInfo)
{
    ui->setupUi(this);

    ui->plainTextEdit->setMaximumBlockCount(100);
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
