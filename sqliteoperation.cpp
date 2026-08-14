#include "sqliteoperation.h"
#include <common.h>
#include <QDebug>
#include "utility.h"

SqliteOperation::SqliteOperation()
{
    openDB();
    createTable();
}

SqliteOperation::~SqliteOperation()
{
    // P0-4：close 后必须 removeDatabase 清理全局数据库连接表，否则再次 addDatabase 会
    // 返回已关闭的旧连接并产生 "driver is already in use" 警告。
    // 注意：默认连接（addDatabase 未指定名称）的 connectionName() 是空字符串，
    // 因此不能以 connectionName() 是否为空作为是否清理的判断条件。
    QString connName = m_sqlDB.connectionName();  // 默认连接为空串 ""
    m_sqlDB.close();
    QSqlDatabase::removeDatabase(connName);
}

void SqliteOperation::openDB()
{
    m_sqlDB = QSqlDatabase::addDatabase("QSQLITE");
    // 使用绝对路径，避免依赖进程当前工作目录（CWD 改变时数据库会打开失败）。
    m_sqlDB.setDatabaseName(g_sAppDir + "/FileMarker.db");
    if(!m_sqlDB.open())
    {
        // 不要用 qFatal 直接终止程序，改为记录错误并继续，
        // 让上层有机会提示用户而不是无声崩溃。
        qCritical("open database fails! %s", qPrintable(m_sqlDB.lastError().text()));
    }
}

void SqliteOperation::createTable()
{
    QSqlQuery query(m_sqlDB);
    QString sSql = R"(
                      CREATE TABLE IF NOT EXISTS  filewithlabels (
                      id        INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
                      dir       CHAR (260) NOT NULL,
                      filepath  CHAR (260) NOT NULL,
                      labels    TEXT (1024) NOT NULL)
                      )";
    if(!query.exec(sSql))
    {
        qCritical("create table filewithlabels fails: %s", qPrintable(query.lastError().text()));
    }

    QSqlQuery query1(m_sqlDB);
    QString sSql1 = R"(
                      CREATE TABLE IF NOT EXISTS  dirs (
                      id    INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
                      dir   CHAR (260) UNIQUE NOT NULL)
                      )";
    if(!query1.exec(sSql1))
    {
        qCritical("create table dirs fails: %s", qPrintable(query1.lastError().text()));
    }
}

void SqliteOperation::insertRecord(QMap<QString, FILE_TAGS> dirAndFileTags)
{
    QSqlQuery query(m_sqlDB);
    // 使用 INSERT OR IGNORE 写入目录，dir 列有 UNIQUE 约束，重复遍历同一目录不会报错。
    query.prepare(QString("INSERT OR IGNORE INTO %1 (dir) VALUES (:dir)").arg(TABLE_NAME_DIR));
    for(QMap<QString, FILE_TAGS>::const_iterator cItr = dirAndFileTags.begin(); cItr != dirAndFileTags.end(); cItr++) {
        query.bindValue(":dir", cItr.key());
        if(!query.exec()) {
            QString sErrorMsg = QString("向表dirs中插入数据失败：%2").arg(query.lastError().text());
            qCritical("%s", sErrorMsg.toStdString().c_str());
        }
    }

    if (m_sqlDB.transaction()) {
        // 预编译语句：删除指定目录的旧缓存。
        QSqlQuery delQuery(m_sqlDB);
        delQuery.prepare(QString("DELETE FROM %1 WHERE dir=:dir").arg(TABLE_NAME_FILEPATH_TAG));
        // 预编译语句：插入文件标签，使用 bindValue 防止单引号等特殊字符造成 SQL 语法错误。
        QSqlQuery insQuery(m_sqlDB);
        insQuery.prepare(QString("INSERT INTO %1 (filepath, labels, dir) VALUES (:filepath, :labels, :dir)").arg(TABLE_NAME_FILEPATH_TAG));

        for(QMap<QString, FILE_TAGS>::const_iterator cItr = dirAndFileTags.begin(); cItr != dirAndFileTags.end(); cItr++) {
            QString selDir = cItr.key();

            delQuery.bindValue(":dir", selDir);
            if(!delQuery.exec()) {
                QString sErrorMsg = QString("删除表%1中dir为%2的记录失败：%3").arg(TABLE_NAME_FILEPATH_TAG).arg(selDir).arg(delQuery.lastError().text());
                qCritical("%s", sErrorMsg.toStdString().c_str());
                m_sqlDB.rollback();
                return;
            }

            for(QMap<QString, QStringList>::const_iterator cItrChild = cItr.value().begin(); cItrChild != cItr.value().end(); cItrChild++) {
                insQuery.bindValue(":filepath", cItrChild.key());
                insQuery.bindValue(":labels", cItrChild.value().join(","));
                insQuery.bindValue(":dir", selDir);
                if (!insQuery.exec()) {
                    QString sErrorMsg = QString("向表%1中插入数据失败：%2").arg(TABLE_NAME_FILEPATH_TAG).arg(insQuery.lastError().text());
                    qCritical("%s", sErrorMsg.toStdString().c_str());
                    m_sqlDB.rollback();
                    return;
                }
            }
        }

        if (!m_sqlDB.commit()) {
            QString sErrorMsg = QString("Transaction commit failed:%1").arg(insQuery.lastError().text());
            qCritical("%s", sErrorMsg.toStdString().c_str());
            m_sqlDB.rollback();
            return;
        }
    } else {
        // P2-20：transaction() 失败时，query 是前面的 INSERT INTO dirs，属于无关语句。
        // 事务本身的错误应取自数据库连接 lastError()。
        QString sErrorMsg = QString("Transaction failed to start:%1").arg(m_sqlDB.lastError().text());
        qCritical("%s", sErrorMsg.toStdString().c_str());
    }
}

void SqliteOperation::searchFilesByLabels(QStringList qLSelDirs, QStringList qLLabels, int nLabelLogic, QStringList &qLFilePaths, QStringList &qLNotHitedDirs)
{
    QStringList qLSearchedDirs; // 已经缓存过的文件夹，从数据库中查找获得
    QSqlQuery query(m_sqlDB);
    query.exec(QString("SELECT dir FROM dirs"));
    while(query.next())
    {
        qLSearchedDirs << query.value("dir").toString();
    }
    query.finish();

    if(qLSearchedDirs.count() == 0)
    {
        qLNotHitedDirs = qLSelDirs;
        return;
    }

    QSet<QString> stSelDirs = qLSelDirs.toSet();
    QSet<QString> stSearchedDirs = qLSearchedDirs.toSet();
    QSet<QString> stHitedDirs = stSelDirs.intersect(stSearchedDirs);    // 选中的文件夹命中缓存的部分
    QSet<QString> stNotHitedDirs = stSelDirs.subtract(stSearchedDirs);  // 选中的文件夹未命中缓存的部分，这部分后续直接在磁盘中查找
    for (QSet<QString>::const_iterator cItr = stNotHitedDirs.begin(); cItr != stNotHitedDirs.end(); cItr++)
    {
        qLNotHitedDirs << *cItr;
    }

    // 使用预编译语句 + bindValue，避免路径中包含单引号等字符导致 SQL 语法错误。
    QSqlQuery hitQuery(m_sqlDB);
    hitQuery.prepare(QString("SELECT filepath,labels FROM filewithlabels WHERE dir=:dir"));
    for(QSet<QString>::const_iterator cItr = stHitedDirs.begin(); cItr != stHitedDirs.end(); cItr++)
    {
        hitQuery.bindValue(":dir", *cItr);
        hitQuery.exec();
        while(hitQuery.next())
        {
            QString sFilePath = hitQuery.value("filepath").toString();
            QString sLabels = hitQuery.value("labels").toString();
            if(nLabelLogic == labelLogic::AND)
            {
                QStringList qLLabelsOfFilePath = sLabels.split(",");
                QSet<QString> stLabelsOfFilePath = qLLabelsOfFilePath.toSet();
                QSet<QString> stConditionLabels = qLLabels.toSet();
                QSet<QString> stIntersect = stLabelsOfFilePath.intersect(stConditionLabels);
                QSet<QString> stSubtract = stConditionLabels.subtract(stIntersect);

                if(stSubtract.count() == 0)
                {
                    qLFilePaths << sFilePath;
                }
            }
            else
            {
                QStringList qLLabelsOfFile = sLabels.split(",");
                if(Utility::hasEqualElement(qLLabels, qLLabelsOfFile))
                {
                    qLFilePaths << sFilePath;
                }
            }
        }
        hitQuery.finish();
    }
}
