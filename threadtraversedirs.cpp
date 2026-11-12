#include "threadtraversedirs.h"

#include "adsoperation.h"
#include "utility.h"
#include "paralleldirtraverser.h"
#include "traversedirectory.h"

ThreadTraverseDirs::ThreadTraverseDirs(QObject *parent) : QThread(parent)
{
    qRegisterMetaType<QMap<QString, QSet<QString>>>();


    connect(this, &ThreadTraverseDirs::finished, this, &QObject::deleteLater);
}

void ThreadTraverseDirs::stopThread()
{
    TraverseDirectory::stop();
}

void ThreadTraverseDirs::setSelDirs(const QStringList &selDirs)
{
    m_selDirs = selDirs;
}

void ThreadTraverseDirs::run()
{
    QMap<QString, FILE_TAGS> dirAndFileTags;
    QMap<QString, QSet<QString>> dirAndTags;
    // 注意：缓存是按"遍历根"存的（filewithlabels.dir = 根目录）。
    // 所以 dirs 表里只登记**根目录**的完整/不完整状态；
    // 如果连子目录也登记成 complete=1，用户选中子目录时会"命中缓存但查不到文件"，
    // 结果是静默漏结果。子目录只有一种情况要登记：被跳过（excluded，明确表示不扫）。
    QStringList qLCompletedDirs;
    QStringList qLSkippedDirs;
    QStringList qLIncompleteDirs;

    TraverseDirectory::resetStop();          // 每个遍历任务开头复位全局停止标志
    TraverseDirectory::resetPathControls();  // 清空上一轮遗留的跳过/停止请求

    // 目录进度树：先建根节点（状态"等待"），然后每 150ms 发一次快照
    m_dirTracker.reset(m_selDirs);
    // 先把子目录建好（分块发快照，界面边建边显示），再开始真正的遍历
    m_dirTracker.prepopulate([this]() { emit sendDirSnapshot(m_dirTracker.snapshot()); });
    m_snapshotTimer.start();
    emit sendDirSnapshot(m_dirTracker.snapshot());

    for (const QString &selDir : m_selDirs) {
        // 用户已点停止：后面的目录一个都不要进，避免又跑一遍
        if (TraverseDirectory::isStopped())
            break;

        m_tags.clear();
        m_fileTags.clear();

        // 目录级钩子：把每个目录的结局收集起来，写缓存时据此打 complete / excluded 标记。
        // 并行遍历时这些回调来自多个线程，容器和进度统计统一用 m_mutex 保护。
        ParallelTraverseCallbacks hooks;
        hooks.onFile = [this](const QFileInfo &info) -> bool { return traverseDir(info); };
        hooks.onDirEnter = [this](const QString &sPath) {
            QMutexLocker locker(&m_mutex);
            m_dirTracker.enterDir(sPath);
        };
        QStringList qLSkippedThisRoot;        // 本轮被跳过的目录（记 excluded 行）
        bool bThisRootIncomplete = false;     // 有子树被停止/读不了 → 整个根的数据不完整

        hooks.onDirLeave = [&](const QString &sPath, DirOutcome outcome) {
            QMutexLocker locker(&m_mutex);
            m_dirTracker.leaveDir(sPath, outcome);
            switch (outcome) {
            case DirOutcome::Skipped:
                qLSkippedThisRoot << sPath;
                break;
            case DirOutcome::Stopped:
            case DirOutcome::Incomplete:
            case DirOutcome::Failed:
                bThisRootIncomplete = true;
                break;
            case DirOutcome::Complete:
                break;
            }
        };
        // 默认排除目录：和搜索保持一致，避免把 node_modules/.pnpm-store 这类目录写进缓存
        hooks.shouldSkipDir = [](const QString &sDirPath) -> bool {
            return TraverseDirectory::isDefaultExcludedDirName(QFileInfo(sDirPath).fileName());
        };

        const int nThreads = ParallelDirTraverser::threadCountForPath(selDir);
        qInfo("[遍历] 并行线程数=%d dir=%s", nThreads, qPrintable(selDir));
        const bool bCompleted = ParallelDirTraverser::run(QStringList() << selDir, nThreads, hooks);

        qLSkippedDirs << qLSkippedThisRoot;

        if (bCompleted && !bThisRootIncomplete) {
            qLCompletedDirs << selDir;
            if (!m_fileTags.isEmpty()) {
                dirAndFileTags[selDir] = m_fileTags;
                if (m_tags.count() != 0)
                    dirAndTags[selDir] = m_tags;
            }
        } else {
            // 根没扫完（被全局停止，或有子树被停止/读不了）：
            // 本地结果直接丢弃，只把根标记为"不完整"，让下次搜索回退磁盘重扫。
            qLIncompleteDirs << selDir;
        }
    }

    emit sendDirSnapshot(m_dirTracker.snapshot());
    emit sendResult(dirAndFileTags, dirAndTags, qLCompletedDirs, qLSkippedDirs,
                    qLIncompleteDirs, TraverseDirectory::isStopped());
}

/** 节流发快照：遍历时目录事件很密集，不能每次都发（会灌满 UI 线程）。 */
void ThreadTraverseDirs::emitSnapshotIfDue()
{
    if (m_snapshotTimer.isValid() && m_snapshotTimer.elapsed() < 150)
        return;
    m_snapshotTimer.restart();
    emit sendDirSnapshot(m_dirTracker.snapshot());
}

bool ThreadTraverseDirs::traverseDir(const QFileInfo& fileInfo)
{
    QString filePath = fileInfo.absoluteFilePath();

    const QStringList adsFileNames = ADSOperation::listADSFileName(filePath);

    QMutexLocker locker(&m_mutex);
    // 先计数再判断有没有标签：进度树上的"已扫"应该是**扫过的文件数**，
    // 而不是"有标签的文件数"，否则没有标签的目录会一直显示 0，看着像卡住。
    m_dirTracker.addFile(false);   // 遍历不按标签匹配，"命中"一栏留空

    if (adsFileNames.isEmpty())
        return true;

    m_fileTags[filePath] = adsFileNames;

    QSet<QString> setAdsNames = Utility::toSet(adsFileNames);
    m_tags.unite(setAdsNames);

    emit sendProcessInfo(filePath);

// QThread::msleep(1);//让出时间片，使得ui有时间响应

    return true;
}
