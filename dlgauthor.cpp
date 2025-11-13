#include "dlgauthor.h"
#include "ui_dlgauthor.h"

DlgAuthor::DlgAuthor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgAuthor)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
}

DlgAuthor::~DlgAuthor()
{
    delete ui;
}
