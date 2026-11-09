/**
** 【标签库 XML 读写】
**
** 原来用 libxml2（外加一次只为选 /Root 的 XPath），实际只需要读写一个很小的文件，
** 现在改成 Qt 自带的 QXmlStreamReader/Writer，去掉第三方依赖。
**
** 兼容两种历史格式：
**   新格式：父 label 里直接嵌套子 label；
**   旧格式：子标签外面多包了一层没有 id/name 的 <label> 容器
**          （旧实现"传首个子节点、再遍历它的 children"，凑巧跳过了这层）。
** 读出时两种都能识别；写出时只产出新格式（不再有包装层）。
**/

#ifndef XMLOPERATION_H
#define XMLOPERATION_H

#include <QString>
#include <QList>

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

    /**
     * 从 xml 文件读取所有标签信息。
     * 文件不存在（首次运行）返回空列表；文件存在但解析失败时，
     * 会先留一份 "<文件名>.bad-时间戳" 备份再返回空列表，不在无提示的情况下丢内容。
     */
    QList<LabelInfo> readLabelFromXmlFile(const char* sXml);

    /** 把所有标签信息存入 xml 文件（先写临时文件再原子替换，避免写一半把旧文件弄坏） */
    bool saveXml(const char* sXml, const QList<LabelInfo> &lstBlockInfo);
};

#endif // XMLOPERATION_H
