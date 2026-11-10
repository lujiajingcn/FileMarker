#ifndef THREADADDLABELBYAI_H
#define THREADADDLABELBYAI_H

#include <QThread>
#include <QFileInfo>
#include <QStringList>

class ThreadAddTagByAI : public QThread
{
    Q_OBJECT

public:
    ThreadAddTagByAI(QObject *parent = nullptr);

    void setDirs(const QStringList &dirs);

    void stopThread();

signals:
    // 结束信号带上统计与首个错误，UI 才能提示"AI 根本没跑起来"，
    // 而不是像以前那样静默回到文件列表。
    void sendFinish(int nSucceeded, int nFailed, const QString &sFirstError);
    void sendProcessInfo(const QString &sProcessedFilePath);
    void sendTags(const QString &tags);
    void sendSummary(const QString &summary);
    /** 某个文件的标签已写入磁盘，主线程据此让搜索缓存失效 */
    void sendTagChanged(const QString &sFilePath);

protected:
    void run();

    bool addTag(const QFileInfo& info);

private:
    // 记录一次失败：计数 + 只保留第一条错误 + 输出到进度页
    bool recordFailure(const QString &sFilePath, const QString &sReason, const QString &sDetail = QString());

    QStringList     m_dirs;
    int             m_succeeded = 0;
    int             m_failed = 0;
    QString         m_firstError;
};

#endif // THREADADDLABELBYAI_H
