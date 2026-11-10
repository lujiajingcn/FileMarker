#include "tst_tagexport.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "adsoperation.h"
#include "tagexport.h"

void TstTagExport::initTestCase()
{
    QVERIFY2(m_tempDir.isValid(), "无法创建临时目录");
}

QString TstTagExport::makeTaggedFile(const QString &sDir, const QString &sName, const QStringList &qLTags)
{
    const QString sPath = QDir(sDir).filePath(sName);
    QFile file(sPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("x");
        file.close();
    }
    if (!qLTags.isEmpty())
        ADSOperation::replaceTags(sPath, qLTags);
    return sPath;
}

void TstTagExport::writeReadJsonRoundTrip()
{
    QList<TagExport::FileTags> qLRecords;
    TagExport::FileTags r1;
    r1.sPath = QStringLiteral("E:/data/甲.txt");
    r1.sRel = QStringLiteral("甲.txt");
    r1.qLTags = QStringList() << QStringLiteral("电子书") << QStringLiteral("户外");
    TagExport::FileTags r2;
    r2.sPath = QStringLiteral("E:/data/子目录/乙.txt");
    r2.sRel = QStringLiteral("子目录/乙.txt");
    r2.qLTags = QStringList() << QStringLiteral("含逗号的,标签");
    qLRecords << r1 << r2;

    const QString sJson = QDir(m_tempDir.path()).filePath(QStringLiteral("backup.json"));
    QString sError;
    QVERIFY2(TagExport::writeJson(sJson, qLRecords, QStringLiteral("E:/data"), &sError),
             qPrintable(sError));

    QList<TagExport::FileTags> qLLoaded;
    QString sRoot;
    QVERIFY2(TagExport::readJson(sJson, &qLLoaded, &sRoot, &sError), qPrintable(sError));

    QCOMPARE(sRoot, QStringLiteral("E:/data"));
    QCOMPARE(qLLoaded.size(), 2);
    QCOMPARE(qLLoaded.at(0).sPath, r1.sPath);
    QCOMPARE(qLLoaded.at(0).sRel, r1.sRel);
    QCOMPARE(qLLoaded.at(0).qLTags, r1.qLTags);
    // 标签名里带逗号也要原样往返
    QCOMPARE(qLLoaded.at(1).qLTags, r2.qLTags);
}

void TstTagExport::readJsonRejectsBrokenFile()
{
    const QString sJson = QDir(m_tempDir.path()).filePath(QStringLiteral("broken.json"));
    QFile file(sJson);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("{ \"files\": [ {\"path\": ");
    file.close();

    QList<TagExport::FileTags> qLLoaded;
    QString sRoot;
    QString sError;
    QVERIFY(!TagExport::readJson(sJson, &qLLoaded, &sRoot, &sError));
    QVERIFY(!sError.isEmpty());
}

void TstTagExport::readJsonToleratesMissingOptionalFields()
{
    // 只写最低限度的字段：没有 rel、没有 root，也不能读失败
    const QString sJson = QDir(m_tempDir.path()).filePath(QStringLiteral("minimal.json"));
    QFile file(sJson);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("{\"app\":\"FileMarker\",\"files\":[{\"path\":\"E:/x/a.txt\",\"tags\":[\"A\"]}]}");
    file.close();

    QList<TagExport::FileTags> qLLoaded;
    QString sRoot;
    QString sError;
    QVERIFY2(TagExport::readJson(sJson, &qLLoaded, &sRoot, &sError), qPrintable(sError));
    QCOMPARE(qLLoaded.size(), 1);
    QCOMPARE(qLLoaded.at(0).qLTags, QStringList() << QStringLiteral("A"));
    // rel / root / fileCount 都缺省：rel 为空、root 为空，但不影响写回
    QVERIFY(qLLoaded.at(0).sRel.isEmpty());
    QVERIFY(sRoot.isEmpty());
}

void TstTagExport::applyOverwriteAndAppend()
{
    const QString sDir = m_tempDir.path();
    const QString sFile = makeTaggedFile(sDir, QStringLiteral("apply.txt"),
                                         QStringList() << QStringLiteral("旧") );

    TagExport::FileTags record;
    record.sPath = sFile;
    record.sRel = QStringLiteral("apply.txt");
    record.qLTags = QStringList() << QStringLiteral("新一") << QStringLiteral("新二");

    // 覆盖：文件上只剩清单里的两个标签
    TagExport::ApplyResult r1 = TagExport::applyRecords(QList<TagExport::FileTags>() << record,
                                                        true, QString(), nullptr);
    QCOMPARE(r1.nApplied, 1);
    QCOMPARE(ADSOperation::listADSTagsInOrder(sFile), record.qLTags);

    // 追加：原有标签保留，新标签接在后面
    QVERIFY(ADSOperation::replaceTags(sFile, QStringList() << QStringLiteral("旧")));
    TagExport::ApplyResult r2 = TagExport::applyRecords(QList<TagExport::FileTags>() << record,
                                                        false, QString(), nullptr);
    QCOMPARE(r2.nApplied, 1);
    QCOMPARE(ADSOperation::listADSTagsInOrder(sFile),
             QStringList() << QStringLiteral("旧") << QStringLiteral("新一")
                           << QStringLiteral("新二"));
}

void TstTagExport::applyUsesFallbackRootForMissingPaths()
{
    const QString sDir = QDir(m_tempDir.path()).filePath(QStringLiteral("moved"));
    QDir().mkpath(sDir);
    const QString sFile = makeTaggedFile(sDir, QStringLiteral("reloc.txt"), QStringList());

    // 清单里记的是"搬走之前"的绝对路径
    TagExport::FileTags record;
    record.sPath = QStringLiteral("E:/already/gone/reloc.txt");
    record.sRel = QStringLiteral("reloc.txt");
    record.qLTags = QStringList() << QStringLiteral("找回来");

    TagExport::ApplyResult r1 = TagExport::applyRecords(QList<TagExport::FileTags>() << record,
                                                        true, QString(), nullptr);
    QCOMPARE(r1.nSkipped, 1);   // 没有 fallback 根：跳过
    QVERIFY(ADSOperation::listADSTagsInOrder(sFile).isEmpty());

    // 给出新根目录后应该能按相对路径找回来
    TagExport::ApplyResult r2 = TagExport::applyRecords(QList<TagExport::FileTags>() << record,
                                                        true, sDir, nullptr);
    QCOMPARE(r2.nApplied, 1);
    QCOMPARE(ADSOperation::listADSTagsInOrder(sFile), record.qLTags);
}

void TstTagExport::collectOnlyTaggedFiles()
{
    const QString sDir = QDir(m_tempDir.path()).filePath(QStringLiteral("scan"));
    QDir().mkpath(sDir);
    makeTaggedFile(sDir, QStringLiteral("有标签.txt"), QStringList() << QStringLiteral("A"));
    makeTaggedFile(sDir, QStringLiteral("没标签.txt"), QStringList());

    QString sError;
    const QList<TagExport::FileTags> qLRecords =
        TagExport::collectFromDirs(QStringList() << sDir, nullptr, &sError);

    QVERIFY2(sError.isEmpty(), qPrintable(sError));
    QCOMPARE(qLRecords.size(), 1);
    QCOMPARE(QFileInfo(qLRecords.at(0).sPath).fileName(), QStringLiteral("有标签.txt"));
    // 每一条都要有相对路径，目录整体搬走时才有救
    QCOMPARE(qLRecords.at(0).sRel, QStringLiteral("有标签.txt"));
}
