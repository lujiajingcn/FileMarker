#ifndef THREADSEARCH_H
#define THREADSEARCH_H

#include <QElapsedTimer>
#include <QThread>
#include <QDir>

#include "common.h"
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
    // 扫描结束时的完整结果；处理完由 FormFileBrowser::onRecvSearchResult 接手
    void sigResult(const QStringList &qLHostFiles);
    void sendProcessInfo(const QString &sProcessedFilePath);

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
};

#endif // THREADSEARCH_H
