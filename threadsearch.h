#ifndef THREADSEARCH_H
#define THREADSEARCH_H

#include <QThread>
#include <QDir>

#include "common.h"

class ThreadSearch : public QThread
{
    Q_OBJECT
public:
    explicit ThreadSearch(QObject *parent = nullptr);
    void stopThread();

    void setPara(const QStringList &selDirs, const QStringList &tags, const labelLogic taglLogic);

    bool searchFile(const QFileInfo& fileInfo);

protected:
    void run();

signals:
    void sigState(bool bIsStop);
    void sigResult(QStringList qLHostFiles);
    void sendProcessInfo(QString sProcessedFilePath);

private:
    QStringList m_selDirs;
    QStringList m_adsName;
    labelLogic  m_tagLogic{labelLogic::AND};  // P1-8：初始化默认值，避免 setPara 未被调用时产生未定义行为
    QStringList m_filePaths;
};

#endif // THREADSEARCH_H
