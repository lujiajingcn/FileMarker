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

    void setDirs(QStringList dirs);

    void stopThread();

signals:
    // 【D5 修复】结束信号带上统计与首个错误，UI 才能提示"AI 根本没跑起来"，
    // 而不是像以前那样静默回到文件列表。
    void sendFinish(int nSucceeded, int nFailed, QString sFirstError);
    void sendProcessInfo(QString sProcessedFilePath);
    void sendTags(QString tags);
    void sendSummary(QString summary);
    /** 某个文件的标签已写入磁盘，主线程据此让搜索缓存失效 */
    void sendTagChanged(QString sFilePath);

protected:
    void run();

    bool addTag(const QFileInfo& info);

private:
    // 记录一次失败：计数 + 只保留第一条错误 + 输出到进度页
    bool recordFailure(const QString &sFilePath, const QString &sReason, const QString &sDetail = QString());

    QStringList     m_dirs;
    int             m_nSucceeded = 0;
    int             m_nFailed = 0;
    QString         m_sFirstError;
};

#endif // THREADADDLABELBYAI_H
