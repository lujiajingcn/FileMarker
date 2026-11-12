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
    void insertRecord(const QMap<QString, FILE_TAGS> &dirAndFileTags,
                      const QStringList &qLCompletedDirs = QStringList(),
                      const QStringList &qLSkippedDirs = QStringList(),
                      const QStringList &qLIncompleteDirs = QStringList());
    /**
     * 根据给定的标签"组"和逻辑关系，在选中的文件夹中查找文件。
     * 每个组 = 用户选中的一个标签 + 它的所有子标签；组内 OR、组间按 nLabelLogic 组合。
     * 这样"搜索父标签能搜到打了子标签的文件"，而 AND 仍然是"每个条件都要命中"。
     */
    void searchFilesByLabels(const QStringList &qLSelDirs,
                             const QList<QSet<QString>> &qLLabelGroups,
                             int nLabelLogic,
                             QStringList &qLFilePaths,
                             QStringList &qLNotHitedDirs,
                             int &nExcludedDirs);

    /** 标签变化后让该目录的搜索缓存失效，下次搜索重新遍历，避免按旧标签命中/漏掉新标签 */
    void invalidateDirCache(const QString &sDir);

    /** 找出包含该文件的缓存"遍历根"（没有则返回空） */
    QStringList cachedRootsForFile(const QString &sFilePath) const;

    /**
     * 缓存按"遍历根"存，而文件的所在目录可能是这个根的子孙；
     * 某个文件的标签变化后，要把所有包含它的根目录一起失效，否则缓存会一直命中旧标签。
     */
    void invalidateCacheForFile(const QString &sFilePath);

    /**
     * 某个文件的标签变了：**只刷新它在缓存里的那一行**（读一次 ADS 后更新/插入/删除）。
     * 不再像 invalidateCacheForFile 那样把整个"遍历根"的缓存删掉 ——
     * 否则改一个文件就得让整目录下次全盘重扫。
     */
    void refreshCachedFile(const QString &sFilePath);

    /** 批量版本：少量文件逐个精确刷新；文件很多（如导入）时退化为受影响根的整根失效 */
    void refreshCachedFiles(const QStringList &qLFiles);

    /** 文件重命名后把缓存里的路径改掉（标签不变），避免"改名后搜不到" */
    void renameCachedFile(const QString &sOldPath, const QString &sNewPath);

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
