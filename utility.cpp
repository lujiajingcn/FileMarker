#include "utility.h"

#include <QUuid>

Utility::Utility()
{

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
    if(sADSName.endsWith(ADSTAIL))
    {
        sADSName.chop(strlen(ADSTAIL));
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
