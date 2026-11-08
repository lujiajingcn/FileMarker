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

private:
    // 菜单动作的处理函数：由构造函数中的 connect 显式连接，
    // 不再使用 on_<对象名>_<信号名> 的自动连接（connectSlotsByName）。
    void onActionHelpTriggered();

    void onActionAuthorTriggered();

    QFile                   m_logFile;

    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
