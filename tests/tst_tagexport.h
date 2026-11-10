#ifndef TST_TAGEXPORT_H
#define TST_TAGEXPORT_H

#include <QObject>
#include <QString>
#include <QTemporaryDir>

/** 【标签备份】JSON 读写与写回的单元测试 */
class TstTagExport : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void writeReadJsonRoundTrip();
    void readJsonRejectsBrokenFile();
    void readJsonToleratesMissingOptionalFields();
    void applyOverwriteAndAppend();
    void applyUsesFallbackRootForMissingPaths();
    void collectOnlyTaggedFiles();

private:
    QString makeTaggedFile(const QString &sDir, const QString &sName, const QStringList &qLTags);

    QTemporaryDir m_tempDir;
};

#endif // TST_TAGEXPORT_H
