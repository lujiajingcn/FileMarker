/**
** 【并行目录遍历】
**
** 把"逐目录递归"拆成任务丢进线程池，多个目录同时扫。
** 整盘搜索/遍历是纯 IO + 少量 CPU，SSD 上并行能明显摊薄单目录的等待。
**
** 语义和单线程版保持一致：
**   - 用户跳过/停止的子树整棵不扫（决策仍在 TraverseDirectory 的控制集合里）；
**   - 符号链接 / junction 不进入；
**   - 回调 onFile 返回 false 或全局 stop() 时，尽快收尾并返回 false。
**
** 线程安全：回调可能被多个线程同时调用，调用方自己加锁
** （搜索/遍历线程各自用一个 QMutex 保护共享容器）。
**/
#ifndef PARALLELDIRTRAVERSER_H
#define PARALLELDIRTRAVERSER_H

#include <QFileInfo>
#include <QString>
#include <QStringList>

#include <functional>

#include "traversedirectory.h"

struct ParallelTraverseCallbacks
{
    std::function<bool(const QFileInfo &)> onFile;                  // 返回 false = 中断
    std::function<void(const QString &)> onDirEnter;                // 进入目录
    std::function<void(const QString &, DirOutcome)> onDirLeave;    // 离开目录
    std::function<bool(const QString &)> shouldSkipDir;             // true = 整棵子树跳过
};

class ParallelDirTraverser
{
public:
    /**
     * 并行遍历多个根目录。
     * @param nThreads 线程数（<=1 时退化为单线程，便于对比/调试）
     * @return true 表示全部扫完；false 表示被停止/中断（结果不完整）
     */
    static bool run(const QStringList &qLRoots, int nThreads,
                    const ParallelTraverseCallbacks &callbacks);

    /**
     * 根据目标盘的类型决定线程数：
     *   - SSD/NVMe：4 个（随机 IO 便宜，能真正并行）
     *   - 机械盘（seek penalty）：1~2 个（并行只会让磁头来回跳，反而更慢）
     * 判定失败时保守用 2。
     */
    static int defaultThreadCount(const QString &sSamplePath = QString());

    /**
     * 按"路径所在卷"决定线程数，并按盘符缓存判定结果。
     * 搜索时每个根目录各问一次：C:(SSD) 用 4 线程，E:(机械盘) 用 1 线程。
     */
    static int threadCountForPath(const QString &sPath);

    /** 该路径所在的卷是否是"需要寻道"的机械盘（判定失败返回 false） */
    static bool incursSeekPenalty(const QString &sPath);
};

#endif // PARALLELDIRTRAVERSER_H
