#include "tst_xmloperation.h"

#include <QtTest>
#include <QDir>
#include <QFile>

namespace {

LabelInfo makeLabel(const QString &sName, const QString &sUuid)
{
    LabelInfo info;
    info.sName = sName;
    info.sUuid = sUuid;
    return info;
}

}   // namespace

void TstXmlOperation::initTestCase()
{
    QVERIFY2(m_tempDir.isValid(), "无法创建临时目录");
}

void TstXmlOperation::compareTree(const QList<LabelInfo> &qLExpected, const QList<LabelInfo> &qLActual)
{
    QCOMPARE(qLActual.size(), qLExpected.size());
    for (int i = 0; i < qLExpected.size(); ++i) {
        QCOMPARE(qLActual.at(i).sName, qLExpected.at(i).sName);
        QCOMPARE(qLActual.at(i).sUuid, qLExpected.at(i).sUuid);
        compareTree(qLExpected.at(i).lstChild, qLActual.at(i).lstChild);
    }
}

void TstXmlOperation::roundTripKeepsTree()
{
    // 两层结构：AAAA / BBBB(含两个子标签) / CCCC
    LabelInfo a = makeLabel(QStringLiteral("AAAA"), QStringLiteral("uuid-a"));
    LabelInfo b = makeLabel(QStringLiteral("BBBB"), QStringLiteral("uuid-b"));
    b.lstChild << makeLabel(QStringLiteral("子1"), QStringLiteral("uuid-b1"))
               << makeLabel(QStringLiteral("子2"), QStringLiteral("uuid-b2"));
    b.lstChild.first().lstChild << makeLabel(QStringLiteral("孙"), QStringLiteral("uuid-b1x"));
    LabelInfo c = makeLabel(QStringLiteral("CCCC"), QStringLiteral("uuid-c"));

    QList<LabelInfo> qLTree;
    qLTree << a << b << c;

    const QString sFile = QDir(m_tempDir.path()).filePath(QStringLiteral("labels.xml"));
    XmlOperation op;
    QVERIFY(op.saveXml(sFile.toUtf8().constData(), qLTree));

    const QList<LabelInfo> qLLoaded = op.readLabelFromXmlFile(sFile.toUtf8().constData());
    compareTree(qLTree, qLLoaded);
}

void TstXmlOperation::legacyWrapperFormatIsFlattened()
{
    // 旧版本写出的是"子标签外面多包一层没有 id/name 的 <label>"
    const QString sFile = QDir(m_tempDir.path()).filePath(QStringLiteral("legacy.xml"));
    QFile file(sFile);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<Root>\n"
               "  <label id=\"p\" name=\"父标签\"><label>"
               "<label id=\"c1\" name=\"子A\"></label>"
               "<label id=\"c2\" name=\"子B\"></label>"
               "</label></label>\n"
               "</Root>\n");
    file.close();

    XmlOperation op;
    const QList<LabelInfo> qLLoaded = op.readLabelFromXmlFile(sFile.toUtf8().constData());
    QCOMPARE(qLLoaded.size(), 1);
    QCOMPARE(qLLoaded.at(0).sName, QStringLiteral("父标签"));
    QCOMPARE(qLLoaded.at(0).lstChild.size(), 2);
    QCOMPARE(qLLoaded.at(0).lstChild.at(0).sName, QStringLiteral("子A"));
    QCOMPARE(qLLoaded.at(0).lstChild.at(1).sName, QStringLiteral("子B"));
}

void TstXmlOperation::missingFileReturnsEmpty()
{
    XmlOperation op;
    const QString sFile = QDir(m_tempDir.path()).filePath(QStringLiteral("不存在.xml"));
    QVERIFY(op.readLabelFromXmlFile(sFile.toUtf8().constData()).isEmpty());
}

void TstXmlOperation::corruptFileIsBackedUpAndReturnsEmpty()
{
    const QString sFile = QDir(m_tempDir.path()).filePath(QStringLiteral("broken.xml"));
    QFile file(sFile);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("<Root><label id='x' name='没闭合'");
    file.close();

    XmlOperation op;
    const QList<LabelInfo> qLLoaded = op.readLabelFromXmlFile(sFile.toUtf8().constData());
    QVERIFY(qLLoaded.isEmpty());

    // 解析失败时要留一份 .bad-时间戳 备份，避免原文件被悄悄丢掉
    const QStringList qLFilters = QStringList() << QStringLiteral("broken.xml.bad-*");
    const QFileInfoList qLBad = QDir(m_tempDir.path()).entryInfoList(qLFilters, QDir::Files);
    QVERIFY2(!qLBad.isEmpty(), "解析失败没有生成 .bad 备份");
}
