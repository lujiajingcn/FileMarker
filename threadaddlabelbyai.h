#ifndef THREADADDLABELBYAI_H
#define THREADADDLABELBYAI_H

#include <QThread>
#include <QFileInfo>

struct FileResult {
    QString sFilePath;
    QString sType;
    QStringList qLTags;
    QString sSummary;
    QString sStatus;
};

Q_DECLARE_METATYPE(FileResult)

class ThreadAddLabelByAI : public QThread
{
    Q_OBJECT

public:
    ThreadAddLabelByAI(QObject *parent = nullptr);

    void setDirs(QStringList qLDirs);

    void stopThread();

signals:
//    void sendResult(const FileResult& res);
    void sendFinish();
    void sendProcessInfo(QString sProcessedFilePath);

protected:
    void run();

    bool addTag(const QFileInfo& info);

private:
    QStringList     m_qLDirs;
};

#endif // THREADADDLABELBYAI_H
