/**
** 【目录进度统计】
**
** 供"搜索 / 遍历"两个工作线程共用：把遍历过程中每个目录的结局
** （完成 / 跳过 / 停止 / 不完整）和已扫文件数、命中数收集成节点，
** 再由 UI 线程渲染成"目录进度树"。
**
** 只显示相对搜索根的前 3 层：整盘动辄几万个目录，全量建树既慢又刷不满；
** 更深的目录不建节点，计数聚合到第 3 层祖先上。
**/
#ifndef DIRPROGRESSTRACKER_H
#define DIRPROGRESSTRACKER_H

#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>

#include "traversedirectory.h"

class DirProgressTracker
{
public:
    /** 只会统计到这个深度（相对根目录），更深的聚合到这一层 */
    static const int kMaxDisplayDepth = 3;

    /**
     * 目录节点键：cleanPath + 去掉结尾斜杠（"E:/" → "E:"）。
     *
     * 必须所有地方共用这一个函数：QFileInfo("E:/bin").path() 在 Qt 里返回的是
     * 带尾斜杠的 "E:/"，拿它直接当 map 键会查不到父节点，界面上就会出现
     * 两个 "E:\" 根节点、根节点计数也汇总不上来。
     */
    static QString dirNodeKey(const QString &sPath);

    /** 每轮开始前调用：建立根节点（状态"等待"） */
    void reset(const QStringList &qLRoots);

    /**
     * 一开始就把根目录下的子目录（到显示深度为止）全部建成"等待"节点。
     * 这样进度树一上来就是完整结构，而不是扫描过程中一个个往外蹦。
     */
    void prepopulate(const std::function<void()> &onChunkDone = std::function<void()>());

    void enterDir(const QString &sPath);
    void leaveDir(const QString &sPath, DirOutcome outcome);
    /** 扫到一个文件（bMatched 表示命中） */
    void addFile(bool bMatched);

    DirProgressSnapshot snapshot() const;
    bool isEmpty() const { return m_nodes.isEmpty(); }

private:
    /** 相对哪个根、在第几层（0 = 根本身）；不属于任何根返回 -1 */
    int levelOfPath(const QString &sPath) const;
    /** 超过显示深度时，找它对应的第 kMaxDisplayDepth 层祖先 */
    QString ancestorAtDisplayDepth(const QString &sPath) const;
    /** prepopulate() 的递归实现（把某目录及其子目录都建成"等待"节点） */
    void addTree(const QString &sDirPath, int nLevel);
    /** 只保证某个节点存在（不往下递归） */
    void ensureNode(const QString &sDirPath, int nLevel);

    QStringList m_roots;                              // 原始根路径（遍历要用，不能去斜杠）
    QString     m_sCurrentDir;                        // 当前正在扫的目录（节点键形式）
    QHash<QString, DirProgressNode> m_nodes;
};

#endif // DIRPROGRESSTRACKER_H
