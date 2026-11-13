#ifndef DLGAISETTINGS_H
#define DLGAISETTINGS_H

#include <QDialog>

#include "aisettings.h"

class QLineEdit;
class QLabel;
class QRadioButton;
class QSpinBox;

/**
 * 【AI 打标设置】对话框：
 *   - 选择本地 Ollama 或云端 OpenAI 兼容 API；
 *   - 云端模式填写 Base URL / API Key / 文本模型 / 视觉模型；
 *   - 提供【测试连接】按钮（只发 1 个 token 的探测请求，费用可忽略）。
 */
class DlgAiSettings : public QDialog
{
    Q_OBJECT

public:
    explicit DlgAiSettings(QWidget *parent = nullptr);

    /** 确定后取回当前配置（点取消时调用方不要保存）。 */
    AiSettings settings() const;

private:
    void loadFrom(const AiSettings &a);
    void updateProviderUi();
    void testConnection();
    bool validate(QString *psError) const;

    QRadioButton *m_rbOllama = nullptr;
    QRadioButton *m_rbApi = nullptr;

    QLineEdit *m_ollamaTextModel = nullptr;
    QLineEdit *m_ollamaImageModel = nullptr;

    QLineEdit *m_apiBase = nullptr;
    QLineEdit *m_apiKey = nullptr;
    QLineEdit *m_apiProxy = nullptr;
    QLineEdit *m_apiTextModel = nullptr;
    QLineEdit *m_apiImageModel = nullptr;

    QSpinBox  *m_timeout = nullptr;
    QLabel    *m_testResult = nullptr;

    AiSettings m_loaded;
};

#endif   // DLGAISETTINGS_H
