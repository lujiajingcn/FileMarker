#include "threadtraversedirs.h"

#include "adsoperation.h"
#include "utility.h"
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
    QStringList qLCompletedDirs;      // 完整扫完的目录
    QStringList qLSkippedDirs;        // 用户主动跳过 / 默认排除的目录（记为"已排除"）
    QStringList qLIncompleteDirs;     // 被停止、读不了、或子目录没扫完的目录（缓存不可信）

    TraverseDirectory::resetStop();          // 每个遍历任务开头复位全局停止标志
    TraverseDirectory::resetPathControls();  // 清空上一轮遗留的跳过/停止请求

    // 目录进度树：先建根节点（状态"等待"），然后每 150ms 发一次快照
    m_dirTracker.reset(m_selDirs);
    m_snapshotTimer.start();
    emit sendDirSnapshot(m_dirTracker.snapshot());

    for (const QString &selDir : m_selDirs) {
        // 用户已点停止：后面的目录一个都不要进，避免又跑一遍
        if (TraverseDirectory::isStopped())
            break;

        m_tags.clear();
        m_fileTags.clear();

        // 目录级钩子：把每个目录的结局收集起来，写缓存时据此打 complete / excluded 标记
        TraverseHooks hooks;
        hooks.onFile = [this](const QFileInfo &info) -> bool { return traverseDir(info); };
        hooks.onDirEnter = [this, &selDir](const QString &sPath) {
            Q_UNUSED(selDir);
            m_dirTracker.enterDir(sPath);
            emitSnapshotIfDue();
        };
        hooks.onDirLeave = [&](const QString &sPath, DirOutcome outcome) {
            m_dirTracker.leaveDir(sPath, outcome);
            emitSnapshotIfDue();
            switch (outcome) {
            case DirOutcome::Complete:
                qLCompletedDirs << sPath;
                break;
            case DirOutcome::Skipped:
                qLSkippedDirs << sPath;
                break;
            case DirOutcome::Stopped:
            case DirOutcome::Incomplete:
            case DirOutcome::Failed:
                qLIncompleteDirs << sPath;
                break;
            }
        };
        // 默认排除目录：和搜索保持一致，避免把 node_modules/.pnpm-store 这类目录写进缓存
        hooks.shouldSkipDir = [](const QString &sDirPath) -> bool {
            return TraverseDirectory::isDefaultExcludedDirName(QFileInfo(sDirPath).fileName());
        };

        const bool bCompleted = TraverseDirectory::traverseDirectory(selDir, hooks, true);
        if (bCompleted && !m_fileTags.isEmpty()) {
            dirAndFileTags[selDir] = m_fileTags;
            if (m_tags.count() != 0)
                dirAndTags[selDir] = m_tags;
        }
        // 未扫完的目录：本地结果直接丢弃，绝不进入缓存（否则缓存里只剩残缺数据）
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

    // 先计数再判断有没有标签：进度树上的"已扫"应该是**扫过的文件数**，
    // 而不是"有标签的文件数"，否则没有标签的目录会一直显示 0，看着像卡住。
    m_dirTracker.addFile(false);   // 遍历不按标签匹配，"命中"一栏留空
    emitSnapshotIfDue();

    QStringList adsFileNames = ADSOperation::listADSFileName(filePath);
    if (adsFileNames.isEmpty())
        return true;

    m_fileTags[filePath] = adsFileNames;

    QSet<QString> setAdsNames = Utility::toSet(adsFileNames);
    m_tags.unite(setAdsNames);

    emit sendProcessInfo(filePath);

// QThread::msleep(1);//让出时间片，使得ui有时间响应

    return true;
}
