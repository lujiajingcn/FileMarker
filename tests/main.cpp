#include <QtTest>
#include <QCoreApplication>

#include "tst_adsoperation.h"
#include "tst_xmloperation.h"
#include "tst_searchmatch.h"
#include "tst_utility.h"
#include "tst_tagexport.h"

/**
 * 一个可执行文件里跑三组测试。
 * 每组用独立的 QTest::qExec，任何一组失败都会让进程返回非 0。
 */
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    int nFailed = 0;
    {
        TstAdsOperation tc;
        nFailed += QTest::qExec(&tc, argc, argv);
    }
    {
        TstXmlOperation tc;
        nFailed += QTest::qExec(&tc, argc, argv);
    }
    {
        TstSearchMatch tc;
        nFailed += QTest::qExec(&tc, argc, argv);
    }
    {
        TstUtility tc;
        nFailed += QTest::qExec(&tc, argc, argv);
    }
    {
        TstTagExport tc;
        nFailed += QTest::qExec(&tc, argc, argv);
    }
    return nFailed;
}
