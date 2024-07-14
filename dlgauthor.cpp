#include "dlgauthor.h"
#include "ui_dlgauthor.h"

DlgAuthor::DlgAuthor(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DlgAuthor)
{
    ui->setupUi(this);
}

DlgAuthor::~DlgAuthor()
{
    delete ui;
}
