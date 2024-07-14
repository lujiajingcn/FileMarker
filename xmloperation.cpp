#include "xmloperation.h"
#include "utility.h"

XmlOperation::XmlOperation()
{

}

QList<LabelInfo> XmlOperation::readLabelFromXmlFile(const char* sXml)
{
    QList<LabelInfo> lstLabelInfo;

    xmlDocPtr doc = xmlReadFile(sXml, "utf-8", XML_PARSE_NOBLANKS);
    if (nullptr == doc)
    {
        return lstLabelInfo;
    }

    xmlChar *xpath = BAD_CAST "/Root";
    xmlXPathObjectPtr result = Utility::searchByXPath(doc, xpath);
    if (nullptr == result)
    {
        return lstLabelInfo;
    }
    xmlNodeSetPtr nodeset = result->nodesetval;
    xmlNodePtr root = nodeset->nodeTab[0];

    traverseReadItem(root, lstLabelInfo, nullptr);

    xmlXPathFreeObject(result);

    return lstLabelInfo;
}

bool XmlOperation::saveXml(const char* sXml, const QList<LabelInfo> &lstBlockInfo)
{
    //定义文档和节点指针
    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
    doc->encoding = BAD_CAST strdup("UTF-8");
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST"Root");

    //设置根节点
    xmlDocSetRootElement(doc, root);

    traverseWriteItem(lstBlockInfo, root);

//    xmlKeepBlanksDefault(0) ;
//    xmlIndentTreeOutput = 1 ;

    int ret = xmlSaveFormatFileEnc(sXml, doc, "UTF-8", 1);
    if(ret == -1)
    {
        return false;
    }

    //释放文档节点动态申请的内存
    free((void *)(doc->encoding));
    doc->encoding = NULL;
    xmlFreeDoc(doc);

    return true;
}

void XmlOperation::traverseReadItem(xmlNodePtr item, QList<LabelInfo> &lstLabelInfo, LabelInfo *pParent)
{
    xmlNodePtr cur = item->children;
    while(cur != nullptr)
    {
        LabelInfo labelInfo;

        xmlChar *propId = xmlGetProp(cur, (xmlChar*)"id");
        labelInfo.sUuid = QString((char*)propId);

        xmlChar *propName = xmlGetProp(cur, (xmlChar*)"name");
        labelInfo.sName = QString((char*)propName);

        if(cur->children != nullptr)
        {
            traverseReadItem(cur->children, lstLabelInfo, &labelInfo);
        }

        if(pParent != nullptr)
        {
            pParent->lstChild.push_back(labelInfo);
        }
        else
        {
            lstLabelInfo.push_back(labelInfo);
        }

        cur = cur->next;
    }
}

void XmlOperation::traverseWriteItem( const QList<LabelInfo> &lstBlockInfo, xmlNodePtr pNode)
{
    for (QList<LabelInfo>::const_iterator cIt = lstBlockInfo.begin(); cIt != lstBlockInfo.end(); cIt++)
    {
        LabelInfo labelInfo = *cIt;

        xmlNodePtr pNodeItem = xmlNewTextChild(pNode, NULL, BAD_CAST"label", BAD_CAST"");
        xmlNewProp(pNodeItem, BAD_CAST"id", (xmlChar *)labelInfo.sUuid.toStdString().c_str());
        xmlNewProp(pNodeItem, BAD_CAST"name", (xmlChar *)labelInfo.sName.toStdString().c_str());

        int nChildCount = labelInfo.lstChild.count();
        if(nChildCount > 0)
        {
            xmlNodePtr pNodeKids = xmlNewTextChild(pNodeItem, NULL, BAD_CAST"label", BAD_CAST"");
            traverseWriteItem(labelInfo.lstChild, pNodeKids);
        }
    }
}

