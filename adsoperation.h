#ifndef ADSOPERATION_H
#define ADSOPERATION_H

#include <QObject>
#include "common.h"

class ADSOperation : public QObject
{
    Q_OBJECT
public:
    ADSOperation();

public :
    static void writeADSFile(const QString &filePath, const QString& streamName, const QString& text, bool isDeleteOldAds = false);
    static void deleteADSFiles(const QString &filePath);
    static QStringList listADSFileName(const QString &filePath);
    static bool isHostFile(const QString &sFilePath, const QStringList &tags, labelLogic logic);

    /**
     ** 按用户录入的顺序列出标签。
     ** NTFS 枚举流时按流名排序，无法反映录入顺序，因此写入时把顺序号存进流内容；
     ** 本函数读取顺序号后排序，没有顺序号的历史标签排在后面、内部按流名排序。
     **/
    static QStringList listADSTagsInOrder(const QString &filePath);

    /** 用给定顺序重写文件的全部标签（先清空，再按顺序写入，顺序号写入流内容） */
    static void replaceTags(const QString &filePath, const QStringList &tags);

    /** 在已有标签之后追加新标签（去重），保留已有标签的顺序，新标签按给定顺序追加 */
    static void appendTags(const QString &filePath, const QStringList &tags);
};

#endif // ADSOPERATION_H
