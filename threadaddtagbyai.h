#ifndef THREADADDLABELBYAI_H
#define THREADADDLABELBYAI_H

#include <QThread>
#include <QFileInfo>

class ThreadAddTagByAI : public QThread
{
    Q_OBJECT

public:
    ThreadAddTagByAI(QObject *parent = nullptr);

    void setDirs(QStringList dirs);

    void stopThread();

signals:
    void sendFinish();
    void sendProcessInfo(QString sProcessedFilePath);
    void sendTags(QString tags);
    void sendSummary(QString summary);
    /** 某个文件的标签已写入磁盘，主线程据此让搜索缓存失效 */
    void sendTagChanged(QString sFilePath);

protected:
    void run();

    bool addTag(const QFileInfo& info);

private:
    QStringList     m_dirs;
};

#endif // THREADADDLABELBYAI_H
