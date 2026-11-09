#include "formfilebrowser.h"
#include "ui_formfilebrowser.h"

#include <QMessageBox>
#include <QDebug>


#include "adsoperation.h"
#include "configoperation.h"
#include "dlgsearchconfig.h"
#include "utility.h"

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
    // 查询结果页的“返回目录”按钮：切回文件列表页面
    connect(ui->pageFilteredFiles, &FormPageFilterdFiles::sendBackRequested, this, [this]() {
        showFilesWidget();
    });
    connect(this, &FormFileBrowser::sendGotoFile, ui->pageFiles, &FormPageFiles::recvGotoFile);

    connect(this, &FormFileBrowser::sendShowFilteredFiles, ui->pageFilteredFiles, &FormPageFilterdFiles::onShowFilteredFiles);
    connect(this, &FormFileBrowser::sendRefresh, ui->pageFiles, &FormPageFiles::onBtnRefreshClicked);
    connect(ui->pageProgressInfo, &FormPageProgressInfo::sendStop, this, &FormFileBrowser::onRecvStopSearch);

    connect(ui->pageFiles, &FormPageFiles::sendLabels, this, &FormFileBrowser::onRecvLabels);

    // 进入目录后立即把该目录的标签送到【当前目录标签】面板（不再依赖先执行“遍历文件”）。
    connect(ui->pageFiles, &FormPageFiles::sendDirLabels, this,
            [this](const QString &sDir, const QSet<QString> &setLabels) {
                emit sendClearDirLabels();
                if (sDir.isEmpty())
                    return;
                QMap<QString, QSet<QString>> mapDirAndLabels;
                mapDirAndLabels.insert(sDir, setLabels);
                emit sendDirAndLabels(mapDirAndLabels);
            });
    // 页面内的提示（如目录文件过多只统计了前 N 个）转发到主窗口状态栏
    connect(ui->pageFiles, &FormPageFiles::sendHintMessage, this, &FormFileBrowser::sendStatusMessage);
    // 标签写盘后让相关目录的搜索缓存失效，避免按旧标签命中/漏掉新标签
    connect(ui->pageFiles, &FormPageFiles::sendTagsChanged, this,
            [this](const QStringList &qLFilePaths) {
                if (m_sqlOperation == nullptr)
                    return;
                // P1-2：缓存按"遍历根"存，文件可能在根的子孙目录里，
                // 必须按文件反查所有包含它的根目录，不能只删 absolutePath()。
                for (const QString &sFilePath : qLFilePaths)
                    m_sqlOperation->invalidateCacheForFile(sFilePath);
            });

    // 【C16】页面里要改标签之前先广播，这里记住改动前的标签用于撤销
    connect(ui->pageFiles, &FormPageFiles::sendBeforeTagsChanged,
            this, &FormFileBrowser::onBeforeTagsChanged);
}

FormFileBrowser::~FormFileBrowser()
{
    // 退出前先停掉并等待后台线程：QThread 运行中被销毁会触发 Qt 警告/崩溃，
    // 线程还可能继续回调已销毁的 UI 或写 ADS。
    // 先对所有线程发停止信号，让它们并行退出，再逐个等待，避免串行等待累计超时。
    QThread *pSearch = m_threadSearch;
    QThread *pTraverse = m_threadTraverseDirs;
    QThread *pAI = m_threadAddLabelByAI;

    if (m_threadSearch != nullptr)
        m_threadSearch->stopThread();
    if (m_threadTraverseDirs != nullptr)
        m_threadTraverseDirs->stopThread();
    if (m_threadAddLabelByAI != nullptr)
        m_threadAddLabelByAI->stopThread();

    auto waitThread = [](QThread *pThread, const QString &sName) {
        if (pThread == nullptr)
            return;
        if (!pThread->wait(5000)) {
            qWarning() << sName << "未能在 5 秒内退出，退出时将其分离";
            pThread->setParent(nullptr);
        }
    };
    waitThread(pSearch, QStringLiteral("搜索线程"));
    waitThread(pTraverse, QStringLiteral("遍历线程"));
    waitThread(pAI, QStringLiteral("AI 线程"));

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

    // 方案A：把"作用于文件列表"的命令做成页面顶部的命令条（主窗口不再有全局工具栏）。
    // nullptr 表示分隔符。
    ui->pageFiles->setCommandActions(QList<QAction *>()
                                     << aAddLabelByAI << nullptr
                                     << aAddLabel << aDelLabel << nullptr
                                     << aSearch << aTraverse << nullptr
                                     << aSearchConfig);
}

QStringList FormFileBrowser::getSelFilePath()
{
    return ui->pageFiles->getSelFilePath();
}

QStringList FormFileBrowser::getSelDirs()
{
    return ui->pageFiles->getSelDirs();
}

void FormFileBrowser::restoreLastDir()
{
    ui->pageFiles->restoreLastDir();
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
    ui->pageFilteredFiles->setSearchSource(m_sSearchSource);   // 【C12】结果来源
    emit sendShowFilteredFiles(qLFilteredFiles);
    ui->pageProgressInfo->clear();

    qInfo() << "搜索完成: results=" << qLFilteredFiles.size();
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
    // 没有收到"分组"信息时（例如从【当前目录中的标签】选择），每个标签自成一组。
    m_qLLabelGroups.clear();
    for (const QString &sLabel : qLSelLabels) {
        QSet<QString> setGroup;
        setGroup.insert(sLabel);
        m_qLLabelGroups << setGroup;
    }

    // 【C11】两个面板都会走到这里：点一下标签就等于"选中"，要明确告诉用户选中的是什么、
    // 接下来能用来做什么，否则【当前目录中的标签】看起来像只读面板。
    if (qLSelLabels.isEmpty())
        emit sendStatusMessage(tr("已清空标签选择。"));
    else
        emit sendStatusMessage(tr("已选中标签：%1（可用于【添加到选中文件】或【查找文件】）")
                                   .arg(qLSelLabels.join("、")));
}

/** A4：接收【标签库】给出的分组（每组 = 选中标签 + 它的全部子标签）。 */
void FormFileBrowser::onRecvLabelGroups(const QList<QSet<QString>> &qLLabelGroups)
{
    m_qLLabelGroups = qLLabelGroups;
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
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendTagChanged, this,
            [this](QString sFilePath) {
                if (m_sqlOperation != nullptr)
                    m_sqlOperation->invalidateCacheForFile(sFilePath);   // P1-2：按文件反查遍历根
            });

    // 线程结束后自动释放对象并把指针置空，避免反复创建导致对象泄漏以及悬空指针误用。
    connect(m_threadAddLabelByAI, &QThread::finished, m_threadAddLabelByAI, &QObject::deleteLater);
    connect(m_threadAddLabelByAI, &QThread::finished, this, [this](){ m_threadAddLabelByAI = nullptr; });

    m_threadAddLabelByAI->setDirs(qLSelDirs);

    m_threadAddLabelByAI->start();
}

void FormFileBrowser::onRecvAddLabelByAIFinish(int nSucceeded, int nFailed, QString sFirstError)
{
    showFilesWidget();
    setActionState(ADDTAGBYAI_FINISH);
    ui->pageProgressInfo->clear();
    // AI 线程写 ADS 时 FileTimeGuard 保持 mtime 不变，模型不会自动感知标签变化；
    // 这里主动刷新并清空标签缓存，否则标签列会一直显示旧值。
    emit sendRefresh();

    // 【D5 修复】进度页在结束时会被清空，所以失败必须在这里用对话框告诉用户，
    // 否则"脚本缺失/缺依赖/解释器不可用"这类失败对用户完全不可见。
    if (nFailed > 0 || !sFirstError.isEmpty()) {
        QString sMsg;
        if (nFailed > 0) {
            sMsg = tr("AI 自动生成标签未全部成功：成功 %1 个文件，失败 %2 个。")
                       .arg(nSucceeded).arg(nFailed);
        } else {
            sMsg = tr("AI 自动生成标签未能运行。");
        }
        if (!sFirstError.isEmpty())
            sMsg += tr("\n\n原因：\n%1").arg(sFirstError);
        sMsg += tr("\n\n详细信息见程序目录下的 FileMarker.log。");
        QMessageBox::warning(this, tr("AI 自动生成标签"), sMsg);
    } else if (nSucceeded == 0) {
        QMessageBox::information(this, tr("AI 自动生成标签"),
                                 tr("没有处理任何文件。\n\n请确认已选中文件（或目录），"
                                    "并且目录里包含可处理的文件。"));
    }
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
        QMessageBox::information(this, "提示", "请先在右侧【标签库（分类）】里选择要添加的标签。");
        return;
    }

    // 【D20】批量打标签是不可撤销的写盘操作，数量多时先确认一次
    if (qLFilePaths.size() > 20) {
        const QMessageBox::StandardButton btn =
            QMessageBox::question(this, tr("添加标签"),
                                  tr("将为 %1 个文件添加标签【%2】，是否继续？")
                                      .arg(qLFilePaths.size()).arg(m_qLSelLabels.join("、")),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes)
            return;
    }

    onBeforeTagsChanged(qLFilePaths);   // 【C16】先记下改动前的标签，供撤销

    bool bAllOk = true;
    QStringList qLFailedFiles;          // 【D7 修复】把失败的文件名带回给用户
    foreach(const QString &sFilePath, qLFilePaths)
    {
        // 追加而不是覆盖：保留文件已有标签的顺序，新标签按选中顺序追加到末尾
        if (!ADSOperation::appendTags(sFilePath, m_qLSelLabels)) {
            bAllOk = false;
            qLFailedFiles << QFileInfo(sFilePath).fileName();
        }
    }

    // 标签变化后让搜索缓存失效，避免搜索仍按旧标签命中/漏掉新标签
    // P1-2：按文件反查所有包含它的"遍历根"，不能只按父目录删缓存。
    if (m_sqlOperation != nullptr) {
        for (const QString &sFilePath : qLFilePaths)
            m_sqlOperation->invalidateCacheForFile(sFilePath);
    }

    emit sendRefresh();

    if (!bAllOk) {
        // 【D7 修复】以前只给一句通用提示，批量操作时无法知道是哪个文件失败；
        // 现在列出前 5 个失败文件名（完整列表见日志）。
        const QString sList = qLFailedFiles.mid(0, 5).join(QStringLiteral("\n"));
        const QString sMore = qLFailedFiles.size() > 5
                                  ? tr("\n…另有 %1 个文件").arg(qLFailedFiles.size() - 5)
                                  : QString();
        QMessageBox::warning(this, tr("保存标签失败"),
                             tr("以下 %1 个文件的标签未能写入（可能是文件被占用、只读或磁盘空间不足）：\n\n%2%3\n\n"
                                "已有标签不会被删除，请检查后重试。")
                                 .arg(qLFailedFiles.size()).arg(sList).arg(sMore));
    }
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
    // 记录搜索条件（后面缓存未命中的目录会被替换，但界面提示应显示用户原始选择）
    const QStringList qLSearchDirs = qLSelDirs;

    if(m_qLSelLabels.count() == 0){
        QMessageBox::information(this, "提示", "请先在右侧【标签库（分类）】里选择要查找的标签（可多选）。");
        return;
    }

    // A4：搜索条件是"分组"——每组 = 选中的标签 + 它的全部子标签。
    // 因此"搜索父标签"能搜到只打了子标签的文件；多个条件之间仍按设置里的 AND/OR 组合。
    if (m_qLLabelGroups.isEmpty()) {
        for (const QString &sLabel : m_qLSelLabels) {
            QSet<QString> setGroup;
            setGroup.insert(sLabel);
            m_qLLabelGroups << setGroup;
        }
    }

    // 结果页回显条件时把"含子标签"写出来，让用户明确层级是参与匹配的。
    QStringList qLConditionTexts;
    foreach (const QSet<QString> &setGroup, m_qLLabelGroups) {
        QStringList qLNames = QStringList(setGroup.values());
        qLNames.sort();
        // 组里"直接选中的那个标签"作为显示名，其余当子标签列出来
        QString sRoot;
        for (const QString &sName : qLNames) {
            if (m_qLSelLabels.contains(sName)) {
                sRoot = sName;
                break;
            }
        }
        if (sRoot.isEmpty() || qLNames.size() <= 1) {
            qLConditionTexts << qLNames.join(QStringLiteral("/"));
            continue;
        }
        QStringList qLChildren = qLNames;
        qLChildren.removeAll(sRoot);
        qLConditionTexts << tr("%1（含子标签：%2）").arg(sRoot, qLChildren.join("、"));
    }

    ui->pageFilteredFiles->setSearchCondition(
        tr("标签：%1；目录：%2").arg(qLConditionTexts.join("、"), qLSearchDirs.join("；")));

    // 诊断日志：如果以后再次出现搜索界面卡住，可从日志判断搜索范围与是否走了缓存。
    qInfo() << "开始搜索: dirs=" << qLSelDirs << "labels=" << m_qLSelLabels
            << "useCache=" << ConfigOperation::readSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE).bFromCache;

    SearchConfig sc = ConfigOperation::readSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE);

    if(sc.bFromCache){
        QStringList qLFilePaths, qLNotHitedDirs;
        m_sqlOperation->searchFilesByLabels(qLSelDirs, m_qLLabelGroups, sc.logic, qLFilePaths, qLNotHitedDirs);
        qInfo() << "[搜索] 缓存命中文件=" << qLFilePaths.size() << " 未命中目录数=" << qLNotHitedDirs.size()
                << " 分组数=" << m_qLLabelGroups.size() << " logic=" << int(sc.logic);
        if(qLNotHitedDirs.count() == 0) { // 查找的文件夹全部在缓存数据库中存在，则显示查找结果后结束。
            m_sSearchSource = tr("（结果来自缓存）");   // 【C12】
            showFilteredFile(qLFilePaths);
            return;
        } else { // 查找的文件夹有部分未在缓存数据库中，不在的文件夹从磁盘查找
            // 命中缓存的部分先保留，最后与磁盘查找结果合并，避免被整体丢弃。
            m_qLFilePathsFromCache = qLFilePaths;
            qLSelDirs = qLNotHitedDirs;
            m_sSearchSource = tr("（结果来自缓存 + 磁盘扫描）");   // 【C12】
        }
    } else {
        m_sSearchSource = tr("（结果来自磁盘扫描）");   // 【C12】
    }

    qInfo() << "[搜索] 启动磁盘扫描 dirs=" << qLSelDirs;

    // P2-15：若前一个搜索线程仍在运行，直接返回。正常流程下搜索按钮运行期间已被禁用，
    // 此检查仅为防御性编程，避免旧线程的 finished lambda（m_threadSearch = nullptr）
    // 误清新线程指针，以及重复启动线程。
    if (m_threadSearch && m_threadSearch->isRunning()) {
        return;
    }

    setActionState(SEARCH_START);

    showProcessPage(RUNNINGTHREAD::SEARCHFILE);// 从磁盘查找比较耗时，切换到处理进度页面
    m_threadSearch = new ThreadSearch(this);
    m_threadSearch->setPara(qLSelDirs, m_qLLabelGroups, sc.logic);
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
    emit sendStatusMessage(tr("正在遍历 %1 个目录…").arg(selDirs.count()));
    qInfo() << "开始遍历: dirs=" << selDirs;

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

void FormFileBrowser::onRecvTraverseResult(QMap<QString, FILE_TAGS> dirAndFileTags,
                                           QMap<QString, QSet<QString>> dirAndTags,
                                           QStringList qLCompletedDirs,
                                           bool bAborted)
{
    // 注意：insertRecord 内部已经按目录做“先删后插”，因此这里不能再整体清空
    // filewithlabels 表，否则之前遍历过的其它目录的缓存会被全部抹掉，
    // 导致“命中缓存”的优化几乎永远不生效。
    // P1-1：只有"完整扫完"的目录才会出现在 dirAndFileTags / qLCompletedDirs 里；
    // 被中断时未扫完的目录不会入库，旧缓存也保持原样（不会被残缺数据覆盖）。
    if (!qLCompletedDirs.isEmpty() || !dirAndFileTags.isEmpty())
        m_sqlOperation->insertRecord(dirAndFileTags, qLCompletedDirs);

    showFilesWidget();
    setActionState(TRAVERER_FINISH);
    emit sendDirAndLabels(dirAndTags);
    ui->pageProgressInfo->clear();

    int nFiles = 0;
    for (auto it = dirAndFileTags.constBegin(); it != dirAndFileTags.constEnd(); ++it)
        nFiles += it.value().size();
    int nLabels = 0;
    for (auto it = dirAndTags.constBegin(); it != dirAndTags.constEnd(); ++it)
        nLabels += it.value().size();

    if (bAborted) {
        // 遍历完成提示：小目录时进度页一闪而过，用户需要明确的完成反馈。
        emit sendStatusMessage(tr("遍历已取消：已完成的 %1 个目录已写入缓存，未完成的目录保持原样")
                                   .arg(qLCompletedDirs.size()));
        qInfo() << "遍历被取消: completedDirs=" << qLCompletedDirs.size()
                << "files=" << nFiles << "labels=" << nLabels;
        return;
    }

    emit sendStatusMessage(tr("遍历完成：%1 个目录，%2 个文件，%3 个标签")
                               .arg(qLCompletedDirs.size()).arg(nFiles).arg(nLabels));
    qInfo() << "遍历完成: dirs=" << qLCompletedDirs.size() << "files=" << nFiles << "labels=" << nLabels;
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

/** 【C16】记住"这次改动之前的标签"，供【撤销上一次标签操作】使用（只保留最近一次）。 */
void FormFileBrowser::onBeforeTagsChanged(const QStringList &qLFilePaths)
{
    if (qLFilePaths.isEmpty())
        return;

    m_mapUndoOldTags.clear();
    for (const QString &sFilePath : qLFilePaths) {
        if (sFilePath.isEmpty() || !QFileInfo(sFilePath).isFile())
            continue;
        m_mapUndoOldTags.insert(sFilePath, ADSOperation::listADSTagsInOrder(sFilePath));
    }
    m_sUndoDesc = tr("撤销：恢复 %1 个文件的标签").arg(m_mapUndoOldTags.size());
    emit sendUndoStateChanged(!m_mapUndoOldTags.isEmpty());
}

/** 【C16】把最近一次标签操作改回原样。 */
void FormFileBrowser::undoLastTagOperation()
{
    if (m_mapUndoOldTags.isEmpty()) {
        QMessageBox::information(this, tr("撤销"), tr("没有可以撤销的标签操作。"));
        return;
    }

    int nFailed = 0;
    for (auto cItr = m_mapUndoOldTags.cbegin(); cItr != m_mapUndoOldTags.cend(); ++cItr) {
        if (!ADSOperation::replaceTags(cItr.key(), cItr.value())) {
            ++nFailed;
            continue;
        }
        if (m_sqlOperation != nullptr)
            m_sqlOperation->invalidateCacheForFile(cItr.key());
    }

    const int nTotal = m_mapUndoOldTags.size();
    m_mapUndoOldTags.clear();
    emit sendUndoStateChanged(false);
    emit sendRefresh();

    if (nFailed > 0)
        QMessageBox::warning(this, tr("撤销"),
                             tr("已恢复 %1 个文件，%2 个失败（可能被占用或只读）。")
                                 .arg(nTotal - nFailed).arg(nFailed));
    else
        emit sendStatusMessage(tr("已撤销上一次标签操作（%1 个文件）。").arg(nTotal));
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

    // 拆分标签：支持中英文分隔符；历史标签名可能合法包含 "，、；"，
    // 用 splitLabelsPreservingExisting 保证已有标签不会被拆开。
    const QStringList qLExisting = ADSOperation::listADSTagsInOrder(sFilePath);
    QStringList qLLabels;
    QStringList qLChanged;
    foreach (const QString &sLabel, Utility::splitLabelsPreservingExisting(sLabels, qLExisting)) {
        // 旧标签原样保留，不重新净化（否则 "a，b" 会变成 "a_b"）
        if (qLExisting.contains(sLabel)) {
            if (!qLLabels.contains(sLabel))
                qLLabels << sLabel;
            continue;
        }
        const QString sSanitized = ADSOperation::sanitizeTagName(sLabel);
        if (!sSanitized.isEmpty() && !qLLabels.contains(sSanitized)) {
            qLLabels << sSanitized;
            if (sSanitized != sLabel.trimmed())
                qLChanged << sLabel + QStringLiteral("→") + sSanitized;
        } else if (sSanitized.isEmpty() && !sLabel.trimmed().isEmpty()) {
            qLChanged << sLabel + tr("（已忽略）");
        }
    }
    // 【D2 修复】输入非空但净化后一个都不剩 → 中止并提示，不能当成"清空全部标签"。
    if (qLLabels.isEmpty() && !sLabels.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("标签名无效"),
                             tr("输入的标签名都不合法（不能用 : / \\ * ? \" < > | 和分隔符），"
                                "未做任何修改。\n\n原始输入：%1")
                                 .arg(sLabels.trimmed()));
        return;
    }

    if (!qLChanged.isEmpty())
        emit sendStatusMessage(tr("部分标签名已规范化：%1").arg(qLChanged.join("，")));

    // replaceTags 会按 qLLabels 的先后顺序写入，并把顺序号存进流内容，
    // 保证下次显示时仍是用户录入的顺序（NTFS 枚举流本身是按流名排序的）。
    onBeforeTagsChanged(QStringList() << sFilePath);   // 【C16】先记下改之前的标签
    const bool bOk = ADSOperation::replaceTags(sFilePath, qLLabels);

    if (m_sqlOperation != nullptr)
        m_sqlOperation->invalidateCacheForFile(sFilePath);   // P1-2：按文件反查遍历根

    emit sendRefresh();

    if (!bOk) {
        QMessageBox::warning(this, tr("保存标签失败"),
                             tr("标签未能完整写入：%1\n"
                                "已尝试恢复操作前的标签。请检查文件是否被占用、只读或磁盘空间不足。")
                                 .arg(sFilePath));
        return;
    }

    // 只把规范化后的非空标签转发给【标签管理】面板
    const QString sNormalizedLabels = qLLabels.join(",");
    if(!sNormalizedLabels.isEmpty())
        emit sendLabels(sNormalizedLabels);
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
