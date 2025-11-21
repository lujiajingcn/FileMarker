#include "formfilebrowser.h"
#include "ui_formfilebrowser.h"

#include <QMessageBox>


#include "adsoperation.h"
#include "configoperation.h"
#include "dlgsearchconfig.h"

FormFileBrowser::FormFileBrowser(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormFileBrowser)
{
    ui->setupUi(this);

    setWindowStyleSheet();

    m_sqlOperation = new SqliteOperation;
    m_threadSearch = nullptr;
    m_threadTraverseDirs = nullptr;
    m_threadAddLabelByAI = nullptr;

    // 点击左侧【文件导航】中节点，将该节点下的文件夹及文件显示在右侧【文件列表】
    connect(ui->wFileNavigation, &FormFileNavigation::sendNavigationClicked, this, &FormFileBrowser::onTvNavigationClicked);

    // 点击左侧【文件导航】中节点时，无论右侧显示的是【查询结果】界面还是正在【查询过程】界面（触发停止查询操作），都统一跳转到【文件列表】界面
    connect(this, &FormFileBrowser::sendNavigationClicked, ui->pageFiles, &FormPageFiles::onTvNavigationClicked);

    connect(ui->pageFilteredFiles, &FormPageFilterdFiles::sendGotoFile, this, &FormFileBrowser::onGotoFilePath);
    connect(this, &FormFileBrowser::sendGotoFile, ui->pageFiles, &FormPageFiles::recvGotoFile);

    connect(this, &FormFileBrowser::sendProgress, ui->pageProgressInfo, &FormPageProgressInfo::onRecvProgress);
    connect(this, &FormFileBrowser::sendShowFilteredFiles, ui->pageFilteredFiles, &FormPageFilterdFiles::onShowFilteredFiles);
    connect(this, &FormFileBrowser::sendRefresh, ui->pageFiles, &FormPageFiles::on_btnRefresh_clicked);
    connect(ui->pageProgressInfo, &FormPageProgressInfo::sendStop, this, &FormFileBrowser::onRecvStopSearch);

    connect(ui->pageFiles, &FormPageFiles::sendLabels, this, &FormFileBrowser::onRecvLabels);
}

FormFileBrowser::~FormFileBrowser()
{
    delete ui;
}

void FormFileBrowser::setWindowStyleSheet()
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

void FormFileBrowser::setActions(QAction *aAddLabelByAI, QAction *aAddLabel, QAction *aDelLabel, QAction *aSearch, QAction *aTraverse, QAction *aSearchConfig)
{
    m_aAddLabelByAI = aAddLabelByAI;
    m_aAddLabel = aAddLabel;
    m_aDelLabel = aDelLabel;
    m_aSearch = aSearch;
    m_aTraverse = aTraverse;
    m_aSearchConfig = aSearchConfig;

    connect(m_aAddLabelByAI, &QAction::triggered, this, &FormFileBrowser::onActionAddLabelByAITriggered);
    connect(m_aAddLabel, &QAction::triggered, this, &FormFileBrowser::onActionAddLabelsTriggered);
    connect(m_aSearch, &QAction::triggered, this, &FormFileBrowser::onActionSearchFilesbyLabelsTriggered);
    connect(m_aTraverse, &QAction::triggered, this, &FormFileBrowser::onActionTraverseSelDirsTriggered);
    connect(m_aSearchConfig, &QAction::triggered, this, &FormFileBrowser::onActionSearchConfigTriggered);

    ui->pageFiles->setAction(aDelLabel);
}

QStringList FormFileBrowser::getSelFilePath()
{
    return ui->pageFiles->getSelFilePath();
}

QStringList FormFileBrowser::getSelDirs()
{
    return ui->pageFiles->getSelDirs();
}

void FormFileBrowser::showProcessPage(RUNNINGTHREAD rt)
{
    ui->stackedWidget->setCurrentWidget(ui->pageProgressInfo);
    m_nThread = rt;
}

void FormFileBrowser::showFilesWidget()
{
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}

void FormFileBrowser::showFilteredFile(QStringList qLFilteredFiles)
{
    ui->stackedWidget->setCurrentWidget(ui->pageFilteredFiles);
    emit sendShowFilteredFiles(qLFilteredFiles);
}

void FormFileBrowser::onGotoFilePath(QString sFilePath)
{
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
    emit sendGotoFile(sFilePath);
}

void FormFileBrowser::onTvNavigationClicked(const QModelIndex &index)
{
    if(ui->stackedWidget->currentWidget() == ui->pageProgressInfo)
    {
        emit sendStop();
        return;
    }

    emit sendNavigationClicked(index);

    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}

void FormFileBrowser::onRecvSelLabels(QStringList qLSelLabels)
{
    m_qLSelLabels = qLSelLabels;
}

void FormFileBrowser::recvProcessInfo(QString sProcessedFilePath)
{
    emit sendProgress(sProcessedFilePath);
}

void FormFileBrowser::on_btnStop_clicked()
{
    emit sendStop();
}

void FormFileBrowser::onActionAddLabelByAITriggered()
{
    showProcessPage(RUNNINGTHREAD::AIADDTAG);

    QStringList qLSelDirs = getSelDirs();

    m_threadAddLabelByAI = new ThreadAddLabelByAI(this);

    connect(m_threadAddLabelByAI, &ThreadAddLabelByAI::sendProcessInfo, this, &FormFileBrowser::recvProcessInfo);
    connect(m_threadAddLabelByAI, &ThreadAddLabelByAI::sendFinish, this, &FormFileBrowser::onRecvAddLabelByAIFinish);
    connect(m_threadAddLabelByAI, &ThreadAddLabelByAI::sendLabels, this, &FormFileBrowser::onRecvLabelsGeneratedByAI);

    m_threadAddLabelByAI->setDirs(qLSelDirs);

    m_threadAddLabelByAI->start();
}

void FormFileBrowser::onRecvAddLabelByAIFinish()
{
    showFilesWidget();
}

void FormFileBrowser::onRecvLabelsGeneratedByAI(QString sLabels)
{
    emit sendLabelsGeneratedByAI(sLabels);
}

/**
 * 使用标签名作为ads文件名，创建一系列文件，形如
 * filepath1:label1
 * filepath1:label2
 * filepath2:label3
 * todo: 后续可以在ads文件中写入一些内容，比如标签的简介
*/
void FormFileBrowser::onActionAddLabelsTriggered()
{
    QStringList qLFilePaths = getSelFilePath();
    if(qLFilePaths.count() == 0)
    {
        QMessageBox::information(this, "提示", "未选中文件或文件夹！");
        return;
    }

    if(m_qLSelLabels.count() == 0)
    {
        QMessageBox::information(this, "提示", "未选中标签！");
        return;
    }

    foreach(const QString &sFilePath, qLFilePaths)
    {
        foreach(const QString &sSelLabel, m_qLSelLabels)
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
void FormFileBrowser::onActionSearchFilesbyLabelsTriggered()
{
    QStringList qLSelDirs = getSelDirs();
    if(qLSelDirs.count() == 0)
    {
        QString sCurDir = ui->pageFiles->getCurDir();
        qLSelDirs.append(sCurDir);
    }

    if(m_qLSelLabels.count() == 0)
    {
        QMessageBox::information(this, "提示", "未选中标签！");
        return;
    }

    SearchConfig sc = ConfigOperation::readSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE);
    int nLabelLogic = labelLogic::AND;

    if(sc.bFromCache)
    {
        QStringList qLFilePaths, qLNotHitedDirs;
        m_sqlOperation->searchFilesByLabels(qLSelDirs, m_qLSelLabels, sc.logic, qLFilePaths, qLNotHitedDirs);
        if(qLNotHitedDirs.count() == 0) // 查找的文件夹全部在缓存数据库中存在，则显示查找结果后结束。
        {
            showFilteredFile(qLFilePaths);
            return;
        }
        else // 查找的文件夹有部分未在缓存数据库中，不在的文件夹从磁盘查找
        {
            qLSelDirs = qLNotHitedDirs;
        }
    }

    showProcessPage(RUNNINGTHREAD::SEARCHFILE);// 从磁盘查找比较耗时，切换到处理进度页面
    m_threadSearch = new ThreadSearch(this); // todo 每次都要创建吗
    m_threadSearch->setPara(qLSelDirs, m_qLSelLabels, nLabelLogic);
    connect(m_threadSearch, &ThreadSearch::sigResult, this, &FormFileBrowser::showFilteredFile);
    connect(m_threadSearch, &ThreadSearch::sendProcessInfo, this, &FormFileBrowser::recvProcessInfo);
    m_threadSearch->start();
}

/** 遍历选中的文件夹，
 * 将所有带有ads的文件和对应的ads名称记录到缓存数据库中
 * 将选中的文件夹也记录到缓存数据库中，用于后续作为查找条件
 */
void FormFileBrowser::onActionTraverseSelDirsTriggered()
{
    QStringList qLSelDirs = getSelDirs();
    if(qLSelDirs.count() == 0)
    {
        QString sCurDir = ui->pageFiles->getCurDir();
        qLSelDirs.append(sCurDir);
    }
    showProcessPage(RUNNINGTHREAD::TRAVERSEDIR);
    m_threadTraverseDirs = new ThreadTraverseDirs(this);
    m_threadTraverseDirs->setSelDirs(qLSelDirs);
    qRegisterMetaType<QMap<QString, QMap<QString, QStringList>>>("QMap<QString, QMap<QString, QStringList>>");
    connect(m_threadTraverseDirs, &ThreadTraverseDirs::sigResult, this, &FormFileBrowser::onRecvTraverseResult);
    connect(this, &FormFileBrowser::sendStop, m_threadTraverseDirs, &ThreadTraverseDirs::recvStop);
    connect(m_threadTraverseDirs, &ThreadTraverseDirs::sendDirAndLabel, this, &FormFileBrowser::onRecvDirAndLabels);

    connect(m_threadTraverseDirs, &ThreadTraverseDirs::sendProcessInfo, this, &FormFileBrowser::recvProcessInfo);
    m_threadTraverseDirs->start();
}

void FormFileBrowser::onRecvTraverseResult(QMap<QString, QMap<QString, QStringList>> mapDirAndmapHostFilesAndLabel)
{
    m_sqlOperation->clearTable(TABLE_NAME_FILEPATH_LABEL);
    m_sqlOperation->insertRecord(mapDirAndmapHostFilesAndLabel);
    showFilesWidget();
}

void FormFileBrowser::onActionSearchConfigTriggered()
{
    DlgSearchConfig dlg(this, g_sAppDir + "/" + SEARCH_CONFIG_FILE);
    dlg.exec();
}

void FormFileBrowser::onRecvDirAndLabels(QMap<QString, QSet<QString>> mapDirAndLabel)
{
    emit sendDirAndLabels(mapDirAndLabel);
}

void FormFileBrowser::onRecvStopSearch()
{
    switch (m_nThread)
    {
    case SEARCHFILE:
        m_threadSearch->stopThread();
        break;
    case AIADDTAG:
        m_threadAddLabelByAI->stopThread();
        break;
    case TRAVERSEDIR:
        m_threadTraverseDirs->recvStop();
        break;
    default:
        break;
    }
}

void FormFileBrowser::onRecvLabels(QString sLabels)
{
    QStringList qLFilePaths = getSelFilePath();
    if(qLFilePaths.count() == 0)
    {
        QMessageBox::information(this, "提示", "未选中文件或文件夹！");
        return;
    }

    QStringList qLLabels = sLabels.split(",");
    foreach(const QString &sFilePath, qLFilePaths)
    {
        ADSOperation::deleteADS(sFilePath);
        foreach(const QString &sSelLabel, qLLabels)
        {
            ADSOperation::writeADS(sFilePath, sSelLabel, "");
        }
    }

    emit sendRefresh();

    if(!sLabels.isEmpty())
        emit sendLabels(sLabels);
}
