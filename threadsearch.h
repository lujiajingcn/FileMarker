#ifndef THREADSEARCH_H
#define THREADSEARCH_H

#include <QElapsedTimer>
#include <QMutex>
#include <QThread>
#include <QDir>

#include "common.h"
#include "dirprogresstracker.h"
#include "paralleldirtraverser.h"
#include "traversedirectory.h"
#include <QHash>
#include <QSet>

class ThreadSearch : public QThread
{
    Q_OBJECT
public:
    explicit ThreadSearch(QObject *parent = nullptr);
    void stopThread();

    // 传入的是"标签组"（每组 = 选中标签 + 其全部子标签），组内 OR、组间按 logic 组合
    void setPara(const QStringList &selDirs, const QList<QSet<QString>> &qLLabelGroups, const labelLogic taglLogic);

    bool searchFile(const QFileInfo& fileInfo);

protected:
    void run();

signals:
    // 扫描结束时的完整结果；处理完由 FormFileBrowser::onRecvSearchResult 接手。
    // bCompleted=false 表示中途被用户停止（结果不完整，界面必须提示）。
    void sigResult(const QStringList &qLHostFiles, bool bCompleted);
    void sendProcessInfo(const QString &sProcessedFilePath);
    // 目录进度树快照（每 150ms 一次，避免把 UI 线程灌满）
    void sendDirSnapshot(const DirProgressSnapshot &snapshot);

private:
    QStringList m_selDirs;
    QList<QSet<QString>> m_labelGroups;
    labelLogic  m_tagLogic{labelLogic::AND};  // 初始化默认值，避免 setPara 未被调用时产生未定义行为
    QStringList m_filePaths;
    int         m_scanned = 0;   // 诊断用：本次扫描了多少个文件
    // 进度信号节流：每个文件都 emit 一次会把 UI 线程的事件队列灌满（几十万文件时
    // 界面会一直"卡"），改成最多每 150ms 报一次，并带上已扫描计数。
    QElapsedTimer m_progressTimer;
    int           m_progressFiles = 0;

    // ---- 目录进度树 ----
    // 统计逻辑在 DirProgressTracker 里（遍历线程也用同一份），这里只负责发快照。
    void emitDirSnapshot();

    DirProgressTracker m_dirTracker;
    // 并行遍历：多个工作线程会同时回调 searchFile，共享数据都要加锁
    QMutex             m_mutex;
};

#endif // THREADSEARCH_H
