#ifndef THREADSEARCH_H
#define THREADSEARCH_H

#include <QThread>

class ThreadSearch : public QThread
{
    Q_OBJECT
public:
    explicit ThreadSearch(QObject *parent = nullptr);
    void stopThread();

    void setPara(const QStringList &sSelDirs, const QStringList &adsName, const int nLabelLogic);

protected:
    void run();
    /** 根据标签和标签的逻辑关系，判断给定的文件是否是目标文件 */
    bool isHostFile(const QString &sFilePath);

    void searchDirectory(const QString& dirPath, QStringList &qLHostFiles);

signals:
    void sigState(bool bIsStop);
    void sigResult(QStringList qLHostFiles);
    void sendProcessInfo(QString sProcessedFilePath);

private:
    bool m_bIsStop;
    QStringList m_qLSelDirs;
    QStringList m_adsName;
    int m_nLabelLogic;
};

#endif // THREADSEARCH_H
