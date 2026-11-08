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

    /** 退出前在锁内 flush 并摘掉日志设备，避免与工作线程日志竞争 */
    static void shutdownLog();

    static xmlXPathObjectPtr searchByXPath(xmlDocPtr doc, const xmlChar *xpath);
    static QString createUuid();

    /** 判断两个QStringList是否有相同的元素 */
    static bool hasEqualElement(const QStringList &list1, const QStringList &list2);

    /** 判断一个QStringList是否包含另一个QStringList的全部元素，不考虑元素顺序 */
    static bool isContains(const QStringList &list1, const QStringList &list2);

    static bool isADSNameValid(QString sADSName);

    /** 去掉 NTFS 流名末尾的 ":$DATA"（FindFirstStreamW 返回的形式），得到真正的标签名 */
    static QString adsNameWithoutType(QString sADSName);

    /**
     ** 拆分标签字符串，兼容英文逗号(,)、中文逗号(，)、顿号(、)、英文分号(;)、中文分号(；)
     ** 结果已去除首尾空白、空项和重复项，保持原有先后顺序。
     **/
    static QStringList splitLabels(const QString &sLabels);
};

#endif // UTILITY_H
