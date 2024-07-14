#ifndef FORMTEST_H
#define FORMTEST_H

#include <QWidget>

namespace Ui {
class FormTest;
}

class FormTest : public QWidget
{
    Q_OBJECT

public:
    explicit FormTest(QWidget *parent = nullptr);
    ~FormTest() override;

    void paintEvent(QPaintEvent *ev) override;

private:
    Ui::FormTest *ui;
};

#endif // FORMTEST_H
