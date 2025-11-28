#ifndef THREADSEARCH_H
#define THREADSEARCH_H

#include <QThread>
#include "common.h"

class ThreadSearch : public QThread
{
    Q_OBJECT
public:
    explicit ThreadSearch(QObject *parent = nullptr);
    void stopThread();

    void setPara(const QStringList &selDirs, const QStringList &tags, const labelLogic taglLogic);

protected:
    void run();

    void searchDirectory(const QString& dirPath, QStringList &hostFiles);

signals:
    void sigState(bool bIsStop);
    void sigResult(QStringList qLHostFiles);
    void sendProcessInfo(QString sProcessedFilePath);

private:
    bool        m_isStop;
    QStringList m_selDirs;
    QStringList m_adsName;
    labelLogic  m_tagLogic;
};

#endif // THREADSEARCH_H
