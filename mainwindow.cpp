#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "dlgauthor.h"
#include "utility.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    g_sAppDir = QCoreApplication::applicationDirPath();

    ui->setupUi(this);

    initLogMessageHanlder();

    // 将【标签管理】窗口的工具栏操作添加到主窗口的菜单中
    ui->dwcLabels->addToolButtonsToMenu(ui->menuLabelManage);

    // 将菜单栏中的动作放到【文件管理】中实现
    ui->wFileBrowser->setActions(ui->actionAddLabels, ui->actionDelLabels, ui->actionSearchFilesbyLabels, ui->actionTraverseSelDirs, ui->actionSearchConfig);

    // 【标签管理】中选中的标签，同步到【文件管理】中
    connect(ui->dwcLabels, &FormLabels::sendSelLabels, ui->wFileBrowser, &FormFileBrowser::onRecvSelLabels);

    // 【当前目录标签】中选中的标签，同步到【文件管理类】中
    connect(ui->dwcCurDirLabels, &FormCurDirLabels::sendSelLabels, ui->wFileBrowser, &FormFileBrowser::onRecvSelLabels);

    // 遍历选中文件夹，检索出其中文件的所有标签，显示在【当前目录标签】
    connect(ui->wFileBrowser, &FormFileBrowser::sendDirAndLabels, ui->dwcCurDirLabels, &FormCurDirLabels::onRecvDirAndLabels);

    // 手动编辑【标签】列内容后，将编辑后的内容传递给【标签管理】
    // 编辑后的标签由英文逗号分隔，形如 照片,故宫,2025
    // todo 传递路径过长，应该是结构不合理，后续改进
    connect(ui->wFileBrowser, &FormFileBrowser::sendLabels, ui->dwcLabels, &FormLabels::onRecvLabels);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initLogMessageHanlder()
{
    m_logFile.setFileName(g_sAppDir + "/" + LOG_FILE);
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
    Utility::tsLogInfo.setDevice(&m_logFile);
    qInstallMessageHandler(Utility::myMessageHandler);
}

// 参考了qt creator的 UI Tour功能，原始代码为 https://github.com/qt-creator/qt-creator/blob/qds/4.5/src/plugins/welcome/introductionwidget.cpp
void MainWindow::on_actionHelp_triggered()
{
    auto *wIntroduction = new IntroductionWidget(this);
    wIntroduction->show();
}

void MainWindow::on_actionAuthor_triggered()
{
    DlgAuthor dlgAuthor(this);
    dlgAuthor.exec();
}

