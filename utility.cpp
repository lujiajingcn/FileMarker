#include "utility.h"

#include <QUuid>
#include <QDateTime>

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
        printf("context is NULL\n");
        return nullptr;
    }

    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == nullptr)
    {
        printf("xmlXPathEvalExpression return NULL\n");
        return nullptr;
    }

    if (xmlXPathNodeSetIsEmpty(result->nodesetval))
    {
        xmlXPathFreeObject(result);
        printf("nodeset is empty\n");
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

bool Utility::isADSNameValue(QString &sADSName)
{
    if(sADSName.endsWith(ADS_TAIL))
    {
        sADSName.chop(strlen(ADS_TAIL));
    }
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
