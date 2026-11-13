#ifndef AISETTINGS_H
#define AISETTINGS_H

#include <QString>

/**
 * AI 打标的服务端配置（存在 FileMarker.ini 的 [ai] 段）。
 *
 * 两种模式：
 *   - ollama：本机 Ollama（默认，模型 qwen2.5:7b / qwen2.5vl:7b）；
 *   - api：任意 OpenAI 兼容接口（DeepSeek / 通义 / 智谱 / OpenRouter / LM Studio…）。
 *
 * 两种模式各存一套模型名，切换模式不会互相覆盖。
 */
struct AiSettings
{
    QString provider = QStringLiteral("ollama");   // ollama | api
    QString apiBase;                                // 例如 https://api.deepseek.com/v1
    QString apiKey;
    QString apiProxy;                               // 可选：http://127.0.0.1:7890
    int     timeoutSec = 120;

    QString ollamaTextModel  = QStringLiteral("qwen2.5:7b");
    QString ollamaImageModel = QStringLiteral("qwen2.5vl:7b");

    QString apiTextModel;                           // 例如 deepseek-chat / qwen-plus
    QString apiImageModel;                          // 例如 qwen-vl-max（没有可留空）

    bool isApi() const { return provider.compare(QStringLiteral("api"), Qt::CaseInsensitive) == 0; }

    QString textModel() const { return isApi() ? apiTextModel : ollamaTextModel; }
    QString imageModel() const { return isApi() ? apiImageModel : ollamaImageModel; }

    static AiSettings load();
    void save() const;

    bool operator==(const AiSettings &other) const;
    bool operator!=(const AiSettings &other) const { return !(*this == other); }
};

#endif   // AISETTINGS_H
