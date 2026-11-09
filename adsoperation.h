#ifndef ADSOPERATION_H
#define ADSOPERATION_H

#include <QObject>
#include "common.h"
#include <QSet>

class ADSOperation : public QObject
{
    Q_OBJECT
public:
    ADSOperation();

public :
    /** 删除本程序写入的全部标签流（保留第三方 ADS / Zone.Identifier），返回是否全部成功 */
    static bool deleteADSFiles(const QString &filePath);
    static QStringList listADSFileName(const QString &filePath);
    static bool isHostFile(const QString &sFilePath, const QList<QSet<QString>> &qLLabelGroups, labelLogic logic);

    /**
     ** 把用户输入的标签名净化为合法的 NTFS 流名：
     ** 替换 / \ : * ? " < > | 及五个分隔符为下划线，控制字符替换为下划线，
     ** 去掉未配对的代理项、首尾空白，并截断到 255 个字符。
     ** 界面层在把标签写入【标签库】时也必须调用它，否则库里会出现
     ** "a:b?c" 而磁盘上是 "a_b_c" 的不一致（标签名对不上）。
     **/
    static QString sanitizeTagName(const QString &name);

    /**
     ** 按用户录入的顺序列出标签。
     ** NTFS 枚举流时按流名排序，无法反映录入顺序，因此写入时把顺序号存进流内容；
     ** 本函数读取顺序号后排序，没有顺序号的历史标签排在后面、内部按流名排序。
     **/
    static QStringList listADSTagsInOrder(const QString &filePath);

    /** 用给定顺序重写文件的全部标签；失败时尽量回滚并返回 false */
    static bool replaceTags(const QString &filePath, const QStringList &tags);

    /** 在已有标签之后追加新标签（去重），失败返回 false；已有标签不会因失败丢失 */
    static bool appendTags(const QString &filePath, const QStringList &tags);

    /** 把文件上的某个标签改名（保留顺序）；文件上没有该标签时返回 true（标签库改名后可选同步） */
    static bool renameTag(const QString &filePath, const QString &sOldTag, const QString &sNewTag);

    /** 从文件上移除某个标签（保留其余标签顺序）；文件上没有该标签时返回 true（标签库删除后可选同步） */
    static bool removeTag(const QString &filePath, const QString &sTag);
};

#endif // ADSOPERATION_H
