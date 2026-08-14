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
        xmlFreeDoc(doc);   // 提前返回也要释放已解析的文档，否则内存泄漏
        return lstLabelInfo;
    }
    xmlNodeSetPtr nodeset = result->nodesetval;
    xmlNodePtr root = nodeset->nodeTab[0];

    traverseReadItem(root, lstLabelInfo, nullptr);

    xmlXPathFreeObject(result);
    xmlFreeDoc(doc);       // 释放已解析的 XML 文档，避免内存泄漏

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
        // P0-5：保存失败时也要释放 strdup 分配的 doc->encoding 和 xmlNewDoc 分配的 doc，
        // 否则每次失败的保存都会泄漏内存。
        free((void *)(doc->encoding));
        doc->encoding = NULL;
        xmlFreeDoc(doc);
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
        // P3-24：跳过非元素节点（如注释节点），避免文本/注释节点破坏遍历逻辑。
        // （纯空白文本节点已由 XML_PARSE_NOBLANKS 过滤，此处再防御性检查。）
        if (cur->type != XML_ELEMENT_NODE) {
            cur = cur->next;
            continue;
        }

        LabelInfo labelInfo;

        xmlChar *propId = xmlGetProp(cur, (xmlChar*)"id");
        if (propId != nullptr) {
            labelInfo.sUuid = QString((char*)propId);
            xmlFree(propId);   // xmlGetProp 返回的字符串需要手动释放
        } else {
            labelInfo.sUuid.clear();
        }

        xmlChar *propName = xmlGetProp(cur, (xmlChar*)"name");
        if (propName != nullptr) {
            labelInfo.sName = QString((char*)propName);
            xmlFree(propName);
        } else {
            labelInfo.sName.clear();
        }

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
        // P3-25：用 toUtf8().constData() 替代 toStdString().c_str() 强转，
        // 确保含非 ASCII 字符的标签名能正确编码为 UTF-8。
        xmlNewProp(pNodeItem, BAD_CAST"id", (xmlChar *)labelInfo.sUuid.toUtf8().constData());
        xmlNewProp(pNodeItem, BAD_CAST"name", (xmlChar *)labelInfo.sName.toUtf8().constData());

        int nChildCount = labelInfo.lstChild.count();
        if(nChildCount > 0)
        {
            xmlNodePtr pNodeKids = xmlNewTextChild(pNodeItem, NULL, BAD_CAST"label", BAD_CAST"");
            traverseWriteItem(labelInfo.lstChild, pNodeKids);
        }
    }
}

