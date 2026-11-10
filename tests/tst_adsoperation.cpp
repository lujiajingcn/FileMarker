#include "tst_adsoperation.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "adsoperation.h"
#include "common.h"

void TstAdsOperation::initTestCase()
{
    QVERIFY2(m_tempDir.isValid(), "无法创建临时目录");
}

void TstAdsOperation::cleanupTestCase()
{
    // QTemporaryDir 析构时自动删除
}

QString TstAdsOperation::makeTempFile(const QString &sName)
{
    const QString sPath = QDir(m_tempDir.path()).filePath(sName);
    QFile file(sPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("test");
        file.close();
    }
    return sPath;
}

void TstAdsOperation::writeAndReadTagsKeepOrder()
{
    const QString sFile = makeTempFile("order.txt");
    const QStringList qLWrite = QStringList() << QStringLiteral("乙") << QStringLiteral("甲")
                                              << QStringLiteral("丙");
    QVERIFY(ADSOperation::replaceTags(sFile, qLWrite));

    // 写入顺序要能读回来（NTFS 枚举流是按流名排序的，顺序号存在流内容里）
    QCOMPARE(ADSOperation::listADSTagsInOrder(sFile), qLWrite);
}

void TstAdsOperation::appendTagsDedup()
{
    const QString sFile = makeTempFile("append.txt");
    QVERIFY(ADSOperation::replaceTags(sFile, QStringList() << QStringLiteral("一")));
    QVERIFY(ADSOperation::appendTags(sFile, QStringList()
                                               << QStringLiteral("二") << QStringLiteral("一")));

    const QStringList qLExpect = QStringList() << QStringLiteral("一") << QStringLiteral("二");
    QCOMPARE(ADSOperation::listADSTagsInOrder(sFile), qLExpect);
}

void TstAdsOperation::renameTagKeepsOrder()
{
    const QString sFile = makeTempFile("rename.txt");
    QVERIFY(ADSOperation::replaceTags(sFile, QStringList()
                                               << QStringLiteral("旧名") << QStringLiteral("另一个")));
    QVERIFY(ADSOperation::renameTag(sFile, QStringLiteral("旧名"), QStringLiteral("新名")));

    const QStringList qLExpect = QStringList() << QStringLiteral("新名") << QStringLiteral("另一个");
    QCOMPARE(ADSOperation::listADSTagsInOrder(sFile), qLExpect);

    // 文件上没有这个标签时视为成功（标签库改名后同步到文件时可以放心调用）
    QVERIFY(ADSOperation::renameTag(sFile, QStringLiteral("不存在"), QStringLiteral("随便")));
}

void TstAdsOperation::removeTagKeepsOthers()
{
    const QString sFile = makeTempFile("remove.txt");
    QVERIFY(ADSOperation::replaceTags(sFile, QStringList()
                                               << QStringLiteral("保留A") << QStringLiteral("删掉")
                                               << QStringLiteral("保留B")));
    QVERIFY(ADSOperation::removeTag(sFile, QStringLiteral("删掉")));

    const QStringList qLExpect = QStringList() << QStringLiteral("保留A") << QStringLiteral("保留B");
    QCOMPARE(ADSOperation::listADSTagsInOrder(sFile), qLExpect);
}

void TstAdsOperation::removingLastTagRemovesManifest()
{
    const QString sFile = makeTempFile("lasttag.txt");
    QVERIFY(ADSOperation::replaceTags(sFile, QStringList() << QStringLiteral("唯一")));
    QVERIFY(ADSOperation::removeTag(sFile, QStringLiteral("唯一")));

    QVERIFY(ADSOperation::listADSTagsInOrder(sFile).isEmpty());
    // 没有标签时归属清单也没有存在的意义，应一并删除
    QVERIFY(!QFile::exists(sFile + QStringLiteral(":") + QString::fromLatin1(FM_TAG_MARKER)));
}

void TstAdsOperation::thirdPartyStreamIsPreserved()
{
    const QString sFile = makeTempFile("thirdparty.txt");
    // 模拟 Windows 自己写的 Zone.Identifier 流
    QFile zone(sFile + QStringLiteral(":") + QString::fromLatin1(FILTER_TAG));
    QVERIFY(zone.open(QIODevice::WriteOnly));
    zone.write("[ZoneTransfer]\r\nZoneId=3\r\n");
    zone.close();

    QVERIFY(ADSOperation::replaceTags(sFile, QStringList() << QStringLiteral("标签一")));
    QVERIFY(ADSOperation::deleteADSFiles(sFile));

    QVERIFY(QFile::exists(sFile + QStringLiteral(":") + QString::fromLatin1(FILTER_TAG)));
    QVERIFY(ADSOperation::listADSTagsInOrder(sFile).isEmpty());
}

void TstAdsOperation::deleteAllOnlyRemovesOurTags()
{
    const QString sFile = makeTempFile("deleteall.txt");
    QVERIFY(ADSOperation::replaceTags(sFile, QStringList()
                                               << QStringLiteral("A") << QStringLiteral("B")));
    QVERIFY(ADSOperation::deleteADSFiles(sFile));
    QVERIFY(ADSOperation::listADSTagsInOrder(sFile).isEmpty());
}

void TstAdsOperation::emptyFileHasNoTags()
{
    const QString sFile = makeTempFile("empty.txt");
    QVERIFY(ADSOperation::listADSTagsInOrder(sFile).isEmpty());
}

// 撤销"打标签"就是拿空列表覆盖回去，必须真的把标签和清单都删掉
void TstAdsOperation::replaceTagsWithEmptyListClearsAll()
{
    const QString sFile = makeTempFile("clearviaempty.txt");
    QVERIFY(ADSOperation::replaceTags(sFile, QStringList()
                                               << QStringLiteral("X") << QStringLiteral("Y")));
    QCOMPARE(ADSOperation::listADSTagsInOrder(sFile).size(), 2);

    QVERIFY(ADSOperation::replaceTags(sFile, QStringList()));
    QVERIFY(ADSOperation::listADSTagsInOrder(sFile).isEmpty());
    QVERIFY(!QFile::exists(sFile + QStringLiteral(":") + QString::fromLatin1(FM_TAG_MARKER)));
}

