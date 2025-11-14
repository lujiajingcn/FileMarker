/**
** 【遍历文件夹类】
**
** 遍历的文件夹可以是单个或者多个
** 遍历文件夹，将文件与关联的ADS文件名（即该文件的标签）存储为map返回，将结果存入数据库，作为下次查询的缓存。
** 将文件夹下所有的ADS文件名（即标签），存储为set返回，在【当前目录文件的标签类】中显示。
**/

#ifndef THREADTRAVERSEDIRS_H
#define THREADTRAVERSEDIRS_H

#include <QThread>
#include <QMap>
#include <QSet>

class ThreadTraverseDirs : public QThread
{
    Q_OBJECT
public:
    explicit ThreadTraverseDirs(QObject *parent = nullptr);
    void setSelDirs(QStringList qLSelDirs);
    void recvStop();

protected:
    void run();

    void searchDirectory(const QString& sDirPath, QMap<QString, QStringList> &mapFilePathAndLabel, QSet<QString> &setLabels);
    void findADSName(const QString &sFilePath, QMap<QString, QStringList> &mapFilePathAndLabel, QSet<QString> &setLabels);

signals:
    void sigState(bool bIsStop);
    void sigResult(QMap<QString, QMap<QString, QStringList>> mapDirAndmapHostFilesAndLabel);
    void sendDirAndLabel(QMap<QString, QSet<QString>> mapDirAndLabel);
    void sendProcessInfo(QString sProcessedFilePath);

private:
    bool m_bIsStop;
    QStringList m_qLSelDirs;
};

#endif // THREADTRAVERSEDIRS_H
