#include "filebrowser.h"
#include "ui_filebrowser.h"
#include <QMenu>
#include <QDebug>
#include <QTimer>

FileBrowser::FileBrowser(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FileBrowser)
{
    ui->setupUi(this);

    setWindowStyleSheet();

    // 点击左侧导航栏中节点，将该节点下的文件夹及文件显示在右侧文件列表
    connect(ui->wFileNavigation, &FormFileNavigation::sendNavigationClicked, this, &FileBrowser::onTvNavigationClicked);

    // 点击左侧导航栏中节点时，无论右侧是显示的根据标签查询后的结果界面还是正在查询的界面（触发停止查询操作），都统一跳转到文件列表界面
    connect(this, &FileBrowser::sendNavigationClicked, ui->pageFiles, &FormPageFiles::onTvNavigationClicked);

    connect(this, &FileBrowser::sendGotoFile, ui->pageFiles, &FormPageFiles::recvGotoFile);

    connect(this, &FileBrowser::sendProgress, ui->pageProgressInfo, &FormPageProgressInfo::onRecvProgress);
    connect(this, &FileBrowser::sendShowFiltedFiles, ui->pageFilteredFiles, &FormPageFilterdFiles::onShowFiltedFiles);
    connect(ui->pageFilteredFiles, &FormPageFilterdFiles::sendGotoFile, this, &FileBrowser::onGotoFilePath);
    connect(this, &FileBrowser::sendRefresh, ui->pageFiles, &FormPageFiles::on_btnRefresh_clicked);
}

FileBrowser::~FileBrowser()
{
    delete ui;
}

void FileBrowser::setWindowStyleSheet()
{
    QList<int> sizes;
    // 在保持比例的情况下，绝对值要尽量大
    sizes << 10000 << 40000;
    ui->splitter->setSizes(sizes);

    // 设置初始窗口大小
    resize(1500,800);

    // 初始显示文件列表
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}

void FileBrowser::setAction(QAction *action)
{
    ui->pageFiles->setAction(action);
}

void FileBrowser::onGotoFilePath(QString sFilePath)
{
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
    emit sendGotoFile(sFilePath);
}

QStringList FileBrowser::getSelFilePath()
{
    return ui->pageFiles->getSelFilePath();
}

QStringList FileBrowser::getSelDirs()
{
    return ui->pageFiles->getSelDirs();
}

void FileBrowser::onTvNavigationClicked(const QModelIndex &index)
{
    if(ui->stackedWidget->currentWidget() == ui->pageProgressInfo)
    {
        emit sendStop();
        return;
    }

    emit sendNavigationClicked(index);

    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}


void FileBrowser::showProcessPage()
{
    ui->stackedWidget->setCurrentWidget(ui->pageProgressInfo);
}

void FileBrowser::showFilesWidget()
{
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}

void FileBrowser::showFilteredFile(QStringList qLFilteredFiles)
{
    ui->stackedWidget->setCurrentWidget(ui->pageFilteredFiles);
    emit sendShowFiltedFiles(qLFilteredFiles);
}

void FileBrowser::recvProcessInfo(QString sProcessedFilePath)
{
    emit sendProgress(sProcessedFilePath);
}


void FileBrowser::on_btnStop_clicked()
{
    emit sendStop();
}

void FileBrowser::onRecvRefresh()
{
    emit sendRefresh();
}

