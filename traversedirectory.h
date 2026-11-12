/**
** 【遍历文件夹】
**
** 遍历指定的文件夹，并执行传入的回调函数
**
** 支持两种粒度：
**   1. 只关心文件（旧接口，搜索/遍历/AI 打标签都能用）；
**   2. 关心"目录开始/目录结束"（目录进度树需要它，才能显示每个文件夹的状态）。
**
** 另外提供"只停止某个子树"和"跳过某个子树"的能力：
** 以前的 stop() 是全局的，要么全停要么全等；整盘搜索时用户想跳过
** node_modules 这种目录就没办法。
**/

#ifndef TRAVERSEDIRECTORY_H
#define TRAVERSEDIRECTORY_H

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QString>
#include <QVector>
#include <QMetaType>

#include <atomic>
#include <functional>

using FileAction = std::function<bool(const QFileInfo&)>;

/** 一个目录扫完时的结果，目录进度树靠它显示状态 */
enum class DirOutcome
{
    Complete,    // 完整扫完
    Skipped,     // 用户要求跳过（整棵子树不扫），不算"没扫完"
    Stopped,     // 用户要求停止这个子树
    Incomplete,  // 有子目录被停止/出错，或者全局停止
    Failed       // 无法读取（权限等）
};

/** 目录进度树上每个节点的状态 */
enum class DirNodeState
{
    Waiting = 0,   // 还没轮到
    Searching,     // 正在扫
    Complete,      // 扫完了
    Stopped,       // 用户停止了这个子树
    Skipped,       // 用户跳过了这个子树
    Incomplete     // 有子目录被停止/读不了，整体不完整
};

/** 目录进度树的一个节点（工作线程算好、UI 线程只负责显示） */
struct DirProgressNode
{
    QString path;         // 目录绝对路径
    int     level = 0;    // 相对搜索根的层级：0 = 搜索根
    int     files = 0;    // 已扫描文件数（含被聚合的更深层）
    int     matches = 0;  // 命中数
    int     subDirs = 0;  // 被聚合进来的深层子目录数（>0 时界面上提示"含 N 个子目录"）
    DirNodeState state = DirNodeState::Waiting;
};
using DirProgressSnapshot = QVector<DirProgressNode>;

Q_DECLARE_METATYPE(DirProgressNode)
Q_DECLARE_METATYPE(DirProgressSnapshot)

/** 目录级遍历钩子；不需要的部分留空即可 */
struct TraverseHooks
{
    FileAction                            onFile;        // 每扫到一个文件调用一次；返回 false 表示中断
    std::function<void(const QString &)>  onDirEnter;    // 进入一个目录（进度树用来高亮"正在搜索"）
    std::function<void(const QString &, DirOutcome)> onDirLeave;  // 离开一个目录（带结果）
    std::function<bool(const QString &)>  shouldSkipDir; // 返回 true = 整棵子树跳过
};

class TraverseDirectory
{
public:
    TraverseDirectory();

    /**
     * 遍历指定目录并执行回调（旧接口，不带目录事件）。
     * @return true  = 目录被完整遍历（结果可以安全地写入缓存）
     *         false = 中途被 stop() 或回调要求中断（结果不完整，不能写缓存）
     */
    static bool traverseDirectory(const QString& path, FileAction action, bool recursive = false);

    /**
     * 带目录事件的遍历：每进入/离开一个目录都会回调，用来驱动目录进度树。
     * 单目录被"跳过/停止"不会影响兄弟目录，只会让上层目录标记为不完整。
     */
    static bool traverseDirectory(const QString &path, const TraverseHooks &hooks, bool recursive = false);

    // ---- 全局停止 ----
    static void stop();
    static void resetStop();
    static bool isStopped();

    // ---- 单个子树：跳过 / 停止（线程安全，UI 线程写、遍历线程读）----
    static void requestSkipPath(const QString &sPath);
    static void requestStopPath(const QString &sPath);
    static bool isPathSkipped(const QString &sPath);
    static bool isPathStopRequested(const QString &sPath);
    /** 每轮搜索/遍历开始前调用，清空上一轮遗留的跳过/停止请求 */
    static void resetPathControls();
    /** 本轮被跳过的目录数（写日志/结果提示用） */
    static int skippedPathCount();

    /**
     * 常见的"没有意义"目录名：整盘搜索默认跳过，避免把时间花在
     * node_modules / .git 这些几乎不可能带标签的目录上。
     * 只按目录名匹配，不区分大小写。
     */
    static bool isDefaultExcludedDirName(const QString &sDirName);

private:
    static bool traverseRecursive(const QString &sDirPath, const TraverseHooks &hooks,
                                  bool bRecursive, DirOutcome &outcome);
    static QString normalizedKey(const QString &sPath);

    // 使用原子变量，避免多线程并发读写导致未定义行为。
    static std::atomic<bool>    m_isStop;
    static int                  m_skippedCount;
};

#endif // TRAVERSEDIRECTORY_H
