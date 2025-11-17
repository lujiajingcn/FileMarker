#include "configoperation.h"

#include <QSettings>
#include <QTextCodec>
#include <QDebug>

ConfigOperation::ConfigOperation()
{

}

SearchConfig ConfigOperation::readSearchConfig(QString sConfigFilePath)
{
    SearchConfig sc;

    QSettings settings(sConfigFilePath, QSettings::Format::IniFormat);
    if(settings.status() != QSettings::NoError)
    {
        qWarning().noquote()<<QString("打开配置文件失败:").arg(settings.status());
        return sc;
    }
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
    settings.beginGroup("search");
    sc.bFromCache = settings.value("fromcache").toBool();
    sc.logic = static_cast<labelLogic>(settings.value("labellogic").toInt());

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
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
    settings.beginGroup("search");
    settings.setValue("fromcache", sc.bFromCache);
    settings.setValue("labellogic", sc.logic);
    settings.endGroup();
}
