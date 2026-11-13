#include "threadaiprepare.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QThread>

#include <atomic>

namespace {

const int kPythonInstallTimeoutMs = 20 * 60 * 1000;   // 静默安装 Python 最多等 20 分钟
const int kOllamaInstallTimeoutMs = 20 * 60 * 1000;   // 静默安装 Ollama 最多等 20 分钟
const int kPipTimeoutMs = 20 * 60 * 1000;             // pip 安装依赖库
const int kModelPullTimeoutMs = 0;                    // 模型下载不限时（用户可停止）

const char kPythonInstallerUrl[] =
    "https://www.python.org/ftp/python/3.12.7/python-3.12.7-amd64.exe";
const char kOllamaInstallerUrl[] = "https://ollama.com/download/OllamaSetup.exe";

/**
 * 清理一行命令输出：
 * - 去掉 ollama 进度条带的 ANSI 转义序列（ESC[?2026h、ESC[K、光标移动等），
 *   否则日志区里全是乱码；
 * - 去掉其它不可见控制字符，并把长度限制在合理范围内。
 */
QString clampLine(const QString &sLine)
{
    static const QRegularExpression ansi(QStringLiteral("\x1b\[[0-9;?]*[A-Za-z]"));
    static const QRegularExpression controlChars(QStringLiteral("[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]"));

    QString s = sLine;
    s.remove(ansi);
    s.remove(controlChars);
    s = s.trimmed();
    if (s.size() > 300)
        s = s.left(300) + QStringLiteral("…");
    return s;
}

}   // namespace

ThreadAIPrepare::ThreadAIPrepare(QObject *parent)
    : QThread(parent)
{
    m_ai = AiSettings::load();
    m_sTextModel = m_ai.textModel();
    m_sImageModel = m_ai.imageModel();
}

void ThreadAIPrepare::setNeededModels(bool bText, bool bImage)
{
    m_bNeedTextModel = bText;
    m_bNeedImageModel = bImage;
}

void ThreadAIPrepare::requestStop()
{
    m_bStop = true;
}

void ThreadAIPrepare::log(const QString &sLine)
{
    const QString s = clampLine(sLine);
    if (s.isEmpty() || s == m_sLastLog)
        return;

    // ollama pull 的进度条会飞快刷新；节流成最多每 250ms 一条，
    // 否则日志区里全是同一行的百分比，用户根本看不清在干什么。
    static const QRegularExpression progressRe(QStringLiteral("^pulling .*%"));
    if (progressRe.match(s).hasMatch()) {
        if (m_logTimer.isValid() && m_logTimer.elapsed() < 250)
            return;
        m_logTimer.restart();
    }

    m_sLastLog = s;
    emit sendLog(s);
}

void ThreadAIPrepare::addToPath(const QString &sDir)
{
    if (sDir.isEmpty())
        return;
    const QString sPath = QString::fromLocal8Bit(qgetenv("PATH"));
    if (sPath.contains(sDir, Qt::CaseInsensitive))
        return;
    // 改当前进程的 PATH：后续 QProcess 启动的 python / ollama 就能直接找到，
    // 不用让用户重开程序。
    qputenv("PATH", QDir::toNativeSeparators(sDir + QLatin1Char(';') + sPath).toLocal8Bit());
    log(tr("已把 %1 加入本程序的 PATH").arg(QDir::toNativeSeparators(sDir)));
}

QString ThreadAIPrepare::runCapture(const QString &sProgram, const QStringList &args, int nTimeoutMs)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(sProgram, args);
    if (!p.waitForStarted(5000))
        return QString();
    if (!p.waitForFinished(nTimeoutMs > 0 ? nTimeoutMs : 15000)) {
        p.kill();
        p.waitForFinished(2000);
        return QString();
    }
    return QString::fromUtf8(p.readAllStandardOutput());
}

bool ThreadAIPrepare::runTool(const QString &sProgram, const QStringList &args, int nTimeoutMs,
                              QString *psError, const QString &sStage)
{
    if (m_bStop) {
        if (psError != nullptr)
            *psError = tr("已取消");
        return false;
    }
    if (!sStage.isEmpty())
        emit sendStage(sStage);
    log(QStringLiteral("> ") + QDir::toNativeSeparators(sProgram) + QLatin1Char(' ') + args.join(QLatin1Char(' ')));

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(sProgram, args);
    if (!proc.waitForStarted(15000)) {
        if (psError != nullptr)
            *psError = tr("无法启动：%1").arg(QDir::toNativeSeparators(sProgram));
        log(*psError);
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    bool bReportedTick = false;
    while (proc.state() != QProcess::NotRunning) {
        if (m_bStop) {
            proc.kill();
            proc.waitForFinished(3000);
            if (psError != nullptr)
                *psError = tr("已取消");
            return false;
        }
        proc.waitForReadyRead(250);

        const QString sChunk = QString::fromUtf8(proc.readAllStandardOutput());
        if (!sChunk.isEmpty()) {
            const QStringList lines = sChunk.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                                   Qt::SkipEmptyParts);
            for (const QString &sLine : lines)
                log(sLine);
        }

        if (nTimeoutMs > 0 && timer.elapsed() > nTimeoutMs) {
            proc.kill();
            proc.waitForFinished(3000);
            if (psError != nullptr)
                *psError = tr("命令超时（超过 %1 分钟）").arg(nTimeoutMs / 60000);
            log(*psError);
            return false;
        }
        // 大文件下载时每 30 秒提示一次"还在跑"，避免界面看起来卡死
        if (!bReportedTick && timer.elapsed() > 30000) {
            bReportedTick = true;
            log(tr("……仍在进行，已等待 %1 秒").arg(timer.elapsed() / 1000));
        }
    }

    const QString sRest = QString::fromUtf8(proc.readAllStandardOutput());
    if (!sRest.isEmpty()) {
        const QStringList lines = sRest.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                              Qt::SkipEmptyParts);
        for (const QString &sLine : lines)
            log(sLine);
    }

    if (m_bStop) {
        if (psError != nullptr)
            *psError = tr("已取消");
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit) {
        if (psError != nullptr)
            *psError = tr("命令异常退出：%1").arg(QDir::toNativeSeparators(sProgram));
        log(*psError);
        return false;
    }
    if (proc.exitCode() != 0) {
        if (psError != nullptr)
            *psError = tr("命令返回错误码 %1").arg(proc.exitCode());
        log(*psError);
        return false;
    }
    return true;
}

QString ThreadAIPrepare::findPython()
{
    const QStringList candidates = {QStringLiteral("python"), QStringLiteral("python3"), QStringLiteral("py")};
    for (const QString &sName : candidates) {
        QProcess p;
        p.start(sName, QStringList() << QStringLiteral("-c")
                                     << QStringLiteral("import sys;print(sys.executable)"));
        if (!p.waitForFinished(8000) || p.exitCode() != 0)
            continue;
        const QString sPath = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        if (!sPath.isEmpty() && QFileInfo::exists(sPath))
            return QDir::fromNativeSeparators(sPath);
    }

    // PATH 里没有时，看看 python.org 安装包默认的按用户安装目录
    const QString sPythonRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                                + QStringLiteral("/Programs/Python");
    QDir dir(sPythonRoot);
    const QStringList qLDirs = dir.entryList(QStringList() << QStringLiteral("Python3*"),
                                             QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for (const QString &sSub : qLDirs) {
        const QString sExe = dir.filePath(sSub + QStringLiteral("/python.exe"));
        if (QFileInfo::exists(sExe))
            return QDir::cleanPath(sExe);
    }
    return QString();
}

QString ThreadAIPrepare::findOllama()
{
    const QString sInPath = QStandardPaths::findExecutable(QStringLiteral("ollama"));
    if (!sInPath.isEmpty())
        return QDir::cleanPath(sInPath);

    const QStringList qLCandidates = {
        QDir::fromNativeSeparators(qEnvironmentVariable("LOCALAPPDATA")) + QStringLiteral("/Programs/Ollama/ollama.exe"),
        QDir::fromNativeSeparators(qEnvironmentVariable("ProgramFiles")) + QStringLiteral("/Ollama/ollama.exe"),
        QDir::fromNativeSeparators(qEnvironmentVariable("ProgramFiles(x86)")) + QStringLiteral("/Ollama/ollama.exe"),
    };
    for (const QString &sPath : qLCandidates) {
        if (QFileInfo::exists(sPath))
            return QDir::cleanPath(sPath);
    }
    return QString();
}

bool ThreadAIPrepare::moduleAvailable(const QString &sPython, const QString &sModule)
{
    QProcess p;
    p.start(sPython, QStringList() << QStringLiteral("-c")
                                   << QStringLiteral("import %1").arg(sModule));
    return p.waitForFinished(20000) && p.exitCode() == 0;
}

bool ThreadAIPrepare::pipAvailable(const QString &sPython)
{
    QProcess p;
    p.start(sPython, QStringList() << QStringLiteral("-m") << QStringLiteral("pip")
                                   << QStringLiteral("--version"));
    return p.waitForFinished(20000) && p.exitCode() == 0;
}

bool ThreadAIPrepare::serverUp()
{
    QTcpSocket socket;
    socket.connectToHost(QStringLiteral("127.0.0.1"), 11434);
    return socket.waitForConnected(1200);
}

bool ThreadAIPrepare::stepPython(QString *psPython, QString *psError)
{
    emit sendStage(tr("检查 Python…"));
    QString sPython = findPython();
    if (!sPython.isEmpty()) {
        log(tr("已找到 Python：%1").arg(sPython));
        addToPath(QFileInfo(sPython).absolutePath());
        *psPython = sPython;
        return true;
    }

    log(tr("没有找到 Python，开始自动下载官方安装包（约 25MB）…"));
    const QString sInstaller = QDir::temp().filePath(QStringLiteral("FileMarker_python_setup.exe"));
    if (!runTool(QStringLiteral("curl"),
                 QStringList() << QStringLiteral("-L") << QStringLiteral("--fail")
                               << QStringLiteral("--retry") << QStringLiteral("2")
                               << QStringLiteral("-o") << sInstaller
                               << QString::fromLatin1(kPythonInstallerUrl),
                 kPythonInstallTimeoutMs, psError,
                 tr("下载 Python 安装包…"))) {
        return false;
    }
    if (!QFileInfo::exists(sInstaller)) {
        *psError = tr("Python 安装包没有下载成功");
        return false;
    }

    // python.org 安装包：按当前用户安装、不弹界面、自动加 PATH 和 pip
    if (!runTool(sInstaller,
                 QStringList() << QStringLiteral("/quiet")
                               << QStringLiteral("InstallAllUsers=0")
                               << QStringLiteral("PrependPath=1")
                               << QStringLiteral("Include_pip=1")
                               << QStringLiteral("Include_launcher=1"),
                 kPythonInstallTimeoutMs, psError,
                 tr("正在静默安装 Python（约 1~2 分钟）…"))) {
        return false;
    }

    sPython = findPython();
    if (sPython.isEmpty()) {
        *psError = tr("Python 安装完成但仍找不到 python 命令，请手动安装后重试"
                      "（https://www.python.org/downloads/）");
        return false;
    }
    addToPath(QFileInfo(sPython).absolutePath());
    log(tr("Python 安装完成：%1").arg(sPython));
    *psPython = sPython;
    return true;
}

bool ThreadAIPrepare::stepPipModules(const QString &sPython, QString *psError)
{
    emit sendStage(tr("检查 Python 依赖库…"));
    const QList<QPair<QString, QString>> modules = {
        {QStringLiteral("requests"), QStringLiteral("requests")},
        {QStringLiteral("pypdf"), QStringLiteral("pypdf")},
        {QStringLiteral("docx"), QStringLiteral("python-docx")},
    };

    QStringList qLMissingPip;
    for (const auto &pair : modules) {
        if (moduleAvailable(sPython, pair.first)) {
            log(tr("依赖库已安装：%1").arg(pair.second));
        } else {
            log(tr("依赖库缺失：%1").arg(pair.second));
            qLMissingPip << pair.second;
        }
    }
    if (qLMissingPip.isEmpty())
        return true;

    if (!pipAvailable(sPython)) {
        log(tr("pip 不可用，先执行 python -m ensurepip…"));
        if (!runTool(sPython, QStringList() << QStringLiteral("-m") << QStringLiteral("ensurepip")
                                            << QStringLiteral("--upgrade"),
                     10 * 60 * 1000, psError, tr("安装 pip…"))) {
            return false;
        }
    }

    QStringList qLArgs = QStringList() << QStringLiteral("-m") << QStringLiteral("pip")
                                       << QStringLiteral("install") << QStringLiteral("--user")
                                       << QStringLiteral("--upgrade");
    qLArgs << qLMissingPip;
    if (!runTool(sPython, qLArgs, kPipTimeoutMs, psError,
                 tr("安装 Python 依赖库：%1…").arg(qLMissingPip.join(QStringLiteral(", "))))) {
        return false;
    }

    for (const QString &sPipName : qLMissingPip) {
        const QString sModule = (sPipName == QStringLiteral("python-docx")) ? QStringLiteral("docx")
                                                                            : sPipName;
        if (!moduleAvailable(sPython, sModule)) {
            *psError = tr("依赖库 %1 安装后仍无法导入，请手动执行：python -m pip install %2")
                           .arg(sPipName, sPipName);
            return false;
        }
    }
    log(tr("Python 依赖库安装完成"));
    return true;
}

bool ThreadAIPrepare::stepOllama(QString *psOllama, QString *psError)
{
    emit sendStage(tr("检查 Ollama…"));
    QString sOllama = findOllama();
    if (!sOllama.isEmpty()) {
        log(tr("已找到 Ollama：%1").arg(sOllama));
        addToPath(QFileInfo(sOllama).absolutePath());
        *psOllama = sOllama;
        return true;
    }

    log(tr("没有找到 Ollama，开始自动下载安装包（约 1GB，走 GitHub 下载）…"));
    const QString sInstaller = QDir::temp().filePath(QStringLiteral("FileMarker_OllamaSetup.exe"));
    if (!runTool(QStringLiteral("curl"),
                 QStringList() << QStringLiteral("-L") << QStringLiteral("--fail")
                               << QStringLiteral("--retry") << QStringLiteral("2")
                               << QStringLiteral("-o") << sInstaller
                               << QString::fromLatin1(kOllamaInstallerUrl),
                 kOllamaInstallTimeoutMs, psError,
                 tr("下载 Ollama 安装包…"))) {
        return false;
    }
    if (!QFileInfo::exists(sInstaller)) {
        *psError = tr("Ollama 安装包没有下载成功（可从 https://ollama.com/download 手动安装）");
        return false;
    }

    // Ollama 的 Windows 安装包是 Inno Setup：静默安装参数
    if (!runTool(sInstaller,
                 QStringList() << QStringLiteral("/VERYSILENT") << QStringLiteral("/NORESTART")
                               << QStringLiteral("/SUPPRESSMSGBOXES"),
                 kOllamaInstallTimeoutMs, psError,
                 tr("正在静默安装 Ollama…"))) {
        return false;
    }

    sOllama = findOllama();
    if (sOllama.isEmpty()) {
        *psError = tr("Ollama 安装完成但仍找不到 ollama 命令，请手动安装后重试"
                      "（https://ollama.com/download）");
        return false;
    }
    addToPath(QFileInfo(sOllama).absolutePath());
    log(tr("Ollama 安装完成：%1").arg(sOllama));
    *psOllama = sOllama;
    return true;
}

bool ThreadAIPrepare::stepOllamaServe(const QString &sOllama, QString *psError)
{
    emit sendStage(tr("检查 Ollama 服务…"));
    if (serverUp()) {
        log(tr("Ollama 服务已在运行"));
        return true;
    }

    log(tr("Ollama 服务没有运行，正在后台启动 ollama serve…"));
    QProcess::startDetached(sOllama, QStringList() << QStringLiteral("serve"));

    for (int nWait = 0; nWait < 90; ++nWait) {
        if (m_bStop) {
            *psError = tr("已取消");
            return false;
        }
        QThread::msleep(1000);
        if (serverUp()) {
            log(tr("Ollama 服务已就绪"));
            return true;
        }
        if (nWait > 0 && nWait % 10 == 0)
            log(tr("……仍在等待 Ollama 服务启动（%1 秒）").arg(nWait));
    }
    *psError = tr("Ollama 服务启动超时，请手动运行 ollama serve 后重试");
    return false;
}

bool ThreadAIPrepare::modelInstalled(const QString &sOllama, const QString &sModel)
{
    const QString sList = runCapture(sOllama, QStringList() << QStringLiteral("list"), 20000);
    if (sList.isEmpty())
        return false;
    const QStringList qLLines = sList.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                            Qt::SkipEmptyParts);
    for (const QString &sLine : qLLines) {
        if (sLine.trimmed().startsWith(sModel))   // 行首是 NAME 列
            return true;
    }
    return false;
}

bool ThreadAIPrepare::stepModels(const QString &sOllama, QString *psError)
{
    emit sendStage(tr("检查模型…"));
    QStringList qLNeeded;
    if (m_bNeedTextModel)
        qLNeeded << m_sTextModel;
    if (m_bNeedImageModel && !qLNeeded.contains(m_sImageModel))
        qLNeeded << m_sImageModel;

    for (const QString &sModel : qLNeeded) {
        if (modelInstalled(sOllama, sModel)) {
            log(tr("模型已安装：%1").arg(sModel));
            continue;
        }
        log(tr("模型未安装：%1，开始下载（可随时点【停止全部】取消）").arg(sModel));
        if (!runTool(sOllama, QStringList() << QStringLiteral("pull") << sModel,
                     kModelPullTimeoutMs, psError,
                     tr("下载模型 %1…").arg(sModel))) {
            return false;
        }
        if (!modelInstalled(sOllama, sModel)) {
            *psError = tr("模型 %1 下载后仍不可用，请手动执行：ollama pull %2").arg(sModel, sModel);
            return false;
        }
        log(tr("模型下载完成：%1").arg(sModel));
    }
    return true;
}

void ThreadAIPrepare::run()
{
    QString sError;
    QString sPython;

    if (!stepPython(&sPython, &sError)) {
        emit sendFinished(false, sError);
        return;
    }
    if (!stepPipModules(sPython, &sError)) {
        emit sendFinished(false, sError);
        return;
    }

    // ---------------- 云端 API：不需要 Ollama / 本地模型 ----------------
    if (m_ai.isApi()) {
        emit sendStage(tr("检查云端 API 配置…"));
        if (m_ai.apiBase.isEmpty()) {
            emit sendFinished(false,
                              tr("云端 API 模式没有配置 Base URL。\n"
                                 "请打开【文件 → AI 打标设置…】填写 Base URL / API Key / 模型名。"));
            return;
        }
        if (m_ai.textModel().isEmpty()) {
            emit sendFinished(false,
                              tr("云端 API 模式没有配置文本模型。\n"
                                 "请打开【文件 → AI 打标设置…】填写模型名（如 deepseek-chat）。"));
            return;
        }
        log(tr("云端 API 模式：%1").arg(m_ai.apiBase));
        log(tr("文本模型：%1；视觉模型：%2")
                .arg(m_ai.textModel(),
                     m_ai.imageModel().isEmpty() ? tr("（未配置）") : m_ai.imageModel()));
        log(tr("已跳过本地 Ollama / 模型下载，文件内容将发送到上面的服务端"));
        emit sendFinished(true, QString());
        return;
    }

    QString sOllama;
    if (!stepOllama(&sOllama, &sError)) {
        emit sendFinished(false, sError);
        return;
    }
    if (!stepOllamaServe(sOllama, &sError)) {
        emit sendFinished(false, sError);
        return;
    }
    if (!stepModels(sOllama, &sError)) {
        emit sendFinished(false, sError);
        return;
    }

    emit sendFinished(true, QString());
}
