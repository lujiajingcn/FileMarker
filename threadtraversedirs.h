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

#include "common.h"

Q_DECLARE_METATYPE(FILE_TAGS);// sendResult传递的参数中有FILE_TAGS类型数据，不声明则槽函数不响应

class ThreadTraverseDirs : public QThread
{
    Q_OBJECT
public:
    explicit ThreadTraverseDirs(QObject *parent = nullptr);

    void setSelDirs(QStringList selDirs);
    void stopThread();

protected:
    void run();

    void searchDirectory(const QString& sDirPath, FILE_TAGS &fileTags, QSet<QString> &setTags);

signals:
    void sendResult(QMap<QString, FILE_TAGS> dirAndFileTags, QMap<QString, QSet<QString>> dirAndTags);
    void sendProcessInfo(QString sProcessedFilePath);

private:
    bool        m_isStop;
    QStringList m_selDirs;
};

#endif // THREADTRAVERSEDIRS_H
