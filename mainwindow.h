#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFile>
#include <QMainWindow>
#include <QTimer>

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

    /** 在状态栏提示一句话（单实例拦下第二次启动时用） */
    void showHintMessage(const QString &sMessage);

protected:
    void initLogMessageHanlder();
    void applyVectorIcons();   // 命令条图标改代码绘制
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void saveWindowGeometry();
    // 菜单动作的处理函数：由构造函数中的 connect 显式连接，
    // 不再使用 on_<对象名>_<信号名> 的自动连接（connectSlotsByName）。
    void onActionHelpTriggered();

    void onActionAuthorTriggered();
    /** 打开【AI 打标设置】：本地 Ollama / 云端 API */
    void onActionAiSettingsTriggered();

    /** 检查 AI 打标依赖：脚本是否存在、Python 是否可用、依赖库是否齐全 */
    void checkAIEnvironment();

    QFile                   m_logFile;
    QTimer                 *m_geometryTimer;

    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
