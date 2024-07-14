#ifndef DLGAUTHOR_H
#define DLGAUTHOR_H

#include <QDialog>

namespace Ui {
class DlgAuthor;
}

class DlgAuthor : public QDialog
{
    Q_OBJECT

public:
    explicit DlgAuthor(QWidget *parent = nullptr);
    ~DlgAuthor();

private:
    Ui::DlgAuthor *ui;
};

#endif // DLGAUTHOR_H
