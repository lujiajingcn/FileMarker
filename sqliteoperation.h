#ifndef SQLITEOPERATION_H
#define SQLITEOPERATION_H

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlDatabase>
#include <QSqlQueryModel>

#include "common.h"

#define TABLE_NAME_FILEPATH_TAG "filewithlabels"
#define TABLE_NAME_DIR "dirs"

class SqliteOperation
{
public:
    SqliteOperation();
    ~SqliteOperation();

    // P2-14：禁用拷贝构造和赋值，避免浅拷贝 QSqlDatabase 引用导致未定义行为。
    SqliteOperation(const SqliteOperation&) = delete;
    SqliteOperation& operator=(const SqliteOperation&) = delete;

    void openDB();
    void createTable();
    /** 兼容旧版本缓存：把历史上以 "tag:$DATA" 形式写入的标签统一去掉后缀 */
    void migrateLegacyLabels();
    /**
     * P1-1：qLCompletedDirs 是"被完整遍历过"的目录（可能没有带标签的文件），用于登记 dirs 表；
     * 只传完整目录，未扫完的目录绝不能进缓存。
     */
    void insertRecord(QMap<QString, FILE_TAGS> dirAndFileTags, const QStringList &qLCompletedDirs = QStringList());
    /** 根据给定的标签和逻辑关系，在选中的文件夹中查找文件 */
    void searchFilesByLabels(QStringList qLSelDirs, QStringList qLLabels, int nLabelLogic, QStringList &qLFilePaths, QStringList &qLNotHitedDirs);

    /** 标签变化后让该目录的搜索缓存失效，下次搜索重新遍历，避免按旧标签命中/漏掉新标签 */
    void invalidateDirCache(const QString &sDir);

    /**
     * P1-2：缓存按"遍历根"存，而文件的所在目录可能是这个根的子孙；
     * 某个文件的标签变化后，要把所有包含它的根目录一起失效，否则缓存会一直命中旧标签。
     */
    void invalidateCacheForFile(const QString &sFilePath);

public:
    QSqlDatabase    m_sqlDB;
};

#endif // SQLITEOPERATION_H
