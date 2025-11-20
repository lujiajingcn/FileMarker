#include "threadaddlabelbyai.h"

#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "common.h"
#include "traversedirectory.h"
#include "adsoperation.h"

ThreadAddLabelByAI::ThreadAddLabelByAI(QObject *parent) : QThread(parent)
{

}

void ThreadAddLabelByAI::setDirs(QStringList qLDirs)
{
    m_qLDirs = qLDirs;
}

void ThreadAddLabelByAI::stopThread()
{
    TraverseDirectory::stop();
}

void ThreadAddLabelByAI::run()
{
    auto func = [this](const QFileInfo& info) -> bool {
        return this->addTag(info);
    };

    foreach (QString sSelDir , m_qLDirs)
    {
        TraverseDirectory::traverseDirectory(sSelDir, func, true);
    }

    emit sendFinish();
}

bool ThreadAddLabelByAI::addTag(const QFileInfo& info)
{
    if(info.isDir())
    {
        return true;
    }

    QString sFilePath = info.filePath();

    emit sendProcessInfo(sFilePath);

    QString sScript = g_sAppDir + "/extract_and_tag.py";
    QProcess proc;
    QStringList args;
    args << sScript << sFilePath;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("PYTHONHOME");
    env.remove("PYTHONPATH");
    proc.setProcessEnvironment(env);

    proc.start("python", args);
    bool ok = proc.waitForFinished(60000); // 60s timeout, adjust as needed
    if (!ok)
    {
        return true;
    }
    QByteArray out = proc.readAllStandardOutput();
    QByteArray err = proc.readAllStandardError();
    Q_UNUSED(err);
    if (out.isEmpty())
    {
        return true;
    }

    // Parse JSON
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(out, &perr);
    if (perr.error != QJsonParseError::NoError)
    {
        QString s = QString::fromUtf8(out).trimmed();
        int idx = s.indexOf('{');
        if (idx >= 0) s = s.mid(idx);
        doc = QJsonDocument::fromJson(s.toUtf8(), &perr);
    }

    if (perr.error == QJsonParseError::NoError && doc.isObject())
    {
        QJsonObject obj = doc.object();
        if (obj.contains("tags") && obj.value("tags").isArray())
        {
            QJsonArray arr = obj.value("tags").toArray();
            for (auto v : arr)
            {
                ADSOperation::writeADS(sFilePath, v.toString(), "");
            }
        }
        if (obj.contains("summary"))
        {
            QString sSummary = obj.value("summary").toString();
        }
    }

    return true;
}
