#include "configoperation.h"

#include <QSettings>
#include <QDebug>

ConfigOperation::ConfigOperation()
{

}

SearchConfig ConfigOperation::readSearchConfig(const QString &sConfigFilePath)
{
    SearchConfig sc;

    QSettings settings(sConfigFilePath, QSettings::Format::IniFormat);
    if (settings.status() != QSettings::NoError)
    {
        qWarning().noquote() << QString("打开配置文件失败: %1").arg(settings.status());
        return sc;
    }
    settings.beginGroup("search");
    sc.bFromCache = settings.value("fromcache").toBool();
    // 校验 labellogic 的枚举范围（仅 0=AND, 1=OR 合法）。
    // 配置文件损坏或手动编辑时，避免 static_cast 产生非法枚举值。
    int nLogic = settings.value("labellogic").toInt();
    if (nLogic == labelLogic::AND || nLogic == labelLogic::OR)
        sc.logic = static_cast<labelLogic>(nLogic);
    else
        sc.logic = labelLogic::AND;

    settings.endGroup();

    return sc;
}

void ConfigOperation::writeSearchConfig(const QString &sConfigFilePath, SearchConfig sc)
{
    QSettings settings(sConfigFilePath, QSettings::Format::IniFormat);
    if (settings.status() != QSettings::NoError)
    {
        qWarning().noquote()<<QString("写入配置文件失败:%1").arg(settings.status());
        return;
    }
    settings.beginGroup("search");
    settings.setValue("fromcache", sc.bFromCache);
    settings.setValue("labellogic", sc.logic);
    settings.endGroup();
}
