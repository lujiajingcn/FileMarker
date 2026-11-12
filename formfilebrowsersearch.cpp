// FormFileBrowser 的搜索相关实现：缓存查询、磁盘扫描、重新扫描、目录遍历。

#include "formfilebrowser.h"
#include <QMenu>
#include <QToolButton>
#include <QAction>
#include <QThread>
#include <QTimer>
#include <QLabel>
#include <QStatusBar>
#include <QStackedWidget>
#include "ui_formfilebrowser.h"

#include <QActionGroup>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>

#include "adsoperation.h"
#include "configoperation.h"
#include "searchmatch.h"
#include "traversedirectory.h"
#include "tagexport.h"
#include "utility.h"

/**
 * 把"缓存写入时间"翻译成一句人话：刚刚遍历 / 12 分钟前遍历 / 3 小时前遍历 / 2 天前遍历。
 * 时间为空（老库没有记录）时返回空串，界面上就只显示"结果来自缓存"。
 */
QString FormFileBrowser::humanReadableCacheAge(const QDateTime &cacheTime)
{
    if (!cacheTime.isValid())
        return QString();
    const qint64 nSeconds = cacheTime.secsTo(QDateTime::currentDateTime());
    if (nSeconds < 0)
        return QString();   // 时钟回拨等异常情况，不做提示
    if (nSeconds < 60)
        return tr("刚刚遍历");
    if (nSeconds < 3600)
        return tr("%1 分钟前遍历").arg(nSeconds / 60);
    if (nSeconds < 86400)
        return tr("%1 小时前遍历").arg(nSeconds / 3600);
    return tr("%1 天前遍历").arg(nSeconds / 86400);
}

/** 结果页的【重新扫描】：忽略缓存，用同样的目录和标签从磁盘重查一遍。 */
void FormFileBrowser::onRescanFromDiskRequested()
{
    if (m_lastSearchDirs.isEmpty() || m_labelGroups.isEmpty()) {
        QMessageBox::information(this, tr("重新扫描"),
                                 tr("上一次的搜索条件已经失效，请重新选择标签再搜索。"));
        return;
    }
    m_rescanDirs = m_lastSearchDirs;
    m_forceDiskScan = true;
    onActionSearchFilesbyLabelsTriggered();
}

void FormFileBrowser::onActionSearchFilesbyLabelsTriggered()
{
    m_cachedFilePaths.clear();   // 清除上一次查找遗留的缓存命中结果

    // 结果页点【重新扫描】时：沿用上一次搜索的目录和标签，并强制走磁盘
    const bool bForceDisk = m_forceDiskScan;
    m_forceDiskScan = false;

    QStringList qLSelDirs = m_rescanDirs;
    m_rescanDirs.clear();
    if (qLSelDirs.isEmpty()) {
        qLSelDirs = getSelDirs();
        if (qLSelDirs.count() == 0) {
            QString sCurDir = ui->pageFiles->getCurDir();
            qLSelDirs.append(sCurDir);
        }
    }
    // 记录搜索条件（后面缓存未命中的目录会被替换，但界面提示应显示用户原始选择）
    const QStringList qLSearchDirs = qLSelDirs;
    m_lastSearchDirs = qLSearchDirs;   // 供结果页【重新扫描】复用

    if (m_selectedLabels.count() == 0) {
        QMessageBox::information(this, "提示", "请先在右侧【标签库（分类）】里选择要查找的标签（可多选）。");
        return;
    }

    // 搜索条件是"分组"——每组 = 选中的标签 + 它的全部子标签。
    // 因此"搜索父标签"能搜到只打了子标签的文件；多个条件之间仍按设置里的 AND/OR 组合。
    if (m_labelGroups.isEmpty()) {
        for (const QString &sLabel : m_selectedLabels) {
            QSet<QString> setGroup;
            setGroup.insert(sLabel);
            m_labelGroups << setGroup;
        }
    }

    // 结果页回显条件时把"含子标签"写出来，让用户明确层级是参与匹配的。
    QStringList qLConditionTexts;
    for (const QSet<QString> &setGroup : m_labelGroups) {
        QStringList qLNames = QStringList(setGroup.values());
        qLNames.sort();
        // 组里"直接选中的那个标签"作为显示名，其余当子标签列出来
        QString sRoot;
        for (const QString &sName : qLNames) {
            if (m_selectedLabels.contains(sName)) {
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
    qInfo() << "开始搜索: dirs=" << qLSelDirs << "labels=" << m_selectedLabels
            << "useCache=" << ConfigOperation::readSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE).bFromCache;

    SearchConfig sc = ConfigOperation::readSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE);

    if (sc.bFromCache && !bForceDisk) {
        QStringList qLFilePaths, qLNotHitedDirs;
        int nExcludedDirs = 0;
        m_sqlOperation->searchFilesByLabels(qLSelDirs, m_labelGroups, sc.logic, qLFilePaths,
                                            qLNotHitedDirs, nExcludedDirs);
        if (nExcludedDirs > 0) {
            // 用户主动跳过的目录不会回退磁盘，所以结果可能偏少，明确告诉用户
            m_excludedDirNote = tr("（已排除 %1 个目录）").arg(nExcludedDirs);
        } else {
            m_excludedDirNote.clear();
        }
        qInfo() << "[搜索] 缓存命中文件=" << qLFilePaths.size() << " 未命中目录数=" << qLNotHitedDirs.size()
                << " 分组数=" << m_labelGroups.size() << " logic=" << int(sc.logic);
        if (qLNotHitedDirs.count() == 0) { // 查找的文件夹全部在缓存数据库中存在，则显示查找结果后结束。
            // 缓存可能过期：把"这份缓存是什么时候写的"一并告诉用户，结果页还能一键重扫
            const QDateTime cacheTime = m_sqlOperation != nullptr
                                            ? m_sqlOperation->oldestCacheTime(qLSelDirs)
                                            : QDateTime();
            const QString sAge = humanReadableCacheAge(cacheTime);
            m_searchSource = sAge.isEmpty() ? tr("（结果来自缓存）")
                                             : tr("（结果来自缓存，%1）").arg(sAge);
            if (!m_excludedDirNote.isEmpty())
                m_searchSource += m_excludedDirNote;
            showFilteredFile(qLFilePaths);
            return;
        } else { // 查找的文件夹有部分未在缓存数据库中，不在的文件夹从磁盘查找
            // 命中缓存的部分先保留，最后与磁盘查找结果合并，避免被整体丢弃。
            m_cachedFilePaths = qLFilePaths;
            qLSelDirs = qLNotHitedDirs;
            m_searchSource = tr("（结果来自缓存 + 磁盘扫描）");
        }
    } else {
        m_searchSource = tr("（结果来自磁盘扫描）");
    if (!m_excludedDirNote.isEmpty())
        m_searchSource += m_excludedDirNote;
    }

    qInfo() << "[搜索] 启动磁盘扫描 dirs=" << qLSelDirs;

    // 若前一个搜索线程仍在运行，直接返回。正常流程下搜索按钮运行期间已被禁用，
    // 此检查仅为防御性编程，避免旧线程的 finished lambda（m_threadSearch = nullptr）
    // 误清新线程指针，以及重复启动线程。
    if (m_threadSearch && m_threadSearch->isRunning()) {
        return;
    }

    setActionState(SEARCH_START);

    showProcessPage(RUNNINGTHREAD::SEARCHFILE);// 从磁盘查找比较耗时，切换到处理进度页面
    m_threadSearch = new ThreadSearch(this);
    m_threadSearch->setPara(qLSelDirs, m_labelGroups, sc.logic);

    // 目录进度树：清空并建立顶层节点；快照按 150ms 合并发过来
    ui->pageProgressInfo->clear();
    ui->pageProgressInfo->resetProgress(qLSelDirs);
    connect(m_threadSearch, &ThreadSearch::sigResult, this, &FormFileBrowser::showFilteredFile);
    connect(m_threadSearch, &ThreadSearch::sendProcessInfo, ui->pageProgressInfo, &FormPageProgressInfo::onRecvProgress);
    connect(m_threadSearch, &ThreadSearch::sendDirSnapshot, ui->pageProgressInfo,
            &FormPageProgressInfo::onRecvDirSnapshot);
    // 单个目录的停止/跳过：直接写进遍历器的控制集合（线程安全），遍历线程下一层就会响应
    connect(ui->pageProgressInfo, &FormPageProgressInfo::sendStopDir, this,
            [](const QString &sPath) { TraverseDirectory::requestStopPath(sPath); });
    connect(ui->pageProgressInfo, &FormPageProgressInfo::sendSkipDir, this,
            [](const QString &sPath) { TraverseDirectory::requestSkipPath(sPath); });
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
    if (selDirs.count() == 0) {
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

void FormFileBrowser::onRecvTraverseResult(const QMap<QString, FILE_TAGS> &dirAndFileTags,
                                           QMap<QString, QSet<QString>> dirAndTags,
                                           const QStringList &qLCompletedDirs,
                                           const QStringList &qLSkippedDirs,
                                           const QStringList &qLIncompleteDirs,
                                           bool bAborted)
{
    // 注意：insertRecord 内部已经按目录做“先删后插”，因此这里不能再整体清空
    // filewithlabels 表，否则之前遍历过的其它目录的缓存会被全部抹掉，
    // 导致“命中缓存”的优化几乎永远不生效。
    // 只有"完整扫完"的目录才会出现在 dirAndFileTags / qLCompletedDirs 里；
    // 被中断时未扫完的目录不会入库，旧缓存也保持原样（不会被残缺数据覆盖）。
    if (!qLCompletedDirs.isEmpty() || !dirAndFileTags.isEmpty()
        || !qLSkippedDirs.isEmpty() || !qLIncompleteDirs.isEmpty())
        m_sqlOperation->insertRecord(dirAndFileTags, qLCompletedDirs, qLSkippedDirs, qLIncompleteDirs);

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
        emit sendStatusMessage(tr("遍历已取消：已完成的 %1 个目录已写入缓存，未完成的目录标记为不完整")
                                   .arg(qLCompletedDirs.size()));
        qInfo() << "遍历被取消: completedDirs=" << qLCompletedDirs.size()
                << "files=" << nFiles << "labels=" << nLabels;
        return;
    }

    emit sendStatusMessage(tr("遍历完成：%1 个目录，%2 个文件，%3 个标签")
                               .arg(qLCompletedDirs.size()).arg(nFiles).arg(nLabels));
    qInfo() << "遍历完成: dirs=" << qLCompletedDirs.size() << "files=" << nFiles << "labels=" << nLabels;
}

/**
 * 搜索按钮右侧小箭头菜单的内容。
 *
 * 主按钮点一下 = 直接用上次保存的条件搜索（不再每次弹对话框）；
 * 想改条件时点小箭头：这里就是"查找条件"的唯一入口，
 *   - 标签关系：与/或
 *   - 数据来源：缓存/磁盘
 * （原来还有一个"搜索条件…"对话框，内容和这两个子菜单完全重复，已删除。）
 */
void FormFileBrowser::initSearchButtonMenu()
{
    m_searchMenu = new QMenu(this);

    QMenu *logicMenu = m_searchMenu->addMenu(tr("标签关系"));
    auto *logicGroup = new QActionGroup(this);
    logicGroup->setExclusive(true);
    m_logicAndAction = logicMenu->addAction(tr("与：同时满足所有标签"));
    m_logicOrAction = logicMenu->addAction(tr("或：满足任意一个标签"));
    for (QAction *menuAction : {m_logicAndAction, m_logicOrAction}) {
        menuAction->setCheckable(true);
        logicGroup->addAction(menuAction);
    }
    connect(m_logicAndAction, &QAction::triggered, this, [this]() { applySearchLogic(labelLogic::AND); });
    connect(m_logicOrAction, &QAction::triggered, this, [this]() { applySearchLogic(labelLogic::OR); });

    QMenu *sourceMenu = m_searchMenu->addMenu(tr("数据来源"));
    auto *sourceGroup = new QActionGroup(this);
    sourceGroup->setExclusive(true);
    m_sourceCache = sourceMenu->addAction(tr("优先从缓存数据库（快，可能过期）"));
    m_sourceDisk = sourceMenu->addAction(tr("直接从磁盘（慢，最新）"));
    for (QAction *menuAction : {m_sourceCache, m_sourceDisk}) {
        menuAction->setCheckable(true);
        sourceGroup->addAction(menuAction);
    }
    connect(m_sourceCache, &QAction::triggered, this, [this]() { applySearchSource(true); });
    connect(m_sourceDisk, &QAction::triggered, this, [this]() { applySearchSource(false); });

    connect(m_searchMenu, &QMenu::aboutToShow, this, &FormFileBrowser::refreshSearchButtonMenu);
}

/** 打开菜单前按 config.ini 里的当前配置刷新勾选状态。 */
void FormFileBrowser::refreshSearchButtonMenu()
{
    const SearchConfig sc = ConfigOperation::readSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE);
    if (m_logicAndAction != nullptr && m_logicOrAction != nullptr) {
        m_logicAndAction->setChecked(sc.logic == labelLogic::AND);
        m_logicOrAction->setChecked(sc.logic == labelLogic::OR);
    }
    if (m_sourceCache != nullptr && m_sourceDisk != nullptr) {
        m_sourceCache->setChecked(sc.bFromCache);
        m_sourceDisk->setChecked(!sc.bFromCache);
    }
}

void FormFileBrowser::applySearchLogic(labelLogic logic)
{
    SearchConfig sc = ConfigOperation::readSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE);
    sc.logic = logic;
    ConfigOperation::writeSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE, sc);
    emit sendStatusMessage(tr("查找条件已更新：标签关系 = %1")
                               .arg(logic == labelLogic::AND ? tr("与") : tr("或")));
}

void FormFileBrowser::applySearchSource(bool bFromCache)
{
    SearchConfig sc = ConfigOperation::readSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE);
    sc.bFromCache = bFromCache;
    ConfigOperation::writeSearchConfig(g_sAppDir + "/" + SEARCH_CONFIG_FILE, sc);
    emit sendStatusMessage(tr("查找条件已更新：数据来源 = %1")
                               .arg(bFromCache ? tr("缓存数据库") : tr("磁盘扫描")));
}

void FormFileBrowser::onRecvStopSearch()
{
    switch (m_thread)
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
