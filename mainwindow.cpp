#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "dlgauthor.h"
#include "utility.h"
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    g_sAppDir = QCoreApplication::applicationDirPath();

    ui->setupUi(this);

    // 保证默认窗口下详情列表的“标签”列也能完整显示（列宽策略见 FormPageFiles）。
    setMinimumSize(1000, 650);
    // 默认开大一点：994x663 时名称列会被压得很窄；不超过屏幕可用区域。
    const QRect rcAvail = QApplication::primaryScreen()->availableGeometry();
    resize(qMin(1280, rcAvail.width() - 80), qMin(820, rcAvail.height() - 80));

    // 恢复上次的窗口大小/位置；恢复上次访问的目录（等窗口显示后执行，保证模型已就绪）
    {
        QSettings settings(g_sAppDir + QStringLiteral("/FileMarker.ini"), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        settings.setIniCodec("UTF-8");
#endif
        const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
        if (!geometry.isEmpty())
            restoreGeometry(geometry);
    }
    QTimer::singleShot(0, this, [this]() {
        ui->wFileBrowser->restoreLastDir();
    });

    initLogMessageHanlder();

    // 显式连接菜单动作（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(ui->actionHelp, &QAction::triggered, this, &MainWindow::onActionHelpTriggered);
    connect(ui->actionAuthor, &QAction::triggered, this, &MainWindow::onActionAuthorTriggered);

    // 将【标签管理】窗口的工具栏操作添加到主窗口的菜单中
    ui->dwcLabels->addToolButtonsToMenu(ui->menuLabelManage);

    // 将菜单栏中的动作放到【文件管理】中实现
    ui->wFileBrowser->setActions(ui->actionAddLabelbyAI, ui->actionAddLabels, ui->actionDelLabels, ui->actionSearchFilesbyLabels, ui->actionTraverseSelDirs, ui->actionSearchConfig);

    // 【标签管理】中选中的标签，同步到【文件管理】中
    connect(ui->dwcLabels, &FormLabels::sendSelLabels, ui->wFileBrowser, &FormFileBrowser::onRecvSelLabels);

    // 【当前目录标签】中选中的标签，同步到【文件管理类】中
    connect(ui->dwcCurDirLabels, &FormCurDirLabels::sendSelLabels, ui->wFileBrowser, &FormFileBrowser::onRecvSelLabels);

    // 遍历选中文件夹，检索出其中文件的所有标签，显示在【当前目录标签】
    connect(ui->wFileBrowser, &FormFileBrowser::sendDirAndLabels, ui->dwcCurDirLabels, &FormCurDirLabels::onRecvDirAndLabels);

    // 切换目录时先清空【当前目录标签】，再显示新目录的标签
    connect(ui->wFileBrowser, &FormFileBrowser::sendClearDirLabels, ui->dwcCurDirLabels, &FormCurDirLabels::clearLabels);

    // 搜索/遍历等耗时操作的文字反馈（状态栏）
    connect(ui->wFileBrowser, &FormFileBrowser::sendStatusMessage, this, [this](const QString &sMsg) {
        statusBar()->showMessage(sMsg, 5000);
    });

    // 手动编辑【标签】列内容后，将编辑后的内容传递给【标签管理】
    // 编辑后的标签由英文逗号分隔，形如 照片,故宫,2025
    // todo 传递路径过长，应该是结构不合理，后续改进
    connect(ui->wFileBrowser, &FormFileBrowser::sendLabels, ui->dwcLabels, &FormLabels::onRecvLabels);

    // AI根据文件信息自动生成的标签，添加到【标签管理】中的“AI自动生成标签”
    connect(ui->wFileBrowser, &FormFileBrowser::sendLabelsGeneratedByAI, ui->dwcLabels, &FormLabels::onRecvLabelsGeneratedByAI);
}

MainWindow::~MainWindow()
{
    // 保存窗口几何信息，下次启动恢复
    {
        QSettings settings(g_sAppDir + QStringLiteral("/FileMarker.ini"), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        settings.setIniCodec("UTF-8");
#endif
        settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    }

    // 先销毁界面（析构过程中产生的日志仍写入文件），再把日志落盘并断开设备，
    // 最后恢复默认 handler：否则窗口销毁后 Qt 收尾阶段的日志会写进已关闭的
    // 文件，出现 "QIODevice::write (QObject): device not open"。
    delete ui;

    Utility::tsLogInfo.flush();
    Utility::tsLogInfo.setDevice(nullptr);
    qInstallMessageHandler(nullptr);
}

void MainWindow::initLogMessageHanlder()
{
    m_logFile.setFileName(g_sAppDir + "/" + LOG_FILE);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // 例如程序装在 C:\Program Files 这类只读目录时打开会失败；
        // 此时不绑定日志设备，所有日志静默丢弃（不崩溃），仅给出警告。
        qWarning() << "无法打开日志文件，日志将不写入文件:" << m_logFile.fileName();
        return;
    }
    Utility::tsLogInfo.setDevice(&m_logFile);
    qInstallMessageHandler(Utility::myMessageHandler);
}

// 参考了qt creator的 UI Tour功能，原始代码为 https://github.com/qt-creator/qt-creator/blob/qds/4.5/src/plugins/welcome/introductionwidget.cpp
void MainWindow::onActionHelpTriggered()
{
    auto *wIntroduction = new IntroductionWidget(this);
    wIntroduction->show();
}

void MainWindow::onActionAuthorTriggered()
{
    DlgAuthor dlgAuthor(this);
    dlgAuthor.exec();
}

