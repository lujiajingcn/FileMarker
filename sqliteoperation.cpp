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
    m_sqlDB.close();
}

void SqliteOperation::openDB()
{
    m_sqlDB = QSqlDatabase::addDatabase("QSQLITE");
    m_sqlDB.setDatabaseName("FileMarker.db");
    if(!m_sqlDB.open())
    {
        QString sErrorMsg = QString("open database fails!").arg(m_sqlDB.lastError().text());
        qFatal("%s", sErrorMsg.toStdString().c_str());
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
        QString sErrorMsg = QString("create table filewithlabels fails:").arg(query.lastError().text());
        qFatal("%s", sErrorMsg.toStdString().c_str());
    }

    QSqlQuery query1(m_sqlDB);
    QString sSql1 = R"(
                      CREATE TABLE IF NOT EXISTS  dirs (
                      id    INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
                      dir   CHAR (260) UNIQUE NOT NULL)
                      )";
    if(!query1.exec(sSql1))
    {
        QString sErrorMsg = QString("create table dirs fails:").arg(query.lastError().text());
        qFatal("%s", sErrorMsg.toStdString().c_str());
    }
}

void SqliteOperation::clearTable(const QString &sTableName)
{
    QSqlQuery query(m_sqlDB);
    QString sSql = QString("DELETE FROM %1").arg(sTableName);
    bool bRet = query.exec(sSql);
    if(!bRet)
    {
        qCritical("删除数据失败！");
    }
}

void SqliteOperation::clearLabels(const QString &sFilePath)
{
    QSqlQuery query(m_sqlDB);
    QString sSql = QString("DELETE FROM filewithlabels WHERE filepath='%1'")
                   .arg(sFilePath);
    bool bRet = query.exec(sSql);
    if(!bRet)
    {
        qCritical("删除数据失败！");
    }
}

void SqliteOperation::insertRecord(QMap<QString, QMap<QString, QStringList>> mapDirAndmapHostFilesAndLabel)
{
    QSqlQuery query(m_sqlDB);
    for(QMap<QString, QMap<QString, QStringList>>::const_iterator cItr = mapDirAndmapHostFilesAndLabel.begin(); cItr != mapDirAndmapHostFilesAndLabel.end(); cItr++)
    {
        QString sSelDir = cItr.key();
        QString sSql = QString("INSERT INTO %1 (dir) VALUES ('%2')").arg(TABLE_NAME_DIR).arg(sSelDir);
        bool bRet = query.exec(sSql);
        if(!bRet)
        {
            QString sErrorMsg = QString("向表dirs中插入数据失败：%2").arg(query.lastError().text());
            qCritical("%s", sErrorMsg.toStdString().c_str());
        }
    }
    if (m_sqlDB.transaction())
    {
        QSqlQuery query(m_sqlDB);
        query.prepare(QString("INSERT INTO %1 (filepath, labels, dir) VALUES (:filepath, :labels, :dir)").arg(TABLE_NAME_FILEPATH_LABEL));
        for(QMap<QString, QMap<QString, QStringList>>::const_iterator cItr = mapDirAndmapHostFilesAndLabel.begin(); cItr != mapDirAndmapHostFilesAndLabel.end(); cItr++)
        {
            QString sSelDir = cItr.key();
            for(QMap<QString, QStringList>::const_iterator cItrChild = cItr.value().begin(); cItrChild != cItr.value().end(); cItrChild++)
            {
                query.bindValue(":filepath", cItrChild.key());
                query.bindValue(":labels", cItrChild.value().join(","));
                query.bindValue(":dir", sSelDir);
                if (!query.exec())
                {
                    QString sErrorMsg = QString("向表%1中插入数据失败：%2").arg(TABLE_NAME_FILEPATH_LABEL).arg(query.lastError().text());
                    qCritical("%s", sErrorMsg.toStdString().c_str());
                    m_sqlDB.rollback();
                    return;
                }
            }
        }

        if (!m_sqlDB.commit()) {
            QString sErrorMsg = QString("Transaction commit failed:%1").arg(query.lastError().text());
            qCritical("%s", sErrorMsg.toStdString().c_str());
            m_sqlDB.rollback();
            return;
        }
    }
    else
    {
        QString sErrorMsg = QString("Transaction failed to start:%1").arg(query.lastError().text());
        qCritical("%s", sErrorMsg.toStdString().c_str());
    }
}

void SqliteOperation::deleteRecord(const QString &sFilePath, const QString &sLabel)
{
    QSqlQuery query(m_sqlDB);
    QString sSql = QString("DELETE FROM filewithlabels WHERE filepath='%1' and labels='%2'")
                   .arg(sFilePath).arg(sLabel);
    bool bRet = query.exec(sSql);
    if(!bRet)
    {
        qCritical("删除数据失败！");
    }

    sSql = QString("DELETE FROM alllabels WHERE label='%2'")
           .arg(sLabel);
    bRet = query.exec(sSql);
    if(!bRet)
    {
        qCritical("删除数据失败！");
    }
}

QStringList SqliteOperation::searchFileByLabel(QStringList qLLabels, int nLabelLogic)
{
    QStringList qLFilesPath;
    QSqlQuery query(m_sqlDB);
    if(nLabelLogic == labelLogic::AND)
    {
        query.exec(QString("SELECT filepath,labels FROM filewithlabels WHERE labels='%1'").arg(qLLabels.join(",")));
        while(query.next())
        {
            qLFilesPath << query.value("filepath").toString();
            QString sLables = query.value("labels").toString();
            QStringList qLLabelsOfFilePath = sLables.split(",");

        }
        return qLFilesPath;
    }
    else
    {
        query.exec(QString("SELECT filepath,labels FROM filewithlabels"));
        while(query.next())
        {
            QString sLabels = query.value("labels").toString();
            QStringList qLAttachedLabels = sLabels.split(",");
            if(Utility::hasEqualElement(qLAttachedLabels, qLLabels))
            {
                qLFilesPath << query.value("filepath").toString();
                break;
            }
        }
        return qLFilesPath;
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

    // todo 不知道根据文件夹条件多次查询跟不带条件的单词查询，哪个速度更快
    for(QSet<QString>::const_iterator cItr = stHitedDirs.begin(); cItr != stHitedDirs.end(); cItr++)
    {
        query.exec(QString("SELECT filepath,labels FROM filewithlabels WHERE dir='%1'").arg(*cItr));
        while(query.next())
        {
            QString sFilePath = query.value("filepath").toString();
            QString sLabels = query.value("labels").toString();
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
        query.finish();
    }
}
