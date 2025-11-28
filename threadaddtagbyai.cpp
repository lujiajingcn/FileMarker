#include "threadaddtagbyai.h"

#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "common.h"
#include "traversedirectory.h"
#include "adsoperation.h"

ThreadAddTagByAI::ThreadAddTagByAI(QObject *parent) : QThread(parent)
{

}

void ThreadAddTagByAI::setDirs(QStringList dirs)
{
    m_dirs = dirs;
}

void ThreadAddTagByAI::stopThread()
{
    TraverseDirectory::stop();
}

void ThreadAddTagByAI::run()
{
    auto func = [this](const QFileInfo& info) -> bool {
        return this->addTag(info);
    };

    foreach (QString selDir , m_dirs)
        TraverseDirectory::traverseDirectory(selDir, func, true);

    emit sendFinish();
}

bool ThreadAddTagByAI::addTag(const QFileInfo& info)
{
    if(info.isDir())
        return true;

    QString filePath = info.filePath();

    QString script = g_sAppDir + "/extract_and_tag.py";
    QProcess proc;
    QStringList args;
    args << script << filePath;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("PYTHONHOME");
    env.remove("PYTHONPATH");
    proc.setProcessEnvironment(env);

    proc.start("python", args);
    bool ok = proc.waitForFinished(60000); // 60s timeout, adjust as needed
    if (!ok)
        return true;

    QByteArray out = proc.readAllStandardOutput();
    QByteArray err = proc.readAllStandardError();
    Q_UNUSED(err);
    if (out.isEmpty())
        return true;

    // Parse JSON
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(out, &perr);
    if (perr.error != QJsonParseError::NoError) {
        QString s = QString::fromUtf8(out).trimmed();
        int idx = s.indexOf('{');
        if (idx >= 0)
            s = s.mid(idx);
        doc = QJsonDocument::fromJson(s.toUtf8(), &perr);
    }

    QStringList listTags;
    QString tags;
    if (perr.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("tags") && obj.value("tags").isArray()) {
            QJsonArray arr = obj.value("tags").toArray();
            for (auto v : arr) {
                ADSOperation::writeADSFile(filePath, v.toString(), "");
                listTags.append(v.toString());
            }
            tags = listTags.join(",");
            if(!tags.isEmpty())
                emit sendTags(tags);
        }
        if (obj.contains("summary"))
            QString sSummary = obj.value("summary").toString();
    }

    QString sProcessInfo = QString("%1      %2").arg(filePath).arg(tags);
    emit sendProcessInfo(sProcessInfo);

    return true;
}
