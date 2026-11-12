/**
** 【遍历文件夹类】
**
** 遍历的文件夹可以是单个或者多个
** 遍历文件夹，将文件与关联的ADS文件名（即该文件的标签）存储为map返回，将结果存入数据库，作为下次查询的缓存。
** 将文件夹下所有的ADS文件名（即标签），存储为set返回，在【当前目录文件的标签类】中显示。
**/

#ifndef THREADTRAVERSEDIRS_H
#define THREADTRAVERSEDIRS_H

#include <QElapsedTimer>
#include <QMutex>
#include <QThread>
#include <QMap>
#include <QSet>
#include <QDir>

#include "common.h"
#include "dirprogresstracker.h"
#include "paralleldirtraverser.h"

Q_DECLARE_METATYPE(FILE_TAGS);// sendResult传递的参数中有FILE_TAGS类型数据，不声明则槽函数不响应

class ThreadTraverseDirs : public QThread
{
    Q_OBJECT
public:
    explicit ThreadTraverseDirs(QObject *parent = nullptr);

    void setSelDirs(const QStringList &selDirs);
    void stopThread();

protected:
    void run();
    bool traverseDir(const QFileInfo& fileInfo);

signals:
    // 结果里只包含"完整扫完"的目录；qLCompletedDirs 供上层登记 dirs 表（空目录也要登记），
    // bAborted 为 true 表示用户中途点了停止，此时未扫完的目录不会出现在任何结果里。
    // 目录进度树快照（遍历也用同一套进度组件）
    void sendDirSnapshot(const DirProgressSnapshot &snapshot);
    void sendResult(const QMap<QString, FILE_TAGS> &dirAndFileTags,
                    QMap<QString, QSet<QString>> dirAndTags,
                    const QStringList &qLCompletedDirs,
                    const QStringList &qLSkippedDirs,
                    const QStringList &qLIncompleteDirs,
                    bool bAborted);
    void sendProcessInfo(const QString &sProcessedFilePath);

private:
    QStringList        m_selDirs;
    FILE_TAGS          m_fileTags;
    QSet<QString>      m_tags;
    DirProgressTracker m_dirTracker;     // 目录进度统计（和搜索共用逻辑）
    QMutex             m_mutex;          // 并行遍历时保护上面的容器
    QElapsedTimer      m_snapshotTimer;  // 快照节流：最多每 150ms 发一次
    /** 节流发一次目录进度快照 */
    void emitSnapshotIfDue();
};

#endif // THREADTRAVERSEDIRS_H
