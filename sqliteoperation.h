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
    void insertRecord(QMap<QString, FILE_TAGS> dirAndFileTags);
    /** 根据给定的标签和逻辑关系，在选中的文件夹中查找文件 */
    void searchFilesByLabels(QStringList qLSelDirs, QStringList qLLabels, int nLabelLogic, QStringList &qLFilePaths, QStringList &qLNotHitedDirs);

public:
    QSqlDatabase    m_sqlDB;
};

#endif // SQLITEOPERATION_H
