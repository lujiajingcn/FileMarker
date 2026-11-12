#include "threadsearch.h"
#include "adsoperation.h"
#include <QDebug>

#include "paralleldirtraverser.h"
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
    // 并行遍历：多个目录同时扫。整盘搜索是纯 IO，串行时时间几乎全花在
    // 单个文件的 ADS 枚举上，4 个线程能把等待摊开。
    ParallelTraverseCallbacks hooks;
    hooks.onFile = [this](const QFileInfo &info) -> bool { return searchFile(info); };
    hooks.onDirEnter = [this](const QString &sPath) {
        QMutexLocker locker(&m_mutex);
        m_dirTracker.enterDir(sPath);
    };
    hooks.onDirLeave = [this](const QString &sPath, DirOutcome outcome) {
        QMutexLocker locker(&m_mutex);
        m_dirTracker.leaveDir(sPath, outcome);
    };
    // 默认跳过 node_modules / .git 这类目录：整盘搜索时它们几乎不可能带标签，
    // 却往往占了文件量的大头，跳过能省下大量 ADS 枚举。
    // 例外：用户**明确选中**的搜索根即使叫 node_modules 也要照扫 ——
    // 否则"我就想搜这个目录"会永远返回空。
    hooks.shouldSkipDir = [this](const QString &sDirPath) -> bool {
        const QString sClean = QDir::cleanPath(sDirPath);
        for (const QString &sRoot : m_selDirs) {
            if (QDir::cleanPath(sRoot).compare(sClean, Qt::CaseInsensitive) == 0)
                return false;
        }
        const QString sName = QFileInfo(sDirPath).fileName();
        return TraverseDirectory::isDefaultExcludedDirName(sName);
    };

    TraverseDirectory::resetStop();          // 每个搜索任务开头复位全局停止标志
    TraverseDirectory::resetPathControls();  // 同时清空上一轮遗留的跳过/停止请求
    // 每个搜索根先建一个节点（状态"等待"），然后**把显示深度内的子目录一次性建好**：
    // 这样进度树一上来就是完整结构（全是"等待"），扫描过程中只改状态和计数。
    m_dirTracker.reset(m_selDirs);
    m_dirTracker.prepopulate([this]() { emitDirSnapshot(); });
    emitDirSnapshot();
    m_progressTimer.start();
    // 每个搜索根**各自**判断所在磁盘类型：SSD 用多线程，机械盘用单线程。
    // （同一根目录下的子目录都在同一个卷上，所以按根判定就等于按"当前目录"判定）
    bool bCompleted = true;   // 中途被停止时置 false，结果页要提示"不完整"
    for (const QString &sRoot : m_selDirs) {
        if (TraverseDirectory::isStopped()) {
            bCompleted = false;
            break;
        }
        const int nThreads = ParallelDirTraverser::threadCountForPath(sRoot);
        qInfo("[搜索] 根=%s 线程数=%d（机械盘=1，SSD=多线程）", qPrintable(sRoot), nThreads);
        if (!ParallelDirTraverser::run(QStringList() << sRoot, nThreads, hooks))
            bCompleted = false;
        emitDirSnapshot();
    }

    emitDirSnapshot();
    emit sendProcessInfo(QString("扫描结束：共扫描 %1 个文件，命中 %2 个（跳过 %3 个目录）")
                             .arg(m_scanned).arg(m_filePaths.size())
                             .arg(TraverseDirectory::skippedPathCount()));

    qInfo() << "磁盘查找结束: scanned=" << m_scanned << "matched=" << m_filePaths.size()
            << "logic=" << int(m_tagLogic) << "groups=" << m_labelGroups;
    emit sigResult(m_filePaths, bCompleted);
}

void ThreadSearch::emitDirSnapshot()
{
    emit sendDirSnapshot(m_dirTracker.snapshot());
}

bool ThreadSearch::searchFile(const QFileInfo& fileInfo)
{
    QString sFilePath = fileInfo.absoluteFilePath();
    // ADS 枚举（最耗时的一步）放到锁外面，锁只保护共享容器
    const bool bMatched = ADSOperation::isHostFile(sFilePath, m_labelGroups, m_tagLogic);

    QMutexLocker locker(&m_mutex);
    ++m_scanned;
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

