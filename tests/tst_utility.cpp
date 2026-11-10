#include "tst_utility.h"

#include <QtTest>

#include "utility.h"

void TstUtility::splitLabelsBasic()
{
    const QStringList qL = Utility::splitLabels(QStringLiteral("电子书,户外,pdf"));
    QCOMPARE(qL, QStringList() << QStringLiteral("电子书") << QStringLiteral("户外")
                               << QStringLiteral("pdf"));
}

void TstUtility::splitLabelsFullWidthSeparators()
{
    // 中文输入法下经常打出全角逗号/顿号/分号
    const QStringList qL = Utility::splitLabels(QStringLiteral("甲，乙、丙；丁;戊,己"));
    QCOMPARE(qL, QStringList() << QStringLiteral("甲") << QStringLiteral("乙")
                               << QStringLiteral("丙") << QStringLiteral("丁")
                               << QStringLiteral("戊") << QStringLiteral("己"));
}

void TstUtility::splitLabelsTrimsAndDedups()
{
    // 前后空格要清掉，重复项只留一个（Windows 流名不区分大小写，但这里大小写不同算两个）
    const QStringList qL = Utility::splitLabels(QStringLiteral("  甲 , 乙,,甲 ,  "));
    QCOMPARE(qL, QStringList() << QStringLiteral("甲") << QStringLiteral("乙"));
}

void TstUtility::splitLabelsEmptyOrOnlySeparators()
{
    QVERIFY(Utility::splitLabels(QString()).isEmpty());
    QVERIFY(Utility::splitLabels(QStringLiteral("   ")).isEmpty());
    QVERIFY(Utility::splitLabels(QStringLiteral(",,,、；")).isEmpty());
}

void TstUtility::splitLabelsPreservingExisting()
{
    // 已有标签里合法带逗号的（历史数据），不能被再拆开
    const QStringList qLExisting = QStringList() << QStringLiteral("甲,乙");
    const QStringList qL = Utility::splitLabelsPreservingExisting(
        QStringLiteral("甲,乙,丙"), qLExisting);
    QCOMPARE(qL, QStringList() << QStringLiteral("甲,乙") << QStringLiteral("丙"));

    // 没有历史标签时就是普通拆分
    const QStringList qLPlain = Utility::splitLabelsPreservingExisting(
        QStringLiteral("甲,乙"), QStringList());
    QCOMPARE(qLPlain, QStringList() << QStringLiteral("甲") << QStringLiteral("乙"));
}

// 注意：isADSNameValid 的语义是"这个流是不是本程序的标签流"，
// 不是"这个名字是不是合法的流名"（非法字符由 sanitizeTagName 负责）。
void TstUtility::isADSNameValidBoundaries()
{
    // 普通标签名（含中文、空格、带 :$DATA 后缀的枚举结果）都算标签流
    QVERIFY(Utility::isADSNameValid(QStringLiteral("电子书")));
    QVERIFY(Utility::isADSNameValid(QStringLiteral("a b")));
    QVERIFY(Utility::isADSNameValid(QStringLiteral("电子书:$DATA")));

    // 空名字不算
    QVERIFY(!Utility::isADSNameValid(QString()));

    // 第三方流与归属清单本身不是标签
    QVERIFY(!Utility::isADSNameValid(QStringLiteral("Zone.Identifier")));
    QVERIFY(!Utility::isADSNameValid(QStringLiteral("zone.identifier:$DATA")));
    QVERIFY(!Utility::isADSNameValid(QStringLiteral("FileMarker.Owned")));
    QVERIFY(!Utility::isADSNameValid(QStringLiteral("filemarker.owned:$DATA")));
}

void TstUtility::adsNameWithoutType()
{
    // FindFirstStreamW 返回的是 "tag:$DATA" 这种带类型后缀的名字
    QCOMPARE(Utility::adsNameWithoutType(QStringLiteral("电子书:$DATA")),
             QStringLiteral("电子书"));
    QCOMPARE(Utility::adsNameWithoutType(QStringLiteral("普通名字")),
             QStringLiteral("普通名字"));
    // 结尾不是 :$DATA 的不动它
    QCOMPARE(Utility::adsNameWithoutType(QStringLiteral("a:$OTHER")),
             QStringLiteral("a:$OTHER"));
}
