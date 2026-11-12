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
    hooks.onDirEnter = [this](const QString &sPath) { m_dirTracker.enterDir(sPath); };
    hooks.onDirLeave = [this](const QString &sPath, DirOutcome outcome) {
        m_dirTracker.leaveDir(sPath, outcome);
    };
    // 默认跳过 node_modules / .git 这类目录：整盘搜索时它们几乎不可能带标签，
    // 却往往占了文件量的大头，跳过能省下大量 ADS 枚举。
    hooks.shouldSkipDir = [](const QString &sDirPath) -> bool {
        const QString sName = QFileInfo(sDirPath).fileName();
        return TraverseDirectory::isDefaultExcludedDirName(sName);
    };

    QStringList hostFiles;
    TraverseDirectory::resetStop();          // 每个搜索任务开头复位全局停止标志
    TraverseDirectory::resetPathControls();  // 同时清空上一轮遗留的跳过/停止请求
    // 每个搜索根先建一个节点（状态"等待"），界面上能立刻看到要扫哪些目录
    m_dirTracker.reset(m_selDirs);
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

void ThreadSearch::emitDirSnapshot()
{
    emit sendDirSnapshot(m_dirTracker.snapshot());
}

bool ThreadSearch::searchFile(const QFileInfo& fileInfo)
{
    QString sFilePath = fileInfo.absoluteFilePath();
    ++m_scanned;
    const bool bMatched = ADSOperation::isHostFile(sFilePath, m_labelGroups, m_tagLogic);
    if (bMatched)
        m_filePaths << sFilePath;
    m_dirTracker.addFile(bMatched);

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

