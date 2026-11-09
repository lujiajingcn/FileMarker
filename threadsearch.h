#ifndef THREADSEARCH_H
#define THREADSEARCH_H

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
    void sigState(bool bIsStop);
    void sigResult(QStringList qLHostFiles);
    void sendProcessInfo(QString sProcessedFilePath);

private:
    QStringList m_selDirs;
    QList<QSet<QString>> m_labelGroups;
    labelLogic  m_tagLogic{labelLogic::AND};  // 初始化默认值，避免 setPara 未被调用时产生未定义行为
    QStringList m_filePaths;
    int         m_nScanned = 0;   // 诊断用：本次扫描了多少个文件
};

#endif // THREADSEARCH_H
