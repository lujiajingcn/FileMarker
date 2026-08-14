#include "formfilebrowser.h"
#include "ui_formfilebrowser.h"

#include <QMessageBox>
#include <QDebug>


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
    m_nThread = -1;  // P2-18：初始化为非法值，防止 onRecvStopSearch 提前触发时 switch 走入意外分支

    // 点击左侧【文件导航】中节点，将该节点下的文件夹及文件显示在右侧【文件列表】
    connect(ui->wFileNavigation, &FormFileNavigation::sendNavigationClicked, this, &FormFileBrowser::onTvNavigationClicked);

    // 点击左侧【文件导航】中节点时，无论右侧显示的是【查询结果】界面还是正在【查询过程】界面（触发停止查询操作），都统一跳转到【文件列表】界面
    connect(this, &FormFileBrowser::sendNavigationClicked, ui->pageFiles, &FormPageFiles::onTvNavigationClicked);

    connect(ui->pageFilteredFiles, &FormPageFilterdFiles::sendGotoFile, this, &FormFileBrowser::onGotoFilePath);
    connect(this, &FormFileBrowser::sendGotoFile, ui->pageFiles, &FormPageFiles::recvGotoFile);

    connect(this, &FormFileBrowser::sendShowFilteredFiles, ui->pageFilteredFiles, &FormPageFilterdFiles::onShowFilteredFiles);
    connect(this, &FormFileBrowser::sendRefresh, ui->pageFiles, &FormPageFiles::on_btnRefresh_clicked);
    connect(ui->pageProgressInfo, &FormPageProgressInfo::sendStop, this, &FormFileBrowser::onRecvStopSearch);

    connect(ui->pageFiles, &FormPageFiles::sendLabels, this, &FormFileBrowser::onRecvLabels);
}

FormFileBrowser::~FormFileBrowser()
{
    // P0-3：m_sqlOperation 在构造函数中 new 在堆上，从未释放，导致内存泄漏。
    delete m_sqlOperation;
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
    // 将命中缓存的部分与磁盘查找得到的部分合并，避免部分目录命中缓存时被整体丢弃。
    if(!m_qLFilePathsFromCache.isEmpty())
    {
        qLFilteredFiles = m_qLFilePathsFromCache + qLFilteredFiles;
        m_qLFilePathsFromCache.clear();
    }
    setActionState(SEARCH_FINISH);
    ui->stackedWidget->setCurrentWidget(ui->pageFilteredFiles);
    emit sendShowFilteredFiles(qLFilteredFiles);
    ui->pageProgressInfo->clear();
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

void FormFileBrowser::onActionAddLabelByAITriggered()
{
    setActionState(ADDTAGBYAI_START);
    showProcessPage(RUNNINGTHREAD::AIADDTAG);

    QStringList qLSelDirs = getSelDirs();

    m_threadAddLabelByAI = new ThreadAddTagByAI(this);

    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendProcessInfo, ui->pageProgressInfo, &FormPageProgressInfo::onRecvProgress);
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendFinish, this, &FormFileBrowser::onRecvAddLabelByAIFinish);
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendTags, this, &FormFileBrowser::onRecvLabelsGeneratedByAI);
    // #10：AI 返回的 summary 之前被丢弃，这里记入日志（程序无专门展示 UI）。
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendSummary, this, [](QString s){
        qInfo() << "AI 摘要:" << s;
    });

    // 线程结束后自动释放对象并把指针置空，避免反复创建导致对象泄漏以及悬空指针误用。
    connect(m_threadAddLabelByAI, &QThread::finished, m_threadAddLabelByAI, &QObject::deleteLater);
    connect(m_threadAddLabelByAI, &QThread::finished, this, [this](){ m_threadAddLabelByAI = nullptr; });

    m_threadAddLabelByAI->setDirs(qLSelDirs);

    m_threadAddLabelByAI->start();
}

void FormFileBrowser::onRecvAddLabelByAIFinish()
{
    showFilesWidget();
    setActionState(ADDTAGBYAI_FINISH);
    ui->pageProgressInfo->clear();
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
            ADSOperation::writeADSFile(sFilePath, sSelLabel, "");
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
    m_qLFilePathsFromCache.clear();   // 清除上一次查找遗留的缓存命中结果

    QStringList qLSelDirs = getSelDirs();
    if(qLSelDirs.count() == 0){
        QString sCurDir = ui->pageFiles->getCurDir();
        qLSelDirs.append(sCurDir);
    }

    if(m_qLSelLabels.count() == 0){
        QMessageBox::information(this, "提示", "未选中标签！");
        return;
    }

    SearchConfig sc = ConfigOperation::readSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE);

    if(sc.bFromCache){
        QStringList qLFilePaths, qLNotHitedDirs;
        m_sqlOperation->searchFilesByLabels(qLSelDirs, m_qLSelLabels, sc.logic, qLFilePaths, qLNotHitedDirs);
        if(qLNotHitedDirs.count() == 0) { // 查找的文件夹全部在缓存数据库中存在，则显示查找结果后结束。
            showFilteredFile(qLFilePaths);
            return;
        } else { // 查找的文件夹有部分未在缓存数据库中，不在的文件夹从磁盘查找
            // 命中缓存的部分先保留，最后与磁盘查找结果合并，避免被整体丢弃。
            m_qLFilePathsFromCache = qLFilePaths;
            qLSelDirs = qLNotHitedDirs;
        }
    }

    // P2-15：若前一个搜索线程仍在运行，直接返回。正常流程下搜索按钮运行期间已被禁用，
    // 此检查仅为防御性编程，避免旧线程的 finished lambda（m_threadSearch = nullptr）
    // 误清新线程指针，以及重复启动线程。
    if (m_threadSearch && m_threadSearch->isRunning()) {
        return;
    }

    setActionState(SEARCH_START);

    showProcessPage(RUNNINGTHREAD::SEARCHFILE);// 从磁盘查找比较耗时，切换到处理进度页面
    m_threadSearch = new ThreadSearch(this);
    m_threadSearch->setPara(qLSelDirs, m_qLSelLabels, sc.logic);
    connect(m_threadSearch, &ThreadSearch::sigResult, this, &FormFileBrowser::showFilteredFile);
    connect(m_threadSearch, &ThreadSearch::sendProcessInfo, ui->pageProgressInfo, &FormPageProgressInfo::onRecvProgress);
    // 注意：ThreadSearch 构造函数里已连接 finished->deleteLater，这里只需在线程结束后把指针置空，避免悬空指针。
    connect(m_threadSearch, &QThread::finished, this, [this](){ m_threadSearch = nullptr; });
    m_threadSearch->start();
}

/** 遍历选中的文件夹，
 * 将所有带有ads的文件和对应的ads名称记录到缓存数据库中
 * 将选中的文件夹也记录到缓存数据库中，用于后续作为查找条件
 */
void FormFileBrowser::onActionTraverseSelDirsTriggered()
{
    setActionState(TRAVERSER_START);

    QStringList selDirs = getSelDirs();
    if(selDirs.count() == 0){
        QString curDir = ui->pageFiles->getCurDir();
        selDirs.append(curDir);
    }

    showProcessPage(RUNNINGTHREAD::TRAVERSEDIR);

    m_threadTraverseDirs = new ThreadTraverseDirs(this);
    m_threadTraverseDirs->setSelDirs(selDirs);
    qRegisterMetaType<QMap<QString, QMap<QString, QStringList>>>("QMap<QString, QMap<QString, QStringList>>");
    connect(m_threadTraverseDirs, &ThreadTraverseDirs::sendResult, this, &FormFileBrowser::onRecvTraverseResult);
    connect(this, &FormFileBrowser::sendStop, m_threadTraverseDirs, &ThreadTraverseDirs::stopThread);

    connect(m_threadTraverseDirs, &ThreadTraverseDirs::sendProcessInfo, ui->pageProgressInfo, &FormPageProgressInfo::onRecvProgress);
    // 注意：ThreadTraverseDirs 构造函数里已连接 finished->deleteLater，这里只需在线程结束后把指针置空，避免悬空指针。
    connect(m_threadTraverseDirs, &QThread::finished, this, [this](){ m_threadTraverseDirs = nullptr; });

    m_threadTraverseDirs->start();
}

void FormFileBrowser::onRecvTraverseResult(QMap<QString, FILE_TAGS> dirAndFileTags, QMap<QString, QSet<QString>> dirAndTags)
{
    // 注意：insertRecord 内部已经按目录做“先删后插”，因此这里不能再整体清空
    // filewithlabels 表，否则之前遍历过的其它目录的缓存会被全部抹掉，
    // 导致“命中缓存”的优化几乎永远不生效。
    m_sqlOperation->insertRecord(dirAndFileTags);
    showFilesWidget();
    setActionState(TRAVERER_FINISH);
    emit sendDirAndLabels(dirAndTags);
    ui->pageProgressInfo->clear();
}

void FormFileBrowser::onActionSearchConfigTriggered()
{
    DlgSearchConfig dlg(this, g_sAppDir + "/" + SEARCH_CONFIG_FILE);
    dlg.exec();
}

void FormFileBrowser::onRecvStopSearch()
{
    switch (m_nThread)
    {
    case SEARCHFILE:
        if (m_threadSearch) m_threadSearch->stopThread();
        break;
    case AIADDTAG:
        if (m_threadAddLabelByAI) m_threadAddLabelByAI->stopThread();
        break;
    case TRAVERSEDIR:
        if (m_threadTraverseDirs) m_threadTraverseDirs->stopThread();
        break;
    default:
        break;
    }
}

void FormFileBrowser::onRecvLabels(QString sFilePath, QString sLabels)
{
    // P2-5：内联编辑/弹框编辑标签时，只作用于被编辑的那一个文件，
    // 不再误覆盖当前选中的其它文件（批量打标签请用工具栏“添加标签”按钮）。
    if(sFilePath.isEmpty())
    {
        QMessageBox::information(this, "提示", "未选中文件！");
        return;
    }

    QStringList qLLabels = sLabels.split(",");
    ADSOperation::deleteADSFiles(sFilePath);
    foreach(const QString &sSelLabel, qLLabels)
    {
        ADSOperation::writeADSFile(sFilePath, sSelLabel, "");
    }

    emit sendRefresh();

    if(!sLabels.isEmpty())
        emit sendLabels(sLabels);   // 转发给【标签管理】面板
}

// 设置菜单和工具栏中动作的状态（可用/不可用）
void FormFileBrowser::setActionState(OPERATION op)
{
    switch (op) {
    case ADDTAGBYAI_START:
    case SEARCH_START:
    case TRAVERSER_START:
        // P1-9：QAction 指针可能为 null（setActions 传入 null 时 connect 静默失败），需判空。
        if (m_aAddLabelByAI) m_aAddLabelByAI->setEnabled(false);
        if (m_aSearch) m_aSearch->setEnabled(false);
        if (m_aTraverse) m_aTraverse->setEnabled(false);
        break;
    case ADDTAGBYAI_FINISH:
    case SEARCH_FINISH:
    case TRAVERER_FINISH:
        if (m_aAddLabelByAI) m_aAddLabelByAI->setEnabled(true);
        if (m_aSearch) m_aSearch->setEnabled(true);
        if (m_aTraverse) m_aTraverse->setEnabled(true);
        break;
    }
}
