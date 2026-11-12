#include "paralleldirtraverser.h"

#include <QAtomicInt>
#include <QDir>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>
#include <QtGlobal>
#include <QWaitCondition>

#ifdef Q_OS_WIN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <QtCore/qt_windows.h>
// DEVICE_SEEK_PENALTY_DESCRIPTOR / IOCTL_STORAGE_QUERY_PROPERTY 在 winioctl.h 里
#include <winioctl.h>
#endif

// 说明：MinGW-w64 13（Qt 6 自带工具链）的 winioctl.h 已经有
// DEVICE_SEEK_PENALTY_DESCRIPTOR 与 IOCTL_STORAGE_QUERY_PROPERTY，这里不再补定义。
// （Qt5 时代的 MinGW 7 缺这两个，才需要手写兜底；本项目已不再支持 Qt5。）

namespace {


/** 各工作线程共享的状态 */
struct TraverseState
{
    QAtomicInt              pending;          // 还没扫完的目录任务数
    QMutex                  mutex;
    QWaitCondition          condition;
    bool                    done = false;     // pending 归零
    QAtomicInt              aborted;          // 用户停止 / 回调要求中断
    const ParallelTraverseCallbacks *callbacks = nullptr;
    QThreadPool            *pool = nullptr;
};

/** 把一个目录排进线程池（调用前 pending 必须已 +1） */
class DirTask : public QRunnable
{
public:
    DirTask(TraverseState *state, QString sPath)
        : m_state(state), m_sPath(std::move(sPath)) {}

    void run() override
    {
        if (m_state->aborted.loadAcquire() != 0) {
            finish();
            return;
        }

        const QFileInfo info(m_sPath);
        if (!info.exists() || !info.isDir()) {
            finish();
            return;
        }

        const ParallelTraverseCallbacks &cb = *m_state->callbacks;
        const QString sKey = QDir::cleanPath(m_sPath);

        // 用户跳过 / 默认排除 / 重解析点：整棵子树不进入
        if (TraverseDirectory::isReparsePoint(sKey)) {
            if (cb.onDirLeave)
                cb.onDirLeave(sKey, DirOutcome::Skipped);
            finish();
            return;
        }
        if (TraverseDirectory::isPathSkipped(sKey)
            || (cb.shouldSkipDir && cb.shouldSkipDir(sKey))) {
            TraverseDirectory::noteSkippedPath();   // 供日志统计"跳过了多少个目录"
            if (cb.onDirLeave)
                cb.onDirLeave(sKey, DirOutcome::Skipped);
            finish();
            return;
        }
        if (TraverseDirectory::isStopped() || TraverseDirectory::isPathStopRequested(sKey)) {
            if (cb.onDirLeave)
                cb.onDirLeave(sKey, DirOutcome::Stopped);
            finish();
            return;
        }

        if (cb.onDirEnter)
            cb.onDirEnter(sKey);

        QDir dir(sKey);
        bool bOk = true;

        // ---- 本目录下的文件 ----
        const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::NoSort);
        for (const QFileInfo &fileInfo : files) {
            if (TraverseDirectory::isStopped() || TraverseDirectory::isPathStopRequested(sKey)) {
                bOk = false;
                break;
            }
            if (cb.onFile && !cb.onFile(fileInfo)) {
                bOk = false;
                m_state->aborted.storeRelease(1);
                break;
            }
        }

        // ---- 子目录：交给线程池，注意 pending 必须先 +1 再入队 ----
        if (bOk) {
            const QFileInfoList subDirs =
                dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::NoSort);
            for (const QFileInfo &subDirInfo : subDirs) {
                if (TraverseDirectory::isStopped() || TraverseDirectory::isPathStopRequested(sKey)) {
                    bOk = false;
                    break;
                }
                const QString sSubPath = subDirInfo.absoluteFilePath();
                m_state->pending.ref();
                m_state->pool->start(new DirTask(m_state, sSubPath));
            }
        }

        if (bOk && (TraverseDirectory::isStopped() || TraverseDirectory::isPathStopRequested(sKey)))
            bOk = false;

        if (cb.onDirLeave)
            cb.onDirLeave(sKey, bOk ? DirOutcome::Complete : DirOutcome::Stopped);

        finish();
    }

private:
    void finish()
    {
        if (m_state->pending.deref() == 0) {
            QMutexLocker locker(&m_state->mutex);
            m_state->done = true;
            m_state->condition.wakeAll();
        }
    }

    TraverseState *m_state;
    QString        m_sPath;
};

}   // namespace

bool ParallelDirTraverser::incursSeekPenalty(const QString &sPath)
{
#ifdef Q_OS_WIN
    if (sPath.isEmpty())
        return false;

    // 取盘符（"E:/..." → "E:"），用卷句柄查询"是否有寻道代价"
    QString sRoot = QDir::toNativeSeparators(sPath);
    if (sRoot.size() >= 2 && sRoot.at(1) == QLatin1Char(':'))
        sRoot = sRoot.left(2);
    else
        return false;

    const QString sVolume = QStringLiteral("\\\\.\\") + sRoot;
    HANDLE hVolume = CreateFileW(reinterpret_cast<const wchar_t *>(sVolume.utf16()),
                                 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, 0, nullptr);
    if (hVolume == INVALID_HANDLE_VALUE)
        return false;

    STORAGE_PROPERTY_QUERY query;
    ZeroMemory(&query, sizeof(query));
    // 7 = StorageDeviceSeekPenaltyProperty（MinGW 的头里没这个枚举项，直接给数值）
    query.PropertyId = static_cast<STORAGE_PROPERTY_ID>(7);
    query.QueryType = PropertyStandardQuery;

    DEVICE_SEEK_PENALTY_DESCRIPTOR descriptor;
    ZeroMemory(&descriptor, sizeof(descriptor));
    DWORD nBytes = 0;
    const BOOL bOk = DeviceIoControl(hVolume, IOCTL_STORAGE_QUERY_PROPERTY,
                                     &query, sizeof(query),
                                     &descriptor, sizeof(descriptor), &nBytes, nullptr);
    CloseHandle(hVolume);

    if (!bOk)
        return false;
    return descriptor.IncursSeekPenalty != FALSE;
#else
    Q_UNUSED(sPath);
    return false;
#endif
}

int ParallelDirTraverser::threadCountForPath(const QString &sPath)
{
    // 同一个卷反复查 DeviceIoControl 没必要，按盘符缓存判定结果。
    // （同一根目录下的子目录都在同一个卷上，所以按根判定就等价于按"当前目录"判定；
    //   跨卷的 junction 我们本来就整棵跳过，不会扫到别的盘。）
    QString sVolumeKey = QDir::toNativeSeparators(sPath);
    if (sVolumeKey.size() >= 2 && sVolumeKey.at(1) == QLatin1Char(':'))
        sVolumeKey = sVolumeKey.left(2).toUpper();
    if (sVolumeKey.isEmpty())
        sVolumeKey = QStringLiteral("<unknown>");

    static QMutex s_mutex;
    static QHash<QString, int> s_cache;
    {
        QMutexLocker locker(&s_mutex);
        const auto it = s_cache.constFind(sVolumeKey);
        if (it != s_cache.constEnd())
            return it.value();
    }

    const int nThreads = defaultThreadCount(sPath);
    {
        QMutexLocker locker(&s_mutex);
        s_cache.insert(sVolumeKey, nThreads);
    }
    return nThreads;
}

int ParallelDirTraverser::defaultThreadCount(const QString &sSamplePath)
{
    // 可用环境变量覆盖（FM_TRAVERSE_THREADS=1 即退化为串行），便于对比耗时/排查问题
    bool bOk = false;
    const int nOverride = qEnvironmentVariableIntValue("FM_TRAVERSE_THREADS", &bOk);
    if (bOk && nOverride > 0)
        return nOverride;

    // 机械盘：并行只会让磁头来回跳。
    // 实测（E: 盘 40188 个文件）：1 线程 11.4s、2 线程 15.4s、4 线程 14.9s —— 并行反而更慢，
    // 所以机械盘直接串行；SSD 上同一批文件 1 线程 2.6s、4 线程 1.1s，值得并行。
    if (incursSeekPenalty(sSamplePath))
        return 1;

    const int nCores = QThread::idealThreadCount();
    return qBound(2, nCores > 0 ? nCores : 4, 4);
}

bool ParallelDirTraverser::run(const QStringList &qLRoots, int nThreads,
                               const ParallelTraverseCallbacks &callbacks)
{
    if (qLRoots.isEmpty())
        return true;

    if (nThreads <= 1) {
        // 单线程：直接复用同步版本，行为完全一致（也方便对比耗时）
        TraverseHooks hooks;
        hooks.onFile = callbacks.onFile;
        hooks.onDirEnter = callbacks.onDirEnter;
        hooks.onDirLeave = callbacks.onDirLeave;
        hooks.shouldSkipDir = callbacks.shouldSkipDir;
        bool bAll = true;
        for (const QString &sRoot : qLRoots) {
            if (TraverseDirectory::isStopped()) {
                bAll = false;
                break;
            }
            if (!TraverseDirectory::traverseDirectory(sRoot, hooks, true))
                bAll = false;
        }
        return bAll;
    }

    TraverseState state;
    state.callbacks = &callbacks;
    state.aborted.storeRelease(0);

    QThreadPool pool;
    pool.setMaxThreadCount(nThreads);
    state.pool = &pool;

    for (const QString &sRoot : qLRoots) {
        state.pending.ref();
        pool.start(new DirTask(&state, sRoot));
    }

    {
        QMutexLocker locker(&state.mutex);
        while (!state.done)
            state.condition.wait(&state.mutex, 100);
    }
    pool.waitForDone();

    return state.aborted.loadAcquire() == 0 && !TraverseDirectory::isStopped();
}
