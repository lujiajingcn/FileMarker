#include "dirprogresstracker.h"

#include <QDir>
#include <QFileInfo>

QString DirProgressTracker::dirNodeKey(const QString &sPath)
{
    QString sKey = QDir::cleanPath(sPath);
    while (sKey.size() > 1 && (sKey.endsWith(QLatin1Char('/')) || sKey.endsWith(QLatin1Char('\\'))))
        sKey.chop(1);
    return sKey;
}

void DirProgressTracker::reset(const QStringList &qLRoots)
{
    m_roots = qLRoots;
    m_sCurrentDir.clear();
    m_nodes.clear();

    for (const QString &sRoot : m_roots) {
        DirProgressNode node;
        node.path = dirNodeKey(sRoot);
        node.level = 0;
        node.state = DirNodeState::Waiting;
        m_nodes.insert(node.path, node);
    }
}

int DirProgressTracker::levelOfPath(const QString &sPath) const
{
    const QString sKey = dirNodeKey(sPath);
    for (const QString &sRoot : m_roots) {
        const QString sRootKey = dirNodeKey(sRoot);
        if (sKey.compare(sRootKey, Qt::CaseInsensitive) == 0)
            return 0;
        const QString sPrefix = sRootKey + QLatin1Char('/');
        if (sKey.startsWith(sPrefix, Qt::CaseInsensitive)) {
            const QString sRest = sKey.mid(sPrefix.size());
            return sRest.count(QLatin1Char('/')) + 1;
        }
    }
    return -1;
}

QString DirProgressTracker::ancestorAtDisplayDepth(const QString &sPath) const
{
    QString sAncestor = dirNodeKey(sPath);
    int nLevel = levelOfPath(sAncestor);
    while (nLevel > kMaxDisplayDepth && !sAncestor.isEmpty()) {
        sAncestor = dirNodeKey(QFileInfo(sAncestor).path());
        nLevel = levelOfPath(sAncestor);
    }
    return nLevel >= 0 ? sAncestor : QString();
}

void DirProgressTracker::enterDir(const QString &sPath)
{
    m_sCurrentDir = dirNodeKey(sPath);
    const int nLevel = levelOfPath(sPath);
    if (nLevel < 0)
        return;

    if (nLevel <= kMaxDisplayDepth) {
        const QString sKey = dirNodeKey(sPath);
        auto it = m_nodes.find(sKey);
        if (it == m_nodes.end()) {
            DirProgressNode node;
            node.path = sKey;
            node.level = nLevel;
            node.state = DirNodeState::Searching;
            m_nodes.insert(sKey, node);
        } else {
            it->state = DirNodeState::Searching;
        }
        return;
    }

    // 更深的目录：不建节点，只把子目录数记到第 3 层祖先上
    const QString sAncestor = ancestorAtDisplayDepth(sPath);
    if (sAncestor.isEmpty())
        return;
    auto it = m_nodes.find(sAncestor);
    if (it != m_nodes.end())
        ++it->subDirs;
}

void DirProgressTracker::leaveDir(const QString &sPath, DirOutcome outcome)
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

    if (nLevel <= kMaxDisplayDepth) {
        auto it = m_nodes.find(dirNodeKey(sPath));
        if (it != m_nodes.end())
            it->state = toState(outcome);
        return;
    }

    // 深层目录出了问题，往上汇总到第 3 层祖先（但不覆盖"已停止"这种更明确的状态）
    if (outcome == DirOutcome::Complete || outcome == DirOutcome::Skipped)
        return;
    const QString sAncestor = ancestorAtDisplayDepth(sPath);
    if (sAncestor.isEmpty())
        return;
    auto it = m_nodes.find(sAncestor);
    if (it != m_nodes.end() && it->state != DirNodeState::Stopped)
        it->state = DirNodeState::Incomplete;
}

void DirProgressTracker::addFile(bool bMatched)
{
    QString sPath = dirNodeKey(m_sCurrentDir);
    while (!sPath.isEmpty()) {
        const QString sKey = dirNodeKey(sPath);
        const int nLevel = levelOfPath(sKey);
        if (nLevel < 0)
            break;
        if (nLevel <= kMaxDisplayDepth) {
            auto it = m_nodes.find(sKey);
            if (it != m_nodes.end()) {
                ++it->files;
                if (bMatched)
                    ++it->matches;
            }
        }
        if (nLevel == 0)
            break;
        // QFileInfo("E:/bin").path() 返回带斜杠的 "E:/"，每步都要重新规范化
        sPath = dirNodeKey(QFileInfo(sKey).path());
    }
}

DirProgressSnapshot DirProgressTracker::snapshot() const
{
    DirProgressSnapshot snapshot;
    snapshot.reserve(m_nodes.size());
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it)
        snapshot.append(it.value());
    return snapshot;
}
