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
    // P3-27：与 ThreadSearch/ThreadTraverseDirs 保持一致，在构造函数内部连接
    // finished→deleteLater，避免未来调用者遗漏导致线程对象泄漏。
    connect(this, &ThreadAddTagByAI::finished, this, &QObject::deleteLater);
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

    // P1-6：静态 m_bIsStop 标志被所有遍历线程共享。若用户此前停止过搜索/遍历操作，
    // 不在此处复位则 m_bIsStop 仍为 true，AI 线程会立即跳过所有目录并结束。
    TraverseDirectory::resetStop();

    foreach (QString selDir , m_dirs) {
        // 已请求停止时，不再继续遍历其余目录。
        if (TraverseDirectory::isStopped())
            break;
        TraverseDirectory::traverseDirectory(selDir, func, true);
    }

    emit sendFinish();
}

bool ThreadAddTagByAI::addTag(const QFileInfo& info)
{
    if(info.isDir())
        return true;

    QString filePath = info.filePath();

    QString script = g_sAppDir + "/extract_and_tag.py";
    QStringList args;
    args << script << filePath;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("PYTHONHOME");
    env.remove("PYTHONPATH");

    // 先后尝试常见解释器名，避免环境里只有 python3 / py 而没有 python 时静默跳过。
    static const QStringList interpreters = { "python", "python3", "py" };
    QProcess proc;
    proc.setProcessEnvironment(env);

    QString prog;
    for (const QString& cand : interpreters) {
        proc.start(cand, args);
        if (proc.waitForStarted(3000)) {
            prog = cand;
            break;
        }
        proc.close();
    }
    if (prog.isEmpty()) {
        emit sendProcessInfo(QString("%1      跳过：未找到 Python 解释器（python/python3/py）").arg(filePath));
        return true;
    }

    // 轮询等待，使“停止”按钮能及时生效；单文件最长 2 分钟，超时则终止并明确报告，不再静默丢弃。
    bool finished = false;
    int waited = 0;
    const int pollMs = 500;
    const int maxMs = 120000;
    while (!finished) {
        if (TraverseDirectory::isStopped()) {
            proc.kill();
            emit sendProcessInfo(QString("%1      已停止").arg(filePath));
            return true;
        }
        if (proc.waitForFinished(pollMs)) {
            finished = true;
        } else if ((waited += pollMs) >= maxMs) {
            proc.kill();
            emit sendProcessInfo(QString("%1      超时（>120s），已跳过").arg(filePath));
            return true;
        }
    }

    if (proc.exitStatus() != QProcess::NormalExit) {
        emit sendProcessInfo(QString("%1      脚本异常退出").arg(filePath));
        return true;
    }

    QByteArray out = proc.readAllStandardOutput();
    QByteArray err = proc.readAllStandardError();
    if (!err.isEmpty())
        qWarning() << "extract_and_tag.py stderr:" << err;
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
        // #10：之前解析出 summary 却直接丢弃，这里通过信号上报，不再浪费模型输出。
        if (obj.contains("summary")) {
            QString sSummary = obj.value("summary").toString();
            if (!sSummary.isEmpty())
                emit sendSummary(sSummary);
        }
    }

    QString sProcessInfo = QString("%1      %2").arg(filePath).arg(tags);
    emit sendProcessInfo(sProcessInfo);

    return true;
}
