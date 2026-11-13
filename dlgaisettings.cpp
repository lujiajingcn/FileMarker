#include "dlgaisettings.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace {

/** 把服务端返回体压缩成一行短的错误说明（不带任何请求头/Key）。 */
QString shortReply(const QByteArray &body)
{
    QString s = QString::fromUtf8(body).simplified();
    if (s.size() > 200)
        s = s.left(200) + QStringLiteral("…");
    return s;
}

}   // namespace

DlgAiSettings::DlgAiSettings(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("AI 打标设置"));
    setMinimumWidth(620);

    auto *mainLayout = new QVBoxLayout(this);

    // ---------- 服务端选择 ----------
    auto *providerBox = new QGroupBox(tr("服务端"), this);
    auto *providerLayout = new QVBoxLayout(providerBox);
    m_rbOllama = new QRadioButton(tr("本地 Ollama（默认，需要下载模型）"), providerBox);
    m_rbApi = new QRadioButton(tr("云端 API（任意 OpenAI 兼容接口，不需要下载模型）"), providerBox);
    providerLayout->addWidget(m_rbOllama);
    providerLayout->addWidget(m_rbApi);
    mainLayout->addWidget(providerBox);

    // ---------- 本地 Ollama ----------
    auto *ollamaBox = new QGroupBox(tr("本地 Ollama"), this);
    auto *ollamaForm = new QFormLayout(ollamaBox);
    m_ollamaTextModel = new QLineEdit(ollamaBox);
    m_ollamaTextModel->setPlaceholderText(QStringLiteral("qwen2.5:7b"));
    m_ollamaImageModel = new QLineEdit(ollamaBox);
    m_ollamaImageModel->setPlaceholderText(QStringLiteral("qwen2.5vl:7b"));
    ollamaForm->addRow(tr("文本模型："), m_ollamaTextModel);
    ollamaForm->addRow(tr("视觉模型："), m_ollamaImageModel);
    mainLayout->addWidget(ollamaBox);

    // ---------- 云端 API ----------
    auto *apiBox = new QGroupBox(tr("云端 API（OpenAI 兼容）"), this);
    auto *apiForm = new QFormLayout(apiBox);
    m_apiBase = new QLineEdit(apiBox);
    m_apiBase->setPlaceholderText(QStringLiteral("https://api.deepseek.com/v1"));
    m_apiKey = new QLineEdit(apiBox);
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_apiKey->setPlaceholderText(tr("sk-…（本地 LM Studio 可留空）"));
    auto *showKey = new QCheckBox(tr("显示"), apiBox);
    connect(showKey, &QCheckBox::toggled, this, [this](bool b) {
        m_apiKey->setEchoMode(b ? QLineEdit::Normal : QLineEdit::Password);
    });
    auto *keyRow = new QWidget(apiBox);
    auto *keyLayout = new QHBoxLayout(keyRow);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(m_apiKey, 1);
    keyLayout->addWidget(showKey);
    m_apiTextModel = new QLineEdit(apiBox);
    m_apiTextModel->setPlaceholderText(QStringLiteral("deepseek-chat / qwen-plus / glm-4-flash …"));
    m_apiImageModel = new QLineEdit(apiBox);
    m_apiImageModel->setPlaceholderText(tr("视觉模型（可留空；图片打标需要）：qwen-vl-max / glm-4v …"));
    apiForm->addRow(tr("Base URL："), m_apiBase);
    apiForm->addRow(tr("API Key："), keyRow);
    apiForm->addRow(tr("文本模型："), m_apiTextModel);
    apiForm->addRow(tr("视觉模型："), m_apiImageModel);

    auto *testRow = new QWidget(apiBox);
    auto *testLayout = new QHBoxLayout(testRow);
    testLayout->setContentsMargins(0, 0, 0, 0);
    auto *btnTest = new QPushButton(tr("测试连接"), testRow);
    m_testResult = new QLabel(testRow);
    m_testResult->setWordWrap(true);
    testLayout->addWidget(btnTest);
    testLayout->addWidget(m_testResult, 1);
    apiForm->addRow(QString(), testRow);
    connect(btnTest, &QPushButton::clicked, this, &DlgAiSettings::testConnection);
    mainLayout->addWidget(apiBox);

    // ---------- 通用 ----------
    auto *commonForm = new QFormLayout;
    m_timeout = new QSpinBox(this);
    m_timeout->setRange(10, 600);
    m_timeout->setSuffix(tr(" 秒"));
    m_timeout->setToolTip(tr("单次 API / 模型请求的超时时间"));
    commonForm->addRow(tr("单文件超时："), m_timeout);
    mainLayout->addLayout(commonForm);

    auto *hint = new QLabel(tr("提示：云端模式会上传【文本文件前 4000 个字符】和【图片（本地缩放到 1024 像素内后压缩）】；"
                               "视频、压缩包、电子书等不会被处理，也不会被上传。"),
                            this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#666;"));
    mainLayout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        QString sError;
        if (!validate(&sError)) {
            QMessageBox::warning(this, tr("AI 打标设置"), sError);
            return;
        }
        settings().save();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    connect(m_rbOllama, &QRadioButton::toggled, this, &DlgAiSettings::updateProviderUi);
    connect(m_rbApi, &QRadioButton::toggled, this, &DlgAiSettings::updateProviderUi);

    loadFrom(AiSettings::load());
    updateProviderUi();
}

AiSettings DlgAiSettings::settings() const
{
    AiSettings a = m_loaded;
    a.provider = m_rbApi->isChecked() ? QStringLiteral("api") : QStringLiteral("ollama");
    a.apiBase = m_apiBase->text().trimmed();
    while (a.apiBase.endsWith(QLatin1Char('/')))
        a.apiBase.chop(1);
    a.apiKey = m_apiKey->text();
    a.timeoutSec = m_timeout->value();
    a.ollamaTextModel = m_ollamaTextModel->text().trimmed();
    a.ollamaImageModel = m_ollamaImageModel->text().trimmed();
    a.apiTextModel = m_apiTextModel->text().trimmed();
    a.apiImageModel = m_apiImageModel->text().trimmed();
    return a;
}

void DlgAiSettings::loadFrom(const AiSettings &a)
{
    m_loaded = a;
    (a.isApi() ? m_rbApi : m_rbOllama)->setChecked(true);
    m_apiBase->setText(a.apiBase);
    m_apiKey->setText(a.apiKey);
    m_timeout->setValue(a.timeoutSec);
    m_ollamaTextModel->setText(a.ollamaTextModel);
    m_ollamaImageModel->setText(a.ollamaImageModel);
    m_apiTextModel->setText(a.apiTextModel);
    m_apiImageModel->setText(a.apiImageModel);
}

void DlgAiSettings::updateProviderUi()
{
    const bool bApi = m_rbApi->isChecked();
    for (QWidget *w : {static_cast<QWidget *>(m_apiBase), static_cast<QWidget *>(m_apiKey),
                       static_cast<QWidget *>(m_apiTextModel), static_cast<QWidget *>(m_apiImageModel)}) {
        if (w != nullptr)
            w->setEnabled(bApi);
    }
    for (QWidget *w : {static_cast<QWidget *>(m_ollamaTextModel),
                       static_cast<QWidget *>(m_ollamaImageModel)}) {
        if (w != nullptr)
            w->setEnabled(!bApi);
    }
}

bool DlgAiSettings::validate(QString *psError) const
{
    if (m_rbApi->isChecked()) {
        if (m_apiBase->text().trimmed().isEmpty()) {
            *psError = tr("云端模式请填写 Base URL（例如 https://api.deepseek.com/v1）。");
            return false;
        }
        if (m_apiTextModel->text().trimmed().isEmpty()) {
            *psError = tr("云端模式请填写文本模型名（例如 deepseek-chat）。");
            return false;
        }
    } else {
        if (m_ollamaTextModel->text().trimmed().isEmpty()) {
            *psError = tr("本地模式请填写文本模型名（默认 qwen2.5:7b）。");
            return false;
        }
    }
    return true;
}

void DlgAiSettings::testConnection()
{
    const QString sBase = m_apiBase->text().trimmed();
    const QString sModel = m_apiTextModel->text().trimmed();
    if (sBase.isEmpty() || sModel.isEmpty()) {
        m_testResult->setText(tr("请先填写 Base URL 和文本模型"));
        return;
    }

    m_testResult->setText(tr("正在测试…"));
    m_testResult->setStyleSheet(QStringLiteral("color:#666;"));

    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(sBase + QStringLiteral("/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QString sKey = m_apiKey->text();
    if (!sKey.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + sKey).toUtf8());

    QJsonObject body;
    body.insert(QStringLiteral("model"), sModel);
    body.insert(QStringLiteral("max_tokens"), 1);
    body.insert(QStringLiteral("temperature"), 0);
    QJsonArray messages;
    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMsg.insert(QStringLiteral("content"), QStringLiteral("ping"));
    messages.append(userMsg);
    body.insert(QStringLiteral("messages"), messages);

    QNetworkReply *reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(20000);
    loop.exec();

    const bool bTimedOut = !reply->isFinished();
    if (bTimedOut)
        reply->abort();

    const int nCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray bodyBytes = reply->readAll();
    const QString sError = reply->errorString();
    reply->deleteLater();

    if (!bTimedOut && reply->error() == QNetworkReply::NoError && nCode >= 200 && nCode < 300) {
        m_testResult->setText(tr("连接成功（HTTP %1）").arg(nCode));
        m_testResult->setStyleSheet(QStringLiteral("color:#1a7f37;"));
        return;
    }

    QString sMsg;
    if (bTimedOut)
        sMsg = tr("连接超时（20 秒）");
    else if (nCode == 401 || nCode == 403)
        sMsg = tr("认证失败（HTTP %1）：API Key 无效或没有权限").arg(nCode);
    else if (nCode == 429)
        sMsg = tr("被限流（HTTP 429），请稍后再试");
    else if (nCode > 0)
        sMsg = tr("HTTP %1：%2").arg(nCode).arg(shortReply(bodyBytes));
    else
        sMsg = tr("连接失败：%1").arg(sError);

    m_testResult->setText(sMsg);
    m_testResult->setStyleSheet(QStringLiteral("color:#b42318;"));
}
