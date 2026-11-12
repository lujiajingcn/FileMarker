#include "threadsearch.h"
#include "adsoperation.h"
#include <QDebug>

#include "traversedirectory.h"

ThreadSearch::ThreadSearch(QObject *parent) : QThread(parent)
{
    connect(this, &ThreadSearch::finished, this, &QObject::deleteLater);
}

void ThreadSearch::stopThread()
{
    TraverseDirectory::stop();
}

void ThreadSearch::setPara(const QStringList &selDirs, const QList<QSet<QString>> &qLLabelGroups, const labelLogic taglLogic)
{
    m_selDirs = selDirs;
    m_labelGroups = qLLabelGroups;
    m_tagLogic = taglLogic;
}

void ThreadSearch::run()
{
    // 目录级遍历：文件交给 searchFile，目录进出事件这一步先不接 UI（步骤 2 再接目录进度树）。
    TraverseHooks hooks;
    hooks.onFile = [this](const QFileInfo &info) -> bool { return searchFile(info); };
    hooks.onDirEnter = [this](const QString &sPath) { onDirEnterHook(sPath); };
    hooks.onDirLeave = [this](const QString &sPath, DirOutcome outcome) {
        onDirLeaveHook(sPath, outcome);
    };
    // 默认跳过 node_modules / .git 这类目录：整盘搜索时它们几乎不可能带标签，
    // 却往往占了文件量的大头，跳过能省下大量 ADS 枚举。
    hooks.shouldSkipDir = [](const QString &sDirPath) -> bool {
        const QString sName = QFileInfo(sDirPath).fileName();
        return TraverseDirectory::isDefaultExcludedDirName(sName);
    };

    QStringList hostFiles;
    // 规范化搜索根：整盘搜索时根是 "E:/"（结尾带斜杠），
    // 若不处理，下面 levelOfPath() 的前缀匹配会全部失配 → 进度树只剩一个根节点、计数全为 0。
    for (QString &sRoot : m_selDirs) {
        while (sRoot.endsWith(QLatin1Char('/')) || sRoot.endsWith(QLatin1Char('\\')))
            sRoot.chop(1);
    }

    TraverseDirectory::resetStop();          // 每个搜索任务开头复位全局停止标志
    TraverseDirectory::resetPathControls();  // 同时清空上一轮遗留的跳过/停止请求
    m_dirNodes.clear();
    // 每个搜索根先建一个节点（状态"等待"），界面上能立刻看到要扫哪些目录
    for (const QString &sRoot : m_selDirs) {
        DirProgressNode node;
        node.path = sRoot;
        node.level = 0;
        node.state = DirNodeState::Waiting;
        m_dirNodes.insert(sRoot, node);
    }
    emitDirSnapshot();
    m_progressTimer.start();
    for (const QString &selDir : m_selDirs)
        TraverseDirectory::traverseDirectory(selDir, hooks, true);

    emitDirSnapshot();
    emit sendProcessInfo(QString("扫描结束：共扫描 %1 个文件，命中 %2 个（跳过 %3 个目录）")
                             .arg(m_scanned).arg(m_filePaths.size())
                             .arg(TraverseDirectory::skippedPathCount()));

    qInfo() << "磁盘查找结束: scanned=" << m_scanned << "matched=" << m_filePaths.size()
            << "logic=" << int(m_tagLogic) << "groups=" << m_labelGroups;
    emit sigResult(m_filePaths);
}

/** 某个路径相对哪个搜索根、在第几层（0 = 搜索根本身）；不属于任何搜索根返回 -1。 */
int ThreadSearch::levelOfPath(const QString &sPath) const
{
    for (const QString &sRoot : m_selDirs) {
        if (sPath.compare(sRoot, Qt::CaseInsensitive) == 0)
            return 0;
        if (sPath.startsWith(sRoot + QLatin1Char('/'), Qt::CaseInsensitive)) {
            const QString sRest = sPath.mid(sRoot.size() + 1);
            return sRest.count(QLatin1Char('/')) + 1;
        }
    }
    return -1;
}

/** 超过显示深度时，找它对应的第 3 层祖先（聚合到那一层）。 */
QString ThreadSearch::ancestorAtDisplayDepth(const QString &sPath) const
{
    QString sAncestor = sPath;
    int nLevel = levelOfPath(sAncestor);
    while (nLevel > 3 && !sAncestor.isEmpty()) {
        sAncestor = QFileInfo(sAncestor).path();
        nLevel = levelOfPath(sAncestor);
    }
    return nLevel >= 0 ? sAncestor : QString();
}

void ThreadSearch::onDirEnterHook(const QString &sPath)
{
    m_sCurrentDir = sPath;
    const int nLevel = levelOfPath(sPath);
    if (nLevel < 0)
        return;

    if (nLevel <= 3) {
        auto it = m_dirNodes.find(sPath);
        if (it == m_dirNodes.end()) {
            DirProgressNode node;
            node.path = sPath;
            node.level = nLevel;
            node.state = DirNodeState::Searching;
            m_dirNodes.insert(sPath, node);
        } else {
            it->state = DirNodeState::Searching;
        }
        return;
    }

    // 更深的目录：不建节点，只把子目录数记到第 3 层祖先上
    const QString sAncestor = ancestorAtDisplayDepth(sPath);
    if (sAncestor.isEmpty())
        return;
    auto it = m_dirNodes.find(sAncestor);
    if (it != m_dirNodes.end())
        ++it->subDirs;
}

void ThreadSearch::onDirLeaveHook(const QString &sPath, DirOutcome outcome)
{
    const int nLevel = levelOfPath(sPath);
    if (nLevel < 0)
        return;

    auto toState = [](DirOutcome value) {
        switch (value) {
        case DirOutcome::Complete:   return DirNodeState::Complete;
        case DirOutcome::Skipped:    return DirNodeState::Skipped;
        case DirOutcome::Stopped:    return DirNodeState::Stopped;
        case DirOutcome::Incomplete: return DirNodeState::Incomplete;
        case DirOutcome::Failed:     return DirNodeState::Incomplete;
        }
        return DirNodeState::Incomplete;
    };

    if (nLevel <= 3) {
        auto it = m_dirNodes.find(sPath);
        if (it != m_dirNodes.end())
            it->state = toState(outcome);
        return;
    }

    // 深层目录出了问题，往上汇总到第 3 层祖先（但不覆盖"已停止"这种更明确的状态）
    if (outcome == DirOutcome::Complete || outcome == DirOutcome::Skipped)
        return;
    const QString sAncestor = ancestorAtDisplayDepth(sPath);
    if (sAncestor.isEmpty())
        return;
    auto it = m_dirNodes.find(sAncestor);
    if (it != m_dirNodes.end() && it->state != DirNodeState::Stopped)
        it->state = DirNodeState::Incomplete;
}

/** 把当前文件的扫描/命中记到它所属的各级（≤3 层）目录节点上。 */
void ThreadSearch::countFileInCurrentDir(bool bMatched)
{
    QString sPath = m_sCurrentDir;
    while (!sPath.isEmpty()) {
        const int nLevel = levelOfPath(sPath);
        if (nLevel < 0)
            break;
        if (nLevel <= 3) {
            auto it = m_dirNodes.find(sPath);
            if (it != m_dirNodes.end()) {
                ++it->files;
                if (bMatched)
                    ++it->matches;
            }
        }
        if (nLevel == 0)
            break;
        sPath = QFileInfo(sPath).path();
    }
}

void ThreadSearch::emitDirSnapshot()
{
    DirProgressSnapshot snapshot;
    snapshot.reserve(m_dirNodes.size());
    for (auto it = m_dirNodes.constBegin(); it != m_dirNodes.constEnd(); ++it)
        snapshot.append(it.value());
    emit sendDirSnapshot(snapshot);
}

bool ThreadSearch::searchFile(const QFileInfo& fileInfo)
{
    QString sFilePath = fileInfo.absoluteFilePath();
    ++m_scanned;
    const bool bMatched = ADSOperation::isHostFile(sFilePath, m_labelGroups, m_tagLogic);
    if (bMatched)
        m_filePaths << sFilePath;
    countFileInCurrentDir(bMatched);

    // 进度节流：以前每个文件都 emit 一次，几十万个文件会把 UI 线程的事件队列灌满，
    // 界面表现就是"一直卡着"。现在每 150ms 才报一次，并带上已扫描文件数。
    ++m_progressFiles;
    if (!m_progressTimer.isValid() || m_progressTimer.elapsed() >= 150) {
        m_progressTimer.restart();
        emit sendProcessInfo(sFilePath);
        emitDirSnapshot();
        m_progressFiles = 0;
    }

    return true;
}

