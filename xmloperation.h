#ifndef XMLOPERATION_H
#define XMLOPERATION_H

#include <QString>
#include <QList>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

struct LabelInfo
{
    QString sUuid;
    QString sName;
    QList<LabelInfo> lstChild;
};

class XmlOperation
{
public:
    XmlOperation();

    /** 从xml文件中读取所有的标签信息 */
    QList<LabelInfo> readLabelFromXmlFile(const char* sXml);

    /** 将所有的标签信息存入xml文件 */
    bool saveXml(const char* sXml, const QList<LabelInfo> &lstBlockInfo);

protected:
    /** 递归读标签信息 */
    void traverseReadItem(xmlNodePtr item, QList<LabelInfo> &vtCataLogNode, LabelInfo *pParent);

    /** 递归写标签信息 */
    void traverseWriteItem( const QList<LabelInfo> &lstBlockInfo, xmlNodePtr pNode);
};

#endif // XMLOPERATION_H
