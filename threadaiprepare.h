#ifndef THREADAIPREPARE_H
#define THREADAIPREPARE_H

#include <QThread>
#include <QStringList>

#include <atomic>

/**
 * AI 打标前的"环境自动准备"线程。
 *
 * 用户点【AI自动生成标签】时，如果机器上缺 Python / pip 依赖库 / Ollama / 模型，
 * 就在这里自动下载安装，全部在后台线程里跑，进度通过信号发到进度页：
 *   - 缺 Python：从 python.org 下载官方安装包，按当前用户静默安装；
 *   - 缺 requests / pypdf / python-docx：python -m pip install --user 安装；
 *   - 缺 Ollama：从 ollama.com 下载 OllamaSetup.exe 静默安装；
 *   - Ollama 没在跑：自动 ollama serve；
 *   - 缺模型：ollama pull（按当前选中的文件类型只拉需要的那个）。
 *
 * 每一步的输出都通过 sendLog 原样发出去，用户可以在进度页看到下载/安装过程，
 * 也可以随时点【停止全部】取消。
 */
class ThreadAIPrepare : public QThread
{
    Q_OBJECT

public:
    explicit ThreadAIPrepare(QObject *parent = nullptr);

    /** 根据选中的文件类型决定要准备哪些模型（只选纯文本时不必下载 6GB 的视觉模型）。 */
    void setNeededModels(bool bText, bool bImage);
    void requestStop();

signals:
    void sendStage(const QString &sStage);      // 当前阶段（显示在状态行）
    void sendLog(const QString &sLine);         // 详细日志（命令输出）
    void sendFinished(bool bOk, const QString &sError);

protected:
    void run() override;

private:
    bool stepPython(QString *psPython, QString *psError);
    bool stepPipModules(const QString &sPython, QString *psError);
    bool stepOllama(QString *psOllama, QString *psError);
    bool stepOllamaServe(const QString &sOllama, QString *psError);
    bool stepModels(const QString &sOllama, QString *psError);

    /** 运行一个命令，把输出逐行发成日志；nTimeoutMs<=0 表示不限时（大模型下载）。 */
    bool runTool(const QString &sProgram, const QStringList &args, int nTimeoutMs,
                 QString *psError, const QString &sStage = QString());
    /** 运行命令并返回合并后的输出（用于 ollama list 这类需要读取结果的命令）。 */
    QString runCapture(const QString &sProgram, const QStringList &args, int nTimeoutMs);

    bool moduleAvailable(const QString &sPython, const QString &sModule);
    bool pipAvailable(const QString &sPython);
    bool serverUp();
    bool modelInstalled(const QString &sOllama, const QString &sModel);

    QString findPython();
    QString findOllama();
    void addToPath(const QString &sDir);
    void log(const QString &sLine);

    std::atomic_bool m_bStop{false};
    bool        m_bNeedTextModel = true;
    bool        m_bNeedImageModel = true;
    QString     m_sTextModel;
    QString     m_sImageModel;
};

#endif   // THREADAIPREPARE_H
