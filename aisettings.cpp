#include "aisettings.h"

#include "utility.h"

#include <QSettings>

namespace {

// 读一个键时顺带做范围裁剪/去空格，避免 ini 里手改出非法值
QString readTrimmed(const QSettings &settings, const QString &sKey, const QString &sDefault)
{
    return settings.value(sKey, sDefault).toString().trimmed();
}

}   // namespace

AiSettings AiSettings::load()
{
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);

    AiSettings a;
    settings.beginGroup(QStringLiteral("ai"));
    a.provider = readTrimmed(settings, QStringLiteral("provider"), QStringLiteral("ollama")).toLower();
    if (a.provider != QLatin1String("api"))
        a.provider = QStringLiteral("ollama");

    a.apiBase = readTrimmed(settings, QStringLiteral("apiBase"), QString());
    a.apiKey = settings.value(QStringLiteral("apiKey"), QString()).toString();
    a.apiProxy = readTrimmed(settings, QStringLiteral("apiProxy"), QString());
    a.timeoutSec = settings.value(QStringLiteral("timeout"), 120).toInt();
    a.timeoutSec = qBound(10, a.timeoutSec, 600);

    // 本地模型默认值可以用环境变量覆盖（测试/调试时很方便）
    a.ollamaTextModel = readTrimmed(settings, QStringLiteral("ollamaTextModel"),
                                    qEnvironmentVariable("FM_AI_TEXT_MODEL",
                                                         QStringLiteral("qwen2.5:7b")));
    a.ollamaImageModel = readTrimmed(settings, QStringLiteral("ollamaImageModel"),
                                     qEnvironmentVariable("FM_AI_IMAGE_MODEL",
                                                          QStringLiteral("qwen2.5vl:7b")));
    a.apiTextModel = readTrimmed(settings, QStringLiteral("apiTextModel"), QString());
    a.apiImageModel = readTrimmed(settings, QStringLiteral("apiImageModel"), QString());
    settings.endGroup();

    if (a.apiBase.endsWith(QLatin1Char('/')))
        a.apiBase.chop(1);
    return a;
}

void AiSettings::save() const
{
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ai"));
    settings.setValue(QStringLiteral("provider"), provider == QLatin1String("api")
                                                        ? QStringLiteral("api")
                                                        : QStringLiteral("ollama"));
    settings.setValue(QStringLiteral("apiBase"), apiBase);
    settings.setValue(QStringLiteral("apiKey"), apiKey);
    settings.setValue(QStringLiteral("apiProxy"), apiProxy);
    settings.setValue(QStringLiteral("timeout"), timeoutSec);
    settings.setValue(QStringLiteral("ollamaTextModel"), ollamaTextModel);
    settings.setValue(QStringLiteral("ollamaImageModel"), ollamaImageModel);
    settings.setValue(QStringLiteral("apiTextModel"), apiTextModel);
    settings.setValue(QStringLiteral("apiImageModel"), apiImageModel);
    settings.endGroup();
    settings.sync();
}

bool AiSettings::operator==(const AiSettings &other) const
{
    return provider == other.provider
           && apiBase == other.apiBase
           && apiKey == other.apiKey
           && apiProxy == other.apiProxy
           && timeoutSec == other.timeoutSec
           && ollamaTextModel == other.ollamaTextModel
           && ollamaImageModel == other.ollamaImageModel
           && apiTextModel == other.apiTextModel
           && apiImageModel == other.apiImageModel;
}
