#ifndef SQLITEOPERATION_H
#define SQLITEOPERATION_H

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlDatabase>
#include <QSqlQueryModel>

#include "common.h"
#include <QSet>

#define TABLE_NAME_FILEPATH_TAG "filewithlabels"
#define TABLE_NAME_DIR "dirs"

class SqliteOperation
{
public:
    SqliteOperation();
    ~SqliteOperation();

    // 禁用拷贝构造和赋值，避免浅拷贝 QSqlDatabase 引用导致未定义行为。
    SqliteOperation(const SqliteOperation&) = delete;
    SqliteOperation& operator=(const SqliteOperation&) = delete;

    void openDB();
    void createTable();
    /** 兼容旧版本缓存：把历史上以 "tag:$DATA" 形式写入的标签统一去掉后缀 */
    void migrateLegacyLabels();
    /**
     * qLCompletedDirs 是"被完整遍历过"的目录（可能没有带标签的文件），用于登记 dirs 表；
     * 只传完整目录，未扫完的目录绝不能进缓存。
     */
    void insertRecord(QMap<QString, FILE_TAGS> dirAndFileTags, const QStringList &qLCompletedDirs = QStringList());
    /**
     * 根据给定的标签"组"和逻辑关系，在选中的文件夹中查找文件。
     * 每个组 = 用户选中的一个标签 + 它的所有子标签；组内 OR、组间按 nLabelLogic 组合。
     * 这样"搜索父标签能搜到打了子标签的文件"，而 AND 仍然是"每个条件都要命中"。
     */
    void searchFilesByLabels(QStringList qLSelDirs,
                             const QList<QSet<QString>> &qLLabelGroups,
                             int nLabelLogic,
                             QStringList &qLFilePaths,
                             QStringList &qLNotHitedDirs);

    /** 标签变化后让该目录的搜索缓存失效，下次搜索重新遍历，避免按旧标签命中/漏掉新标签 */
    void invalidateDirCache(const QString &sDir);

    /**
     * 缓存按"遍历根"存，而文件的所在目录可能是这个根的子孙；
     * 某个文件的标签变化后，要把所有包含它的根目录一起失效，否则缓存会一直命中旧标签。
     */
    void invalidateCacheForFile(const QString &sFilePath);

    /**
     * 列出缓存中"带这个标签"的文件（按标签全名精确匹配，大小写不敏感）。
     * 用于【标签库】改名/删除时告诉用户"有多少文件在用"，以及可选地同步到文件。
     * 注意：只覆盖被遍历过的目录，未遍历的文件不在其中。
     */
    QStringList filesUsingLabel(const QString &sLabel) const;

    /** 这批目录里最旧的一份缓存是什么时候写入的；无效 QDateTime 表示查不到 */
    QDateTime oldestCacheTime(const QStringList &qLDirs) const;

public:
    QSqlDatabase    m_sqlDB;
};

#endif // SQLITEOPERATION_H
