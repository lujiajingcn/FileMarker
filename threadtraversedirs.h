#ifndef THREADTRAVERSEDIRS_H
#define THREADTRAVERSEDIRS_H

#include <QThread>
#include <QMap>

class ThreadTraverseDirs : public QThread
{
    Q_OBJECT
public:
    explicit ThreadTraverseDirs(QObject *parent = nullptr);
    void setSelDirs(QStringList qLSelDirs);
    void recvStop();

protected:
    void run();

    void searchDirectory(const QString& sDirPath, QMap<QString, QStringList> &mapFilePathAndLabel);
    void findADSName(const QString &sFilePath, QMap<QString, QStringList> &mapFilePathAndLabel);

signals:
    void sigState(bool bIsStop);
    void sigResult(QMap<QString, QMap<QString, QStringList>> mapDirAndmapHostFilesAndLabel);
    void sendProcessInfo(QString sProcessedFilePath);

private:
    bool m_bIsStop;
    QStringList m_qLSelDirs;
};

#endif // THREADTRAVERSEDIRS_H
