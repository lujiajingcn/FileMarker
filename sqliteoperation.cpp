#include "sqliteoperation.h"
#include <common.h>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include "utility.h"

SqliteOperation::SqliteOperation()
{
    openDB();
    createTable();
    migrateLegacyLabels();
}

SqliteOperation::~SqliteOperation()
{
    // P0-4：close 后必须 removeDatabase 清理全局数据库连接表，否则再次 addDatabase 会
    // 返回已关闭的旧连接并产生 "driver is already in use" 警告。
    // 注意：默认连接（addDatabase 未指定名称）的 connectionName() 是空字符串，
    // 因此不能以 connectionName() 是否为空作为是否清理的判断条件。
    QString connName = m_sqlDB.connectionName();  // 默认连接为空串 ""
    m_sqlDB.close();
    // removeDatabase 要求没有其它 QSqlDatabase 实例引用该连接。
    // 先释放本对象成员持有的引用，再移除连接，否则退出时会警告
    // "connection is still in use, all queries will cease to work"。
    m_sqlDB = QSqlDatabase();
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

void SqliteOperation::migrateLegacyLabels()
{
    // 用 user_version 记录迁移，避免每次启动都全表 UPDATE
    QSqlQuery versionQuery(m_sqlDB);
    if (versionQuery.exec(QStringLiteral("PRAGMA user_version")) && versionQuery.next()
            && versionQuery.value(0).toInt() >= 1) {
        return;
    }

    // 旧版本把 FindFirstStreamW 返回的 "tag:$DATA" 原样写进 labels。
    // 新版按 "tag" 搜索时精确比较会全部落空，这里一次性去掉缓存里的后缀。
    QSqlQuery query(m_sqlDB);
    if (!query.exec(QStringLiteral("UPDATE %1 SET labels = REPLACE(labels, ':$DATA', '')")
                        .arg(TABLE_NAME_FILEPATH_TAG))) {
        qWarning() << "迁移旧标签缓存失败:" << query.lastError().text();
        return;
    }
    if (query.numRowsAffected() > 0)
        qInfo() << "已迁移旧标签缓存:" << query.numRowsAffected() << "行";

    QSqlQuery markQuery(m_sqlDB);
    if (!markQuery.exec(QStringLiteral("PRAGMA user_version = 1")))
        qWarning() << "写入迁移版本失败:" << markQuery.lastError().text();
}

void SqliteOperation::invalidateDirCache(const QString &sDir)
{
    if (sDir.isEmpty())
        return;

    QSqlQuery query(m_sqlDB);
    query.prepare(QString("DELETE FROM %1 WHERE dir=:dir").arg(TABLE_NAME_FILEPATH_TAG));
    query.bindValue(":dir", sDir);
    if (!query.exec())
        qWarning() << "清除文件标签缓存失败:" << sDir << query.lastError().text();

    QSqlQuery dirQuery(m_sqlDB);
    dirQuery.prepare(QString("DELETE FROM %1 WHERE dir=:dir").arg(TABLE_NAME_DIR));
    dirQuery.bindValue(":dir", sDir);
    if (!dirQuery.exec())
        qWarning() << "清除目录缓存失败:" << sDir << dirQuery.lastError().text();
}

QStringList SqliteOperation::filesUsingLabel(const QString &sLabel) const
{
    QStringList qLFiles;
    if (sLabel.isEmpty())
        return qLFiles;

    QSqlDatabase db = m_sqlDB;
    QSqlQuery query(db);
    if (!query.exec(QString("SELECT filepath,labels FROM %1").arg(TABLE_NAME_FILEPATH_TAG))) {
        qWarning() << "查询使用标签的文件失败:" << query.lastError().text();
        return qLFiles;
    }
    while (query.next()) {
        QString sLabels = query.value("labels").toString();
        sLabels.remove(QStringLiteral(":$DATA"), Qt::CaseInsensitive);
        const QStringList qLLabelsOfFile = sLabels.split(QLatin1Char(','));
        for (const QString &sOne : qLLabelsOfFile) {
            if (sOne.compare(sLabel, Qt::CaseInsensitive) == 0) {
                qLFiles << query.value("filepath").toString();
                break;
            }
        }
    }
    qLFiles.removeDuplicates();
    return qLFiles;
}

void SqliteOperation::invalidateCacheForFile(const QString &sFilePath)
{
    if (sFilePath.isEmpty())
        return;

    const QString sFileDir = QDir::cleanPath(QFileInfo(sFilePath).absolutePath());

    // 缓存行的 dir 列记录的是"遍历根"，它可能是该文件的任意祖先目录；
    // 只删 absolutePath() 会让"遍历了父目录、改的是子目录里的文件"这种情况永远失效不掉（P1-2）。
    QStringList qLRoots;
    QSqlQuery query(m_sqlDB);
    if (query.exec(QString("SELECT dir FROM %1").arg(TABLE_NAME_DIR))) {
        while (query.next()) {
            const QString sRoot = QDir::cleanPath(query.value(0).toString());
            if (sRoot.isEmpty())
                continue;
            // 比较前去掉末尾分隔符：遍历根可能是 "E:/" 这种盘符根，
            // 直接拼 '/' 会变成 "E://"，子目录就永远匹配不上。
            QString sRootCmp = sRoot;
            while (sRootCmp.endsWith(QLatin1Char('/')) && sRootCmp.size() > 1)
                sRootCmp.chop(1);
            if (sFileDir.compare(sRootCmp, Qt::CaseInsensitive) == 0
                    || sFileDir.startsWith(sRootCmp + QLatin1Char('/'), Qt::CaseInsensitive)) {
                qLRoots << sRoot;      // 删除时用库里存的原始写法
            }
        }
    } else {
        qWarning() << "读取缓存目录列表失败:" << query.lastError().text();
    }

    // 兜底：dirs 表为空或读取失败时，至少按文件父目录清一次
    if (qLRoots.isEmpty())
        qLRoots << sFileDir;

    for (const QString &sRoot : qLRoots)
        invalidateDirCache(sRoot);
}

void SqliteOperation::insertRecord(QMap<QString, FILE_TAGS> dirAndFileTags, const QStringList &qLCompletedDirs)
{
    // P1-1：dirs 表必须和文件行在同一个事务里。否则文件行回滚了、目录却仍标着"已缓存"，
    // 之后按缓存搜索会命中"有目录、没数据"的空壳，永远查不到文件。
    if (!m_sqlDB.transaction()) {
        QString sErrorMsg = QString("Transaction failed to start:%1").arg(m_sqlDB.lastError().text());
        qCritical("%s", sErrorMsg.toStdString().c_str());
        return;
    }

    // 要登记的目录 = 完整遍历过的目录 + 结果里出现过的目录（去重）。
    // 完整遍历但一个带标签文件都没有的目录也要登记，这样缓存命中时不必反复扫盘。
    QStringList qLAllDirs = qLCompletedDirs;
    for (QMap<QString, FILE_TAGS>::const_iterator cItr = dirAndFileTags.begin(); cItr != dirAndFileTags.end(); cItr++) {
        if (!qLAllDirs.contains(cItr.key()))
            qLAllDirs << cItr.key();
    }

    QSqlQuery query(m_sqlDB);
    // 使用 INSERT OR IGNORE 写入目录，dir 列有 UNIQUE 约束，重复遍历同一目录不会报错。
    query.prepare(QString("INSERT OR IGNORE INTO %1 (dir) VALUES (:dir)").arg(TABLE_NAME_DIR));
    for (int i = 0; i < qLAllDirs.count(); i++) {
        query.bindValue(":dir", qLAllDirs.at(i));
        if(!query.exec()) {
            QString sErrorMsg = QString("向表dirs中插入数据失败：%1").arg(query.lastError().text());
            qCritical("%s", sErrorMsg.toStdString().c_str());
            m_sqlDB.rollback();
            return;
        }
    }

    {
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
        }
    }
}

// A4：按"标签组"判断一行缓存是否命中。
// 组内是 OR（选了父标签就等于选了它整棵子树），组间按 AND/OR 组合。
static bool isFileMatchLabelGroups(const QSet<QString> &setFileLabels,
                                   const QList<QSet<QString>> &qLLabelGroups,
                                   int nLabelLogic)
{
    if (qLLabelGroups.isEmpty())
        return false;

    // 【NOT】逻辑非：文件上不能出现任何一组里的标签
    if (nLabelLogic == labelLogic::NOT) {
        for (const QSet<QString> &setGroup : qLLabelGroups) {
            for (QSet<QString>::const_iterator cItr = setGroup.cbegin(); cItr != setGroup.cend(); ++cItr) {
                if (setFileLabels.contains(*cItr))
                    return false;
            }
        }
        return true;
    }

    if (nLabelLogic == labelLogic::OR) {
        for (const QSet<QString> &setGroup : qLLabelGroups) {
            for (QSet<QString>::const_iterator cItr = setGroup.cbegin(); cItr != setGroup.cend(); ++cItr) {
                if (setFileLabels.contains(*cItr))
                    return true;
            }
        }
        return false;
    }

    for (const QSet<QString> &setGroup : qLLabelGroups) {
        bool bHit = false;
        for (QSet<QString>::const_iterator cItr = setGroup.cbegin(); cItr != setGroup.cend(); ++cItr) {
            if (setFileLabels.contains(*cItr)) {
                bHit = true;
                break;
            }
        }
        if (!bHit)
            return false;
    }
    return true;
}

void SqliteOperation::searchFilesByLabels(QStringList qLSelDirs, const QList<QSet<QString>> &qLLabelGroups, int nLabelLogic, QStringList &qLFilePaths, QStringList &qLNotHitedDirs)
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

    // 【重要】Qt 5 的 QSet::intersect()/subtract() 只有"就地修改"的版本（返回自身引用），
    // 直接写 stSelDirs.intersect(stSearchedDirs) 会把 stSelDirs 本身改成交集（这里就是空集），
    // 于是后面的 subtract 永远是空 —— "未命中目录"恒为 0，缓存没覆盖的目录根本不会被磁盘查询，
    // 搜索结果直接变空。这里显式拷贝一份再求交/求差。
    const QSet<QString> stSelDirs = Utility::toSet(qLSelDirs);
    const QSet<QString> stSearchedDirs = Utility::toSet(qLSearchedDirs);
    QSet<QString> stHitedDirs = stSelDirs;              // 选中的文件夹命中缓存的部分
    stHitedDirs.intersect(stSearchedDirs);
    QSet<QString> stNotHitedDirs = stSelDirs;           // 未命中缓存的部分，后续直接从磁盘查找
    stNotHitedDirs.subtract(stSearchedDirs);
    for (QSet<QString>::const_iterator cItr = stNotHitedDirs.begin(); cItr != stNotHitedDirs.end(); cItr++)
    {
        qLNotHitedDirs << *cItr;
    }

    qDebug() << "[缓存查询] 选中=" << qLSelDirs << "已缓存=" << qLSearchedDirs
            << "命中=" << stHitedDirs << "未命中=" << qLNotHitedDirs;

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
            // 防御性兼容：即使迁移失败/旧库只读，也按去掉 :$DATA 后的名字比较
            sLabels.remove(QStringLiteral(":$DATA"), Qt::CaseInsensitive);
            const QSet<QString> setLabelsOfFilePath = Utility::toSet(sLabels.split(QLatin1Char(',')));
            if (isFileMatchLabelGroups(setLabelsOfFilePath, qLLabelGroups, nLabelLogic)
                    && QFileInfo::exists(sFilePath)) {
                qLFilePaths << sFilePath;
            }
        }
        hitQuery.finish();
    }
}
