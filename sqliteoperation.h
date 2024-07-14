#ifndef SQLITEOPERATION_H
#define SQLITEOPERATION_H

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlDatabase>
#include <QSqlQueryModel>

#define TABLE_NAME_FILEPATH_LABEL "filewithlabels"
#define TABLE_NAME_DIR "dirs"

class SqliteOperation
{
public:
    SqliteOperation();
    ~SqliteOperation();

    void openDB();
    void createTable();
    void clearTable(const QString &sTableName);
    void clearLabels(const QString &sFilePath);
    void insertRecord(QMap<QString, QMap<QString, QStringList>> mapDirAndmapHostFilesAndLabel);
    void deleteRecord(const QString &sFilePath, const QString &sLabel);
    QStringList searchFileByLabel(QStringList qLLabels, int nLabelLogic);
    /** 根据给定的标签和逻辑关系，在选中的文件夹中查找文件 */
    void searchFilesByLabels(QStringList qLSelDirs, QStringList qLLabels, int nLabelLogic, QStringList &qLFilePaths, QStringList &qLNotHitedDirs);
    QVector<QString> getAllLabels();

public:
    QSqlDatabase    m_sqlDB;
};

#endif // SQLITEOPERATION_H
