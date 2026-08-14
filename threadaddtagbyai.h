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

protected:
    void run();

    bool addTag(const QFileInfo& info);

private:
    QStringList     m_dirs;
};

#endif // THREADADDLABELBYAI_H
