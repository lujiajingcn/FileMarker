#include "configoperation.h"

#include <QSettings>
#include <QDebug>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif

ConfigOperation::ConfigOperation()
{

}

SearchConfig ConfigOperation::readSearchConfig(QString sConfigFilePath)
{
    SearchConfig sc;

    QSettings settings(sConfigFilePath, QSettings::Format::IniFormat);
    if(settings.status() != QSettings::NoError)
    {
        qWarning().noquote() << QString("打开配置文件失败: %1").arg(settings.status());
        return sc;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6 的 QSettings INI 格式默认即为 UTF-8，且 setIniCodec 的 QString 重载已在 Qt6 移除，
    // 调用会编译失败，故此处不调用（P3/Qt6 兼容）。
#else
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
#endif
    settings.beginGroup("search");
    sc.bFromCache = settings.value("fromcache").toBool();
    // P3-28：校验 labellogic 的枚举范围（仅 0=AND, 1=OR 合法）。
    // 配置文件损坏或手动编辑时，避免 static_cast 产生非法枚举值。
    int nLogic = settings.value("labellogic").toInt();
    if (nLogic == labelLogic::AND || nLogic == labelLogic::OR)
        sc.logic = static_cast<labelLogic>(nLogic);
    else
        sc.logic = labelLogic::AND;

    settings.endGroup();

    return sc;
}

void ConfigOperation::writeSearchConfig(QString sConfigFilePath, SearchConfig sc)
{
    QSettings settings(sConfigFilePath, QSettings::Format::IniFormat);
    if(settings.status() != QSettings::NoError)
    {
        qWarning().noquote()<<QString("写入配置文件失败:%1").arg(settings.status());
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6 的 QSettings INI 格式默认即为 UTF-8，且 setIniCodec 的 QString 重载已在 Qt6 移除，
    // 调用会编译失败，故此处不调用（P3/Qt6 兼容）。
#else
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
#endif
    settings.beginGroup("search");
    settings.setValue("fromcache", sc.bFromCache);
    settings.setValue("labellogic", sc.logic);
    settings.endGroup();
}
