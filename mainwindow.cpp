#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QSettings>
#include <QTextCodec>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

#include "adsoperation.h"
#include "common.h"
#include "dlgauthor.h"

static QTextStream tsLogInfo;
static QMutex mutex;

void myMessageHandler(QtMsgType mType, const QMessageLogContext&, const QString& sMsg)
{
    mutex.lock();
    QString sFormatCurTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    switch (mType) {
    case QtDebugMsg:
        tsLogInfo << QString("%1    Debug: %2").arg(sFormatCurTime).arg(sMsg) << endl;
        break;
    case QtInfoMsg:
        tsLogInfo << QString("%1    Info: %2").arg(sFormatCurTime).arg(sMsg) << endl;
        break;
    case QtWarningMsg:
        tsLogInfo << QString("%1    Warning: %2").arg(sFormatCurTime).arg(sMsg) << endl;
        break;
    case QtCriticalMsg:
        tsLogInfo << QString("%1    Critical: %2").arg(sFormatCurTime).arg(sMsg) << endl;
        break;
    case QtFatalMsg:
        tsLogInfo << QString("%1    Fatal: %2").arg(sFormatCurTime).arg(sMsg) << endl;
        abort();
    }
    mutex.unlock();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    g_sAppDir = QCoreApplication::applicationDirPath();

    initLogMessageHanlder();

    m_sqlOperation = new SqliteOperation;

    m_fileBrowser = ui->wFileBrowser;
    m_fileBrowser->setAction(ui->actionDelLabels);
    connect(m_fileBrowser, &FileBrowser::sendStop, this, &MainWindow::recvStop);
    connect(this, &MainWindow::sendRefresh, m_fileBrowser, &FileBrowser::on_btnRefresh_clicked);

    m_formLabels = new FormLabels(this);
    m_formLabels->addToolButtonsToMenu(ui->menuLabelManage);
    ui->dwLabels->setWidget(m_formLabels);

    m_threadSearch = nullptr;
    m_threadTraverseDirs = nullptr;

    readSearchConfig();
}

void MainWindow::initLogMessageHanlder()
{
    m_logFile.setFileName(g_sAppDir + "/" + LOG_FILE);
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
    tsLogInfo.setDevice(&m_logFile);
    qInstallMessageHandler(myMessageHandler);
}

void MainWindow::readSearchConfig()
{
    QString sConfigFilePath = g_sAppDir + "/" + SEARCH_CONFIG_FILE;
    QDir dir(sConfigFilePath);
    if(!dir.exists())
    {
        qWarning().noquote()<<QString("配置文件%1不存在！").arg(SEARCH_CONFIG_FILE);
        return;
    }
    QSettings settings(sConfigFilePath, QSettings::Format::IniFormat);
    if(settings.status() != QSettings::NoError)
    {
        qWarning().noquote()<<QString("打开配置文件%1失败！").arg(SEARCH_CONFIG_FILE);
        return;
    }
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
    bool bIsSearchFormCache = settings.value("searchfromcache").toBool();
    if(bIsSearchFormCache)
    {
        ui->actionFromCache->setChecked(true);
        ui->actionFromDisk->setChecked(false);
    }
    else
    {
        ui->actionFromCache->setChecked(false);
        ui->actionFromDisk->setChecked(true);
    }
    int nLabelLogic = settings.value("labellogic").toInt();
    if(nLabelLogic == labelLogic::AND)
    {
        ui->actionLabelLogicAnd->setChecked(true);
        ui->actionLabelLogicOr->setChecked(false);
    }
    else if(nLabelLogic == labelLogic::OR)
    {
        ui->actionLabelLogicAnd->setChecked(false);
        ui->actionLabelLogicOr->setChecked(true);
    }
}

void MainWindow::writeSearchConfig()
{
    QSettings settings(g_sAppDir + "/" + SEARCH_CONFIG_FILE, QSettings::Format::IniFormat);
    if(settings.status() != QSettings::NoError)
    {
        qWarning().noquote()<<QString("写入配置文件%1失败").arg(SEARCH_CONFIG_FILE);
        return;
    }
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
    settings.setValue("searchfromcache", ui->actionFromCache->isChecked());
    int nLabelLogic = labelLogic::AND;
    if(ui->actionLabelLogicOr->isChecked())
    {
        nLabelLogic = labelLogic::OR;
    }
    settings.setValue("labellogic", nLabelLogic);
}

void MainWindow::recvStop()
{
    m_threadTraverseDirs->recvStop();
}

MainWindow::~MainWindow()
{
    writeSearchConfig();
    delete ui;
}

/**
 * 使用标签名作为ads文件名，创建一系列文件，形如
 * filepath1:label1
 * filepath1:label2
 * filepath2:label3
 * todo: 后续可以在ads文件中写入一些内容，比如标签的简介
*/
void MainWindow::on_actionAddLabels_triggered()
{
    QStringList qLFilePaths = m_fileBrowser->getSelFilePath();
    if(qLFilePaths.count() == 0)
    {
        QMessageBox::information(this, "提示", "未选中文件或文件夹！");
        return;
    }
    QStringList qLSelLabels = m_formLabels->getSelLabels();
    if(qLSelLabels.count() == 0)
    {
        QMessageBox::information(this, "提示", "未选中标签！");
        return;
    }

    foreach(const QString &sFilePath, qLFilePaths)
    {
        foreach(const QString &sSelLabel, qLSelLabels)
        {
            ADSOperation::writeADS(sFilePath, sSelLabel, "");
        }
    }

    emit sendRefresh();
}

/**
 * 根据给定的标签查找具有这些标签的文件。
 * 根据配置信息，选择从缓存数据库中查找还是直接从磁盘查找。
 * 从缓存数据库中查找速度会很快，适用于上次标记完文件后，文件夹中文件未发生变动的情况。
 * 如果上次标记完以后移动或删除文件，则可能查找不准确。
 * 选中多个文件夹时，有可能有些文件夹在缓存数据库中没有信息，这些文件夹直接从磁盘查找
*/
void MainWindow::on_actionSearchFilesbyLabels_triggered()
{
    QStringList qLSelDirs = m_fileBrowser->getSelDirs();
    if(qLSelDirs.count() == 0)
    {
        QMessageBox::information(this, "提示", "未选中文件夹！");
        return;
    }

    QStringList qLSelLabels = m_formLabels->getSelLabels();
    if(qLSelLabels.count() == 0)
    {
        QMessageBox::information(this, "提示", "未选中标签！");
        return;
    }

    int nLabelLogic = labelLogic::AND;
    if(ui->actionLabelLogicOr->isChecked())
    {
        nLabelLogic = labelLogic::OR;
    }

    if(ui->actionFromCache->isChecked())
    {
        QStringList qLFilePaths, qLNotHitedDirs;
        m_sqlOperation->searchFilesByLabels(qLSelDirs, qLSelLabels, nLabelLogic, qLFilePaths, qLNotHitedDirs);
        if(qLNotHitedDirs.count() == 0) // 查找的文件夹全部在缓存数据库中存在，则显示查找结果后结束。
        {
            m_fileBrowser->showFilteredFile(qLFilePaths);
            return;
        }
        else // 查找的文件夹有部分未在缓存数据库中，不在的文件夹从磁盘查找
        {
            qLSelDirs = qLNotHitedDirs;
        }
    }

    m_fileBrowser->showProcessPage();// 从磁盘查找比较耗时，切换到处理进度页面
    m_threadSearch = new ThreadSearch(this);
    m_threadSearch->setPara(qLSelDirs, qLSelLabels, nLabelLogic);
    connect(m_threadSearch, &ThreadSearch::sigResult, m_fileBrowser, &FileBrowser::showFilteredFile);
    connect(m_threadSearch, &ThreadSearch::sendProcessInfo, m_fileBrowser, &FileBrowser::recvProcessInfo);
    m_threadSearch->start();
}

/** 遍历选中的文件夹，
 * 将所有带有ads的文件和对应的ads名称记录到缓存数据库中
 * 将选中的文件夹也记录到缓存数据库中，用于后续作为查找条件
 */
void MainWindow::on_actionTraverseSelDirs_triggered()
{
    QStringList qLSelDirs = m_fileBrowser->getSelDirs();
    if(qLSelDirs.count() == 0)
    {
        QMessageBox::information(this, "提示", "未选中文件夹！");
        return;
    }
    m_fileBrowser->showProcessPage();
    m_threadTraverseDirs = new ThreadTraverseDirs(this);
    m_threadTraverseDirs->setSelDirs(qLSelDirs);
    qRegisterMetaType<QMap<QString, QMap<QString, QStringList>>>("QMap<QString, QMap<QString, QStringList>>");
    connect(m_threadTraverseDirs, &ThreadTraverseDirs::sigResult, this, &MainWindow::onRecvTraverseResult);
    connect(m_threadTraverseDirs, &ThreadTraverseDirs::sendProcessInfo, m_fileBrowser, &FileBrowser::recvProcessInfo);
    m_threadTraverseDirs->start();
}

void MainWindow::onRecvTraverseResult(QMap<QString, QMap<QString, QStringList>> mapDirAndmapHostFilesAndLabel)
{
    m_sqlOperation->clearTable(TABLE_NAME_FILEPATH_LABEL);
    m_sqlOperation->insertRecord(mapDirAndmapHostFilesAndLabel);
    m_fileBrowser->showFilesWidget();
}

//https://github.com/qt-creator/qt-creator/blob/master/src/plugins/welcome/introductionwidget.cpp
void MainWindow::on_actionHelp_triggered()
{
    w = new IntroductionWidget(this);
    w->show();
}

void MainWindow::on_actionFromCache_changed()
{
    if(ui->actionFromCache->isChecked())
    {
        ui->actionFromDisk->setChecked(false);
    }
    else
    {
        ui->actionFromDisk->setChecked(true);
    }
}

void MainWindow::on_actionFromDisk_changed()
{
    if(ui->actionFromDisk->isChecked())
    {
        ui->actionFromCache->setChecked(false);
    }
    else
    {
        ui->actionFromCache->setChecked(true);
    }
}

void MainWindow::on_actionLabelLogicAnd_changed()
{
    if(ui->actionLabelLogicAnd->isChecked())
    {
        ui->actionLabelLogicOr->setChecked(false);
    }
    else
    {
        ui->actionLabelLogicOr->setChecked(true);
    }
}

void MainWindow::on_actionLabelLogicOr_changed()
{
    if(ui->actionLabelLogicOr->isChecked())
    {
        ui->actionLabelLogicAnd->setChecked(false);
    }
    else
    {
        ui->actionLabelLogicAnd->setChecked(true);
    }
}

void MainWindow::on_actionAuthor_triggered()
{
    DlgAuthor dlgAuthor(this);
    dlgAuthor.exec();
}
