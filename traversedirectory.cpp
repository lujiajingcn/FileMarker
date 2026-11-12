#include "traversedirectory.h"

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

#include <QReadWriteLock>
#include <QSet>

std::atomic<bool> TraverseDirectory::m_isStop{false};
std::atomic<int> TraverseDirectory::m_skippedCount{0};

namespace {

// 跳过 / 停止的路径集合：UI 线程写，遍历线程读，所以要用读写锁保护。
// 键统一成"清理过 + 小写"的形式，Windows 路径不区分大小写。
QReadWriteLock g_pathLock;
QSet<QString>  g_skippedPaths;
QSet<QString>  g_stoppedPaths;

// 默认跳过的目录名：整盘搜索时这些目录几乎不可能带标签，纯浪费 IO。
// 只按"目录名"匹配（不区分大小写），并且允许用户以后在界面上取消。
const QSet<QString> &defaultExcludedNames()
{
    static const QSet<QString> s_names = {
        QStringLiteral("node_modules"),
        QStringLiteral(".pnpm-store"),
        QStringLiteral(".npm"),
        QStringLiteral(".yarn"),
        QStringLiteral(".cache"),
        QStringLiteral(".git"),
        QStringLiteral(".svn"),
        QStringLiteral(".hg"),
        QStringLiteral("$recycle.bin"),
        QStringLiteral("system volume information"),
        QStringLiteral("__pycache__"),
        QStringLiteral(".gradle"),
        QStringLiteral(".idea"),
        QStringLiteral(".vs"),
        QStringLiteral(".venv"),
        QStringLiteral("venv")
    };
    return s_names;
}

}   // namespace


namespace {

/**
 * 是否是"重解析点"（Windows 的符号链接 / 目录联接 junction / OneDrive 占位等）。
 *
 * 必须跳过：旧实现用 QDirIterator 递归，它默认**不跟随**符号链接；改成自己递归后
 * 如果不检查，就会钻进 junction 里 —— 目标不可达时会一直等超时（实测整盘搜索
 * 卡在 E:\pnm-store 这类目录上，6 秒只扫了 3 个文件），指向父目录时还会无限递归。
 */
bool isReparsePoint(const QString &sPath)
{
#ifdef Q_OS_WIN
    const DWORD attrs = GetFileAttributesW(reinterpret_cast<const wchar_t *>(sPath.utf16()));
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return false;
    return (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    return QFileInfo(sPath).isSymLink();
#endif
}

}   // namespace

TraverseDirectory::TraverseDirectory()
{
}

QString TraverseDirectory::normalizedKey(const QString &sPath)
{
    return QDir::cleanPath(sPath).toLower();
}

bool TraverseDirectory::traverseDirectory(const QString &path, FileAction action, bool recursive)
{
    TraverseHooks hooks;
    hooks.onFile = action;
    DirOutcome outcome = DirOutcome::Complete;
    traverseRecursive(path, hooks, recursive, outcome);
    return outcome == DirOutcome::Complete || outcome == DirOutcome::Skipped;
}

bool TraverseDirectory::traverseDirectory(const QString &path, const TraverseHooks &hooks, bool recursive)
{
    DirOutcome outcome = DirOutcome::Complete;
    traverseRecursive(path, hooks, recursive, outcome);
    // 顶层被跳过也算"调用方的目的达到了"，但被停止/中断算不完整
    return outcome == DirOutcome::Complete || outcome == DirOutcome::Skipped;
}

/**
 * 递归遍历一个目录（自己实现递归，而不是 QDirIterator::Subdirectories，因为
 * 目录进度树需要知道"进入了哪个目录、哪个目录扫完了"）。
 */
bool TraverseDirectory::traverseRecursive(const QString &sDirPath, const TraverseHooks &hooks,
                                          bool bRecursive, DirOutcome &outcome)
{
    outcome = DirOutcome::Complete;

    if (m_isStop.load()) {
        outcome = DirOutcome::Stopped;
        return false;
    }

    // 符号链接 / junction：不进入（旧实现的 QDirIterator 默认也不跟随它们）
    if (isReparsePoint(sDirPath)) {
        outcome = DirOutcome::Skipped;
        if (hooks.onDirLeave)
            hooks.onDirLeave(sDirPath, DirOutcome::Skipped);
        return true;
    }

    // 用户把这棵子树整个跳过：不进入、不扫，向调用方报"已跳过"
    if (isPathSkipped(sDirPath) || (hooks.shouldSkipDir && hooks.shouldSkipDir(sDirPath))) {
        outcome = DirOutcome::Skipped;
        m_skippedCount.fetch_add(1);
        if (hooks.onDirLeave)
            hooks.onDirLeave(sDirPath, DirOutcome::Skipped);
        return true;
    }

    if (hooks.onDirEnter)
        hooks.onDirEnter(sDirPath);

    // 用户对这个子树点了"停止"：不扫文件也不再进入子目录
    if (isPathStopRequested(sDirPath)) {
        outcome = DirOutcome::Stopped;
        if (hooks.onDirLeave)
            hooks.onDirLeave(sDirPath, DirOutcome::Stopped);
        return false;
    }

    QDir dir(sDirPath);
    if (!dir.exists()) {
        outcome = DirOutcome::Failed;
        if (hooks.onDirLeave)
            hooks.onDirLeave(sDirPath, DirOutcome::Failed);
        return false;
    }

    bool bOk = true;

    // ---- 先扫本目录下的文件 ----
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::NoSort);
    for (const QFileInfo &fileInfo : files) {
        if (m_isStop.load() || isPathStopRequested(sDirPath)) {
            bOk = false;
            break;
        }
        if (hooks.onFile && !hooks.onFile(fileInfo)) {
            bOk = false;
            break;
        }
    }

    // ---- 再递归子目录；某个子目录被停止不影响兄弟目录继续扫 ----
    if (bOk && bRecursive) {
        const QFileInfoList subDirs =
            dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::NoSort);
        for (const QFileInfo &subDirInfo : subDirs) {
            if (m_isStop.load() || isPathStopRequested(sDirPath)) {
                bOk = false;
                break;
            }

            DirOutcome subOutcome = DirOutcome::Complete;
            traverseRecursive(subDirInfo.absoluteFilePath(), hooks, true, subOutcome);

            // 子目录被停止/没扫完 → 本目录整体算"不完整"，但继续扫其它兄弟目录
            if (subOutcome == DirOutcome::Stopped || subOutcome == DirOutcome::Incomplete
                || subOutcome == DirOutcome::Failed) {
                bOk = false;
            }
        }
    }

    if (m_isStop.load())
        outcome = DirOutcome::Stopped;
    else if (!bOk)
        outcome = DirOutcome::Incomplete;
    else
        outcome = DirOutcome::Complete;

    if (hooks.onDirLeave)
        hooks.onDirLeave(sDirPath, outcome);

    return outcome == DirOutcome::Complete;
}

void TraverseDirectory::stop()
{
    m_isStop = true;
}

void TraverseDirectory::resetStop()
{
    // 由调用方（各遍历/搜索线程）在 run() 开头调用一次，
    // 不要在 traverseDirectory 内部复位，否则选中多个目录时"停止"只会停当前目录、下一个目录又会继续跑。
    m_isStop = false;
}

bool TraverseDirectory::isStopped()
{
    return m_isStop.load();
}

void TraverseDirectory::requestSkipPath(const QString &sPath)
{
    QWriteLocker locker(&g_pathLock);
    g_skippedPaths.insert(normalizedKey(sPath));
}

void TraverseDirectory::requestStopPath(const QString &sPath)
{
    QWriteLocker locker(&g_pathLock);
    g_stoppedPaths.insert(normalizedKey(sPath));
}

bool TraverseDirectory::isPathSkipped(const QString &sPath)
{
    QReadLocker locker(&g_pathLock);
    return g_skippedPaths.contains(normalizedKey(sPath));
}

bool TraverseDirectory::isPathStopRequested(const QString &sPath)
{
    QReadLocker locker(&g_pathLock);
    return g_stoppedPaths.contains(normalizedKey(sPath));
}

void TraverseDirectory::resetPathControls()
{
    QWriteLocker locker(&g_pathLock);
    g_skippedPaths.clear();
    g_stoppedPaths.clear();
    m_skippedCount.store(0);
}

int TraverseDirectory::skippedPathCount()
{
    return m_skippedCount.load();
}

bool TraverseDirectory::isDefaultExcludedDirName(const QString &sDirName)
{
    if (sDirName.isEmpty())
        return false;
    return defaultExcludedNames().contains(sDirName.toLower());
}
