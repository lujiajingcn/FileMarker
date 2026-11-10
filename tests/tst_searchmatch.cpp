#include "tst_searchmatch.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QSet>

#include "adsoperation.h"
#include "common.h"

void TstSearchMatch::initTestCase()
{
    QVERIFY2(m_tempDir.isValid(), "无法创建临时目录");

    m_sFileA     = makeFileWithTags(QStringLiteral("only-a.txt"), QStringList() << QStringLiteral("A"));
    m_sFileAB    = makeFileWithTags(QStringLiteral("a-and-b.txt"),
                                    QStringList() << QStringLiteral("A") << QStringLiteral("B"));
    m_sFileB     = makeFileWithTags(QStringLiteral("only-b.txt"), QStringList() << QStringLiteral("B"));
    m_sFileChild = makeFileWithTags(QStringLiteral("child.txt"), QStringList() << QStringLiteral("A子"));
    m_sFileNoTag = makeFileWithTags(QStringLiteral("no-tag.txt"), QStringList());
}

void TstSearchMatch::cleanupTestCase()
{
    // QTemporaryDir 析构自动清理
}

QString TstSearchMatch::makeFileWithTags(const QString &sName, const QStringList &qLTags)
{
    const QString sPath = QDir(m_tempDir.path()).filePath(sName);
    QFile file(sPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("x");
        file.close();
    }
    if (!qLTags.isEmpty()) {
        if (!ADSOperation::replaceTags(sPath, qLTags))
            qWarning() << "写入标签失败:" << sPath << qLTags;
    }
    return sPath;
}

void TstSearchMatch::emptyGroupsMatchNothing()
{
    // 没选标签时不应该匹配任何文件（空集合不该被当成"全部满足"）
    QVERIFY(!ADSOperation::isHostFile(m_sFileAB, QList<QSet<QString>>(), labelLogic::AND));
    QVERIFY(!ADSOperation::isHostFile(m_sFileAB, QList<QSet<QString>>(), labelLogic::OR));
}

void TstSearchMatch::andRequiresEveryGroup()
{
    QSet<QString> gA; gA.insert(QStringLiteral("A"));
    QSet<QString> gB; gB.insert(QStringLiteral("B"));
    const QList<QSet<QString>> qLGroups = QList<QSet<QString>>() << gA << gB;

    QVERIFY(ADSOperation::isHostFile(m_sFileAB, qLGroups, labelLogic::AND));
    QVERIFY(!ADSOperation::isHostFile(m_sFileA, qLGroups, labelLogic::AND));
    QVERIFY(!ADSOperation::isHostFile(m_sFileB, qLGroups, labelLogic::AND));
}

void TstSearchMatch::orRequiresAnyGroup()
{
    QSet<QString> gA; gA.insert(QStringLiteral("A"));
    QSet<QString> gB; gB.insert(QStringLiteral("B"));
    const QList<QSet<QString>> qLGroups = QList<QSet<QString>>() << gA << gB;

    QVERIFY(ADSOperation::isHostFile(m_sFileA, qLGroups, labelLogic::OR));
    QVERIFY(ADSOperation::isHostFile(m_sFileAB, qLGroups, labelLogic::OR));
    QVERIFY(ADSOperation::isHostFile(m_sFileB, qLGroups, labelLogic::OR));
    QVERIFY(!ADSOperation::isHostFile(m_sFileChild, qLGroups, labelLogic::OR));
}

void TstSearchMatch::childLabelMatchesParentGroup()
{
    // 组 = 选中的父标签 + 它的全部子标签；组内是 OR，所以只打子标签的文件也要命中
    QSet<QString> gParent;
    gParent.insert(QStringLiteral("A"));
    gParent.insert(QStringLiteral("A子"));
    const QList<QSet<QString>> qLGroups = QList<QSet<QString>>() << gParent;

    QVERIFY(ADSOperation::isHostFile(m_sFileA, qLGroups, labelLogic::AND));
    QVERIFY(ADSOperation::isHostFile(m_sFileChild, qLGroups, labelLogic::AND));
    QVERIFY(ADSOperation::isHostFile(m_sFileChild, qLGroups, labelLogic::OR));
    QVERIFY(!ADSOperation::isHostFile(m_sFileB, qLGroups, labelLogic::AND));
}

void TstSearchMatch::untaggedFileNeverMatches()
{
    QSet<QString> gA; gA.insert(QStringLiteral("A"));
    const QList<QSet<QString>> qLGroups = QList<QSet<QString>>() << gA;

    QVERIFY(!ADSOperation::isHostFile(m_sFileNoTag, qLGroups, labelLogic::AND));
    QVERIFY(!ADSOperation::isHostFile(m_sFileNoTag, qLGroups, labelLogic::OR));
}
