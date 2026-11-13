#include "threadaddtagbyai.h"

#include <QProcess>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QUuid>

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

void ThreadAddTagByAI::setDirs(const QStringList &dirs)
{
    m_dirs = dirs;
}

void ThreadAddTagByAI::stopThread()
{
    TraverseDirectory::stop();
}

namespace {
/** 和脚本 extract_text() 支持的类型保持一致；其余类型不启动 Python，直接跳过。 */
bool isAiSupportedFile(const QString &sPath)
{
    static const QStringList exts = {
        QStringLiteral("txt"), QStringLiteral("csv"), QStringLiteral("json"),
        QStringLiteral("xml"), QStringLiteral("log"), QStringLiteral("md"),
        QStringLiteral("pdf"), QStringLiteral("docx"),
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
    };
    const QString sSuffix = QFileInfo(sPath).suffix().toLower();
    return exts.contains(sSuffix);
}
}   // namespace

bool ThreadAddTagByAI::recordFailure(const QString &sFilePath, const QString &sReason,
                                     const QString &sDetail)
{
    ++m_failed;
    ++m_done;
    emit sendAIProgress(m_done);
    const QString sLine = QString("%1      失败：%2%3")
                              .arg(sFilePath, sReason,
                                   sDetail.isEmpty() ? QString() : QStringLiteral("（") + sDetail + QStringLiteral("）"));
    if (m_firstError.isEmpty())
        m_firstError = sReason + (sDetail.isEmpty() ? QString() : QStringLiteral("：") + sDetail);
    emit sendProcessInfo(sLine);
    qWarning().noquote() << "AI 打标失败:" << sLine;
    // 还没成功过就已连续失败 3 个（典型：Python 依赖缺失/解释器不可用），
    // 再挨个试剩下的文件没有意义 —— 让 traverseDirectory 停下来。
    return !(m_succeeded == 0 && m_failed >= 3);
}

void ThreadAddTagByAI::setFiles(const QStringList &qLFiles)
{
    m_files = qLFiles;
}

void ThreadAddTagByAI::run()
{
    m_succeeded = 0;
    m_failed = 0;
    m_skipped = 0;
    m_done = 0;
    m_firstError.clear();

    // 每次任务开始读一次服务端配置（用户在设置里改了也能立刻生效）
    m_ai = AiSettings::load();
    if (m_ai.isApi()) {
        if (m_ai.apiBase.isEmpty()) {
            m_firstError = tr("云端 API 模式没有配置 Base URL，请在【文件 → AI 打标设置…】里填写");
            emit sendProcessInfo(m_firstError);
            emit sendFinish(m_succeeded, m_failed, m_skipped, m_firstError);
            return;
        }
        if (m_ai.textModel().isEmpty()) {
            m_firstError = tr("云端 API 模式没有配置文本模型，请在【文件 → AI 打标设置…】里填写");
            emit sendProcessInfo(m_firstError);
            emit sendFinish(m_succeeded, m_failed, m_skipped, m_firstError);
            return;
        }
    }

    auto func = [this](const QFileInfo& info) -> bool {
        return this->addTag(info);
    };

    // 先确认脚本存在：以前脚本缺失时每个文件都静默失败，界面毫无提示。
    const QString sScript = Utility::aiScriptPath();
    if (sScript.isEmpty()) {
        m_firstError = tr("找不到 AI 脚本 extract_and_tag.py（应放在程序目录或源码目录）");
        emit sendProcessInfo(m_firstError);
        emit sendFinish(m_succeeded, m_failed, m_skipped, m_firstError);
        return;
    }

    // 静态 m_isStop 标志被所有遍历线程共享。若用户此前停止过搜索/遍历操作，
    // 不在此处复位则 m_isStop 仍为 true，AI 线程会立即跳过所有目录并结束。
    TraverseDirectory::resetStop();

    // 用户明确选中了文件：只处理这些文件，不再顺手把整个文件夹扫一遍
    if (!m_files.isEmpty()) {
        for (const QString &sFile : m_files) {
            if (TraverseDirectory::isStopped())
                break;
            emit sendProcessInfo(sFile);
            if (!addTag(QFileInfo(sFile)))
                break;
            if (m_succeeded == 0 && m_failed >= 3)
                break;
        }
        emit sendFinish(m_succeeded, m_failed, m_skipped, m_firstError);
        return;
    }

    for (QString selDir : m_dirs) {
        // 已请求停止时，不再继续遍历其余目录。
        if (TraverseDirectory::isStopped())
            break;
        TraverseDirectory::traverseDirectory(selDir, func, true);
        // 连续失败且一个都没成功（典型：Python 没装、依赖缺失、解释器不可用），
        // 就不要再把成百上千个文件挨个试一遍，直接结束并把错误交给界面。
        if (m_succeeded == 0 && m_failed >= 3)
            break;
    }

    emit sendFinish(m_succeeded, m_failed, m_skipped, m_firstError);
}

QString ThreadAddTagByAI::makeApiImageCopy(const QString &sFilePath, QString *psError)
{
    QImage image(sFilePath);
    if (image.isNull()) {
        if (psError != nullptr)
            *psError = tr("图片无法读取（格式不支持或文件损坏）");
        return QString();
    }

    // 云端视觉模型不需要原分辨率：最长边 1024 足够识别内容，体积能降一两个数量级
    const int nMaxSide = 1024;
    if (image.width() > nMaxSide || image.height() > nMaxSide) {
        image = image.scaled(nMaxSide, nMaxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (image.format() != QImage::Format_RGB888)
        image = image.convertToFormat(QImage::Format_RGB888);

    const QString sTemp = QDir::tempPath() + QStringLiteral("/fm_ai_")
                          + QUuid::createUuid().toString(QUuid::WithoutBraces)
                          + QStringLiteral(".jpg");
    if (!image.save(sTemp, "JPG", 80)) {
        if (psError != nullptr)
            *psError = tr("图片压缩失败");
        return QString();
    }
    if (QFileInfo(sTemp).size() > 2 * 1024 * 1024) {
        QFile::remove(sTemp);
        if (psError != nullptr)
            *psError = tr("图片压缩后仍超过 2MB，已跳过（请先手动缩小图片）");
        return QString();
    }
    return sTemp;
}

bool ThreadAddTagByAI::addTag(const QFileInfo& info)
{
    if (info.isDir())
        return true;

    const QString filePath = info.filePath();
    if (!isAiSupportedFile(filePath)) {
        // 不支持的类型（mp4/mobi/epub/zip…）不启动 Python：以前每个都白跑一次进程，
        // 脚本对非图片返回空标签还被算成"成功"，成功率虚高。
        ++m_skipped;
        ++m_done;
        emit sendProcessInfo(QString("%1      跳过：不支持 AI 打标的文件类型").arg(filePath));
        emit sendAIProgress(m_done);
        return true;
    }

    // 只读文件写不了 ADS：跳过并给出明确原因（不改文件属性）
    if (ADSOperation::isFileReadOnly(filePath)) {
        return recordFailure(filePath,
                             tr("文件是只读的（请先取消「只读」属性）"));
    }

    // 云端 API 模式：图片先在本机缩放到 1024 像素内、压成 jpg 再上传，
    // 避免几十 MB 的原图撑爆请求体、也省 token。
    QString sUploadPath = filePath;
    QString sTempImage;
    struct TempImageGuard {
        QString path;
        ~TempImageGuard() {
            if (!path.isEmpty())
                QFile::remove(path);
        }
    } tempGuard;
    const QString sSuffix = info.suffix().toLower();
    const bool bImage = (sSuffix == QLatin1String("png") || sSuffix == QLatin1String("jpg")
                         || sSuffix == QLatin1String("jpeg"));
    if (m_ai.isApi() && bImage) {
        QString sImgErr;
        sTempImage = makeApiImageCopy(filePath, &sImgErr);
        if (sTempImage.isEmpty())
            return recordFailure(filePath, sImgErr);
        sUploadPath = sTempImage;
        tempGuard.path = sTempImage;
    }

    const QString script = Utility::aiScriptPath();
    QStringList args;
    args << script << sUploadPath;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove("PYTHONHOME");
    env.remove("PYTHONPATH");
    // 把服务端配置传给脚本（Key 只通过环境变量传，不会出现在命令行/日志里）
    env.insert(QStringLiteral("FM_AI_PROVIDER"), m_ai.isApi() ? QStringLiteral("api")
                                                             : QStringLiteral("ollama"));
    env.insert(QStringLiteral("FM_AI_TEXT_MODEL"), m_ai.textModel());
    env.insert(QStringLiteral("FM_AI_IMAGE_MODEL"), m_ai.imageModel());
    env.insert(QStringLiteral("FM_AI_TIMEOUT"), QString::number(m_ai.timeoutSec));
    if (m_ai.isApi()) {
        env.insert(QStringLiteral("FM_AI_API_BASE"), m_ai.apiBase);
        env.insert(QStringLiteral("FM_AI_API_KEY"), m_ai.apiKey);
    }

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

    ++m_succeeded;
    ++m_done;
    emit sendAIProgress(m_done);
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
