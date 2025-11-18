#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "introductionwidget.h"
#include "common.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void initLogMessageHanlder();

private slots:
    void on_actionHelp_triggered();

    void on_actionAuthor_triggered();


private:
    QFile                   m_logFile;
//    IntroductionWidget      *wIntroduction;

    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
