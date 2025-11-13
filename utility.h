#ifndef UTILITY_H
#define UTILITY_H

#include <QString>
#include <QStringList>
#include <QSet>
#include <QTextStream>
#include <QMutex>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include "common.h"

class Utility
{
public:
    Utility();

    static QTextStream tsLogInfo;
    static QMutex mutex;

    static void myMessageHandler(QtMsgType mType, const QMessageLogContext&, const QString& sMsg);

    static xmlXPathObjectPtr searchByXPath(xmlDocPtr doc, const xmlChar *xpath);
    static QString createUuid();

    /** 判断两个QStringList是否有相同的元素 */
    static bool hasEqualElement(const QStringList &list1, const QStringList &list2);

    /** 判断一个QStringList是否包含另一个QStringList的全部元素，不考虑元素顺序 */
    static bool isContains(const QStringList &list1, const QStringList &list2);

    static bool isADSNameValue(QString &sADSName);
};

#endif // UTILITY_H
