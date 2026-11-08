#include "utility.h"

#include <QUuid>
#include <QDateTime>
#include <QRegularExpression>

Utility::Utility()
{

}

QTextStream Utility::tsLogInfo;
QMutex Utility::mutex;

void Utility::myMessageHandler(QtMsgType mType, const QMessageLogContext&, const QString& sMsg)
{
    mutex.lock();
    QString sFormatCurTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    switch (mType) {
    case QtDebugMsg:
        tsLogInfo << QString("%1    Debug       %2").arg(sFormatCurTime).arg(sMsg) << endl;
        break;
    case QtInfoMsg:
        tsLogInfo << QString("%1    Info        %2").arg(sFormatCurTime).arg(sMsg) << endl;
        break;
    case QtWarningMsg:
        tsLogInfo << QString("%1    Warning     %2").arg(sFormatCurTime).arg(sMsg) << endl;
        break;
    case QtCriticalMsg:
        tsLogInfo << QString("%1    Critical    %2").arg(sFormatCurTime).arg(sMsg) << endl;
        break;
    case QtFatalMsg:
        tsLogInfo << QString("%1    Fatal       %2").arg(sFormatCurTime).arg(sMsg) << endl;
        abort();
    }
    mutex.unlock();
}

// 根据xpath找到指定的节点集合。
xmlXPathObjectPtr Utility::searchByXPath(xmlDocPtr doc, const xmlChar *xpath)
{
    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;
    context = xmlXPathNewContext(doc);

    if (context == nullptr)
    {
        // P3-26：改用 qWarning 替代 printf，确保日志能被全局 messageHandler 捕获写入文件。
        qWarning("searchByXPath: context is NULL");
        return nullptr;
    }

    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == nullptr)
    {
        qWarning("searchByXPath: xmlXPathEvalExpression returned NULL");
        return nullptr;
    }

    if (xmlXPathNodeSetIsEmpty(result->nodesetval))
    {
        xmlXPathFreeObject(result);
        qWarning("searchByXPath: nodeset is empty");
        return nullptr;
    }

    return result;
}

QString Utility::createUuid()
{
    return QUuid::createUuid().toString().remove("{").remove("}").remove("-");
}

bool Utility::hasEqualElement(const QStringList &list1, const QStringList &list2)
{
    QSet<QString> set1 = list1.toSet();
    QSet<QString> set2 = list2.toSet();
    return !set1.intersect(set2).isEmpty();
}

bool Utility::isContains(const QStringList &list1, const QStringList &list2)
{
    QSet<QString> set1 = list1.toSet();
    QSet<QString> set2 = list2.toSet();
    return set1.contains(set2);
}

// 去掉 FindFirstStreamW 返回的流名后缀 ":$DATA"，得到真正的标签名。
// 例：FindFirstStreamW 返回 "::tag:$DATA"，调用方去掉前导冒号后为 "tag:$DATA"，
// 本函数返回 "tag"。缺少这一步会让标签在界面上显示成 "tag:$DATA"，
// 并且用 "tag" 查找文件时永远匹配不到。
QString Utility::adsNameWithoutType(QString sADSName)
{
    if(sADSName.endsWith(ADS_TAIL))
    {
        sADSName.chop(strlen(ADS_TAIL));
    }
    return sADSName;
}

// 拆分标签字符串：英文逗号、中文逗号、顿号、英文分号、中文分号都算，
// 方便中文输入法下直接敲"，"、"、"或"；"。split() 保留空项，这里统一 trim + 过滤空项 + 去重。
QStringList Utility::splitLabels(const QString &sLabels)
{
    static const QRegularExpression re("[,，、;；]");

    QStringList labels;
    const QStringList parts = sLabels.split(re);
    foreach (const QString &sPart, parts)
    {
        const QString sLabel = sPart.trimmed();
        if (sLabel.isEmpty() || labels.contains(sLabel))
            continue;
        labels << sLabel;
    }
    return labels;
}

// P2-21：参数改为按值传递。原签名是非 const 引用，函数内部调用 chop() 会修改调用者的字符串，
// 调用者可能不期望参数被修改。按值传递后函数内部可以自由修改副本。
bool Utility::isADSNameValid(QString sADSName)
{
    sADSName = adsNameWithoutType(sADSName);
    if(sADSName.isEmpty())
    {
        return false;
    }
    if(QString::compare(sADSName, FILTER_TAG) == 0)
    {
        return false;
    }
    return true;
}
