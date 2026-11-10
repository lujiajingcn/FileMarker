#include "threadaddtagbyai.h"

#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

#include "common.h"
#include "traversedirectory.h"
#include "adsoperation.h"
#include "utility.h"

namespace {
// 把脚本 stderr / 进程错误压成一行，便于写进进度页与日志
QString oneLine(const QByteArray &data, int nMax = 300)
{
    QString s = QString::fromLocal8Bit(data).trimmed();
    if (s.isEmpty())
        return QString();
    s.replace(QLatin1Char('\r'), QLatin1Char(' '));
    s.replace(QLatin1Char('\n'), QLatin1Char(' '));
    s.replace(QLatin1Char('\t'), QLatin1Char(' '));
    s = s.simplified();
    if (s.size() > nMax)
        s = s.left(nMax) + QStringLiteral("…");
    return s;
}
}   // namespace

ThreadAddTagByAI::ThreadAddTagByAI(QObject *parent) : QThread(parent)
{
    // 与 ThreadSearch/ThreadTraverseDirs 保持一致，在构造函数内部连接
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

bool ThreadAddTagByAI::recordFailure(const QString &sFilePath, const QString &sReason,
                                     const QString &sDetail)
{
    ++m_nFailed;
    const QString sLine = QString("%1      失败：%2%3")
                              .arg(sFilePath, sReason,
                                   sDetail.isEmpty() ? QString() : QStringLiteral("（") + sDetail + QStringLiteral("）"));
    if (m_sFirstError.isEmpty())
        m_sFirstError = sReason + (sDetail.isEmpty() ? QString() : QStringLiteral("：") + sDetail);
    emit sendProcessInfo(sLine);
    qWarning().noquote() << "AI 打标失败:" << sLine;
    // 还没成功过就已连续失败 3 个（典型：Python 依赖缺失/解释器不可用），
    // 再挨个试剩下的文件没有意义 —— 让 traverseDirectory 停下来。
    return !(m_nSucceeded == 0 && m_nFailed >= 3);
}

void ThreadAddTagByAI::run()
{
    m_nSucceeded = 0;
    m_nFailed = 0;
    m_sFirstError.clear();

    auto func = [this](const QFileInfo& info) -> bool {
        return this->addTag(info);
    };

    // 先确认脚本存在：以前脚本缺失时每个文件都静默失败，界面毫无提示。
    const QString sScript = g_sAppDir + "/extract_and_tag.py";
    if (!QFileInfo::exists(sScript)) {
        m_sFirstError = tr("找不到 AI 脚本：%1（请把 extract_and_tag.py 与程序放在同一目录）").arg(sScript);
        emit sendProcessInfo(m_sFirstError);
        emit sendFinish(m_nSucceeded, m_nFailed, m_sFirstError);
        return;
    }

    // 静态 m_bIsStop 标志被所有遍历线程共享。若用户此前停止过搜索/遍历操作，
    // 不在此处复位则 m_bIsStop 仍为 true，AI 线程会立即跳过所有目录并结束。
    TraverseDirectory::resetStop();

    for (QString selDir : m_dirs) {
        // 已请求停止时，不再继续遍历其余目录。
        if (TraverseDirectory::isStopped())
            break;
        TraverseDirectory::traverseDirectory(selDir, func, true);
        // 连续失败且一个都没成功（典型：Python 没装、依赖缺失、解释器不可用），
        // 就不要再把成百上千个文件挨个试一遍，直接结束并把错误交给界面。
        if (m_nSucceeded == 0 && m_nFailed >= 3)
            break;
    }

    emit sendFinish(m_nSucceeded, m_nFailed, m_sFirstError);
}

bool ThreadAddTagByAI::addTag(const QFileInfo& info)
{
    if (info.isDir())
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
        recordFailure(filePath, tr("未找到可用的 Python 解释器（python/python3/py）"));
        return false;   // 解释器都没有，继续试其它文件没有意义
    }

    // 轮询等待，使“停止”按钮能及时生效；单文件最长 2 分钟，超时则终止并明确报告。
    bool finished = false;
    int waited = 0;
    const int pollMs = 500;
    const int maxMs = 120000;
    while (!finished) {
        if (TraverseDirectory::isStopped()) {
            proc.kill();
            proc.waitForFinished(3000);     // 回收子进程，避免 QProcess 析构告警
            emit sendProcessInfo(QString("%1      已停止").arg(filePath));
            return false;
        }
        if (proc.waitForFinished(pollMs)) {
            finished = true;
        } else if ((waited += pollMs) >= maxMs) {
            proc.kill();
            proc.waitForFinished(3000);
            return recordFailure(filePath, tr("处理超时（>120 秒），已跳过"));
        }
    }

    if (proc.exitStatus() != QProcess::NormalExit) {
        return recordFailure(filePath, tr("脚本异常退出"), oneLine(proc.readAllStandardError()));
    }

    QByteArray out = proc.readAllStandardOutput();
    QByteArray err = proc.readAllStandardError();
    const QString sErr = oneLine(err);
    if (!sErr.isEmpty())
        qWarning() << "extract_and_tag.py stderr:" << err;

    // 以前只要退出码是 NormalExit 就当成成功，于是"缺依赖的 traceback（退出码 1）"
    // 被完全吞掉。现在把非 0 退出码、空输出、JSON error 字段都当成失败上报。
    if (proc.exitCode() != 0) {
        return recordFailure(filePath, tr("脚本返回错误（退出码 %1）").arg(proc.exitCode()), sErr);
    }
    if (out.isEmpty()) {
        return recordFailure(filePath, tr("脚本没有输出任何结果"), sErr);
    }

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
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        return recordFailure(filePath, tr("脚本输出无法解析为 JSON"), oneLine(out));
    }

    QJsonObject obj = doc.object();
    // 脚本自己报告的失败（如模型未安装）以前被直接丢弃，现在原样上报。
    const QString sScriptError = obj.value("error").toString();
    if (!sScriptError.isEmpty()) {
        return recordFailure(filePath, sScriptError);
    }

    QStringList listTags;
    QString tags;
    if (obj.contains("tags") && obj.value("tags").isArray()) {
        QJsonArray arr = obj.value("tags").toArray();
        for (auto v : arr) {
            // 模型可能把多个标签写在一个数组元素里（中文 comma/分号等），
            // 按统一规则再拆一次，避免把 "a，b" 整体写成一个标签。
            const QStringList tagsInItem = Utility::splitLabels(v.toString());
            for (const QString &sTag : tagsInItem) {
                // 按磁盘流名规则先净化，避免库/磁盘出现 "C++:语言" vs "C++_语言"
                const QString sSanitized = ADSOperation::sanitizeTagName(sTag);
                if (!sSanitized.isEmpty() && !listTags.contains(sSanitized))
                    listTags.append(sSanitized);
            }
        }
        // 追加到已有标签之后，保留文件原有标签顺序（AI 生成的标签按模型给出的顺序排在其后）
        const bool bOk = ADSOperation::appendTags(filePath, listTags);
        emit sendTagChanged(filePath);
        if (!bOk) {
            return recordFailure(filePath, tr("标签写入不完整（已有标签未丢失，请检查占用/权限）"));
        }
        tags = listTags.join(",");
        if (!tags.isEmpty())
            emit sendTags(tags);
    }

    ++m_nSucceeded;
    // 之前解析出 summary 却直接丢弃，这里通过信号上报，不再浪费模型输出。
    if (obj.contains("summary")) {
        QString sSummary = obj.value("summary").toString();
        if (!sSummary.isEmpty())
            emit sendSummary(sSummary);
    }

    QString sProcessInfo = QString("%1      %2")
                               .arg(filePath, tags.isEmpty() ? tr("（模型未给出标签）") : tags);
    emit sendProcessInfo(sProcessInfo);

    return true;
}
