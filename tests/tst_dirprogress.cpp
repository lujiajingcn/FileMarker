#include "tst_dirprogress.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringList>

#include "dirprogresstracker.h"
#include "paralleldirtraverser.h"
#include "traversedirectory.h"

void TstDirProgress::initTestCase()
{
    m_tempDir = new QTemporaryDir;
    QVERIFY2(m_tempDir->isValid(), "无法创建临时目录");
    m_sRoot = QDir::fromNativeSeparators(m_tempDir->path());

    makeDir(QStringLiteral("a/b"));
    makeDir(QStringLiteral("keep"));
    makeDir(QStringLiteral("stopme"));
    makeDir(QStringLiteral("node_modules"));

    makeFile(QStringLiteral("a/f1.txt"));
    makeFile(QStringLiteral("a/b/f2.txt"));
    makeFile(QStringLiteral("keep/f3.txt"));
    makeFile(QStringLiteral("stopme/f4.txt"));
    makeFile(QStringLiteral("stopme/f5.txt"));
    makeFile(QStringLiteral("node_modules/f6.txt"));
}

void TstDirProgress::cleanupTestCase()
{
    delete m_tempDir;
    m_tempDir = nullptr;
}

QString TstDirProgress::makeDir(const QString &sRelPath)
{
    const QString sPath = m_sRoot + QLatin1Char('/') + sRelPath;
    QDir().mkpath(sPath);
    return QDir::fromNativeSeparators(sPath);
}

QString TstDirProgress::makeFile(const QString &sRelPath)
{
    const QString sPath = m_sRoot + QLatin1Char('/') + sRelPath;
    QDir().mkpath(QFileInfo(sPath).path());
    QFile file(sPath);
    file.open(QIODevice::WriteOnly);
    file.write("x");
    file.close();
    return QDir::fromNativeSeparators(sPath);
}

/**
 * 节点键必须统一：盘符根 "E:/" → "E:"，普通目录去掉结尾斜杠。
 * 这条规则是整个目录进度树不出现"两个根节点"、计数能汇总到根的前提。
 */
void TstDirProgress::dirNodeKeyNormalization()
{
    QCOMPARE(DirProgressTracker::dirNodeKey(QStringLiteral("E:/")), QStringLiteral("E:"));
    QCOMPARE(DirProgressTracker::dirNodeKey(QStringLiteral("E:")), QStringLiteral("E:"));
    QCOMPARE(DirProgressTracker::dirNodeKey(QStringLiteral("E:/a/b")), QStringLiteral("E:/a/b"));
    QCOMPARE(DirProgressTracker::dirNodeKey(QStringLiteral("E:/a/b/")), QStringLiteral("E:/a/b"));
    QCOMPARE(DirProgressTracker::dirNodeKey(QStringLiteral("E:\\a\\b\\")), QStringLiteral("E:/a/b"));

    // 关键回归点：QFileInfo("E:/bin").path() 返回的是带尾斜杠的 "E:/"，
    // 规范化之后才能和根节点键对上。
    QCOMPARE(DirProgressTracker::dirNodeKey(QFileInfo(QStringLiteral("E:/bin")).path()),
             QStringLiteral("E:"));
}

void TstDirProgress::defaultExcludedNames()
{
    QVERIFY(TraverseDirectory::isDefaultExcludedDirName(QStringLiteral("node_modules")));
    QVERIFY(TraverseDirectory::isDefaultExcludedDirName(QStringLiteral("NODE_MODULES")));
    QVERIFY(TraverseDirectory::isDefaultExcludedDirName(QStringLiteral(".git")));
    QVERIFY(TraverseDirectory::isDefaultExcludedDirName(QStringLiteral(".pnpm-store")));
    QVERIFY(TraverseDirectory::isDefaultExcludedDirName(QStringLiteral("$RECYCLE.BIN")));

    // 普通目录不能被误伤（以前加过 debug/release，容易被用户目录撞上，已去掉）
    QVERIFY(!TraverseDirectory::isDefaultExcludedDirName(QStringLiteral("src")));
    QVERIFY(!TraverseDirectory::isDefaultExcludedDirName(QStringLiteral("bin")));
    QVERIFY(!TraverseDirectory::isDefaultExcludedDirName(QStringLiteral("debug")));
    QVERIFY(!TraverseDirectory::isDefaultExcludedDirName(QString()));
}

void TstDirProgress::trackerLevelsAndCounts()
{
    DirProgressTracker tracker;
    tracker.reset(QStringList() << QStringLiteral("E:/"));

    // 根节点先以"等待"状态建好
    DirProgressSnapshot initial = tracker.snapshot();
    QCOMPARE(initial.size(), 1);
    QCOMPARE(initial.first().path, QStringLiteral("E:"));
    QCOMPARE(initial.first().state, DirNodeState::Waiting);

    tracker.enterDir(QStringLiteral("E:/"));
    tracker.addFile(false);
    tracker.addFile(true);
    tracker.leaveDir(QStringLiteral("E:/"), DirOutcome::Complete);

    DirProgressSnapshot snap = tracker.snapshot();
    QCOMPARE(snap.size(), 1);
    QCOMPARE(snap.first().files, 2);
    QCOMPARE(snap.first().matches, 1);
    QCOMPARE(snap.first().state, DirNodeState::Complete);

    // 子目录：层级 1，计数会同时加到父目录上
    tracker.enterDir(QStringLiteral("E:/a"));
    tracker.addFile(false);
    tracker.leaveDir(QStringLiteral("E:/a"), DirOutcome::Skipped);

    snap = tracker.snapshot();
    QCOMPARE(snap.size(), 2);
    QString sRootFiles;
    for (const DirProgressNode &node : snap) {
        if (node.path == QStringLiteral("E:")) {
            QCOMPARE(node.files, 3);            // 根目录也跟着累加
            QCOMPARE(node.level, 0);
        } else {
            QCOMPARE(node.path, QStringLiteral("E:/a"));
            QCOMPARE(node.level, 1);
            QCOMPARE(node.files, 1);
            QCOMPARE(node.state, DirNodeState::Skipped);
        }
    }
}

/** 超过 3 层的目录不建节点，只把子目录数聚合到第 3 层祖先上。 */
void TstDirProgress::trackerDeepDirAggregation()
{
    DirProgressTracker tracker;
    tracker.reset(QStringList() << QStringLiteral("E:/"));
    tracker.enterDir(QStringLiteral("E:/"));
    tracker.enterDir(QStringLiteral("E:/a"));           // level 1
    tracker.enterDir(QStringLiteral("E:/a/b"));         // level 2
    tracker.enterDir(QStringLiteral("E:/a/b/c"));       // level 3 → 建节点
    tracker.enterDir(QStringLiteral("E:/a/b/c/d"));     // level 4 → 聚合到 level 3
    tracker.leaveDir(QStringLiteral("E:/a/b/c/d"), DirOutcome::Stopped);
    tracker.leaveDir(QStringLiteral("E:/a/b/c"), DirOutcome::Incomplete);

    const DirProgressSnapshot snap = tracker.snapshot();
    bool bHasLevel4 = false;
    bool bHasLevel3 = false;
    for (const DirProgressNode &node : snap) {
        if (node.level > DirProgressTracker::kMaxDisplayDepth)
            bHasLevel4 = true;
        if (node.path == QStringLiteral("E:/a/b/c")) {
            bHasLevel3 = true;
            QCOMPARE(node.level, 3);
            QVERIFY(node.subDirs >= 1);                        // 深层目录被聚合进来
            QCOMPARE(node.state, DirNodeState::Incomplete);    // 深层被停止 → 标记不完整
        }
    }
    QVERIFY(bHasLevel3);
    QVERIFY(!bHasLevel4);
}

/** shouldSkipDir 返回 true 时整棵子树都不扫，并且结局是 Skipped。 */
void TstDirProgress::traversalSkipDir()
{
    TraverseDirectory::resetStop();
    TraverseDirectory::resetPathControls();

    QStringList qLVisitedFiles;
    QMap<QString, DirOutcome> outcomes;

    TraverseHooks hooks;
    hooks.onFile = [&qLVisitedFiles](const QFileInfo &info) -> bool {
        qLVisitedFiles << QDir::fromNativeSeparators(info.absoluteFilePath());
        return true;
    };
    hooks.onDirLeave = [&outcomes](const QString &sPath, DirOutcome outcome) {
        outcomes.insert(DirProgressTracker::dirNodeKey(sPath), outcome);
    };
    hooks.shouldSkipDir = [](const QString &sPath) -> bool {
        return TraverseDirectory::isDefaultExcludedDirName(QFileInfo(sPath).fileName());
    };

    const bool bComplete = TraverseDirectory::traverseDirectory(m_sRoot, hooks, true);
    QVERIFY(bComplete);

    // node_modules 里的文件不该被访问
    for (const QString &sFile : qLVisitedFiles)
        QVERIFY2(!sFile.contains(QStringLiteral("node_modules")), qPrintable(sFile));

    // 其它目录照常扫到
    QCOMPARE(qLVisitedFiles.size(), 5);   // a/f1, a/b/f2, keep/f3, stopme/f4, stopme/f5
    QCOMPARE(outcomes.value(DirProgressTracker::dirNodeKey(m_sRoot + QStringLiteral("/node_modules"))),
             DirOutcome::Skipped);
    QCOMPARE(outcomes.value(DirProgressTracker::dirNodeKey(m_sRoot + QStringLiteral("/keep"))),
             DirOutcome::Complete);
}

/** 只停掉一个子树：该子树不扫，兄弟目录继续扫，整体返回"不完整"。 */
void TstDirProgress::traversalStopSubtree()
{
    TraverseDirectory::resetStop();
    TraverseDirectory::resetPathControls();

    const QString sStopDir = QDir::fromNativeSeparators(m_sRoot + QStringLiteral("/stopme"));
    TraverseDirectory::requestStopPath(sStopDir);

    QStringList qLVisitedFiles;
    QMap<QString, DirOutcome> outcomes;

    TraverseHooks hooks;
    hooks.onFile = [&qLVisitedFiles](const QFileInfo &info) -> bool {
        qLVisitedFiles << QDir::fromNativeSeparators(info.absoluteFilePath());
        return true;
    };
    hooks.onDirLeave = [&outcomes](const QString &sPath, DirOutcome outcome) {
        outcomes.insert(DirProgressTracker::dirNodeKey(sPath), outcome);
    };

    const bool bComplete = TraverseDirectory::traverseDirectory(m_sRoot, hooks, true);
    QVERIFY(!bComplete);   // 有子树被停止 → 整体不完整

    for (const QString &sFile : qLVisitedFiles)
        QVERIFY2(!sFile.contains(QStringLiteral("stopme")), qPrintable(sFile));

    // 兄弟目录照常扫完
    QVERIFY(qLVisitedFiles.contains(QDir::fromNativeSeparators(m_sRoot + QStringLiteral("/keep/f3.txt"))));
    QCOMPARE(outcomes.value(DirProgressTracker::dirNodeKey(sStopDir)), DirOutcome::Stopped);
    QCOMPARE(outcomes.value(DirProgressTracker::dirNodeKey(m_sRoot + QStringLiteral("/keep"))),
             DirOutcome::Complete);

    TraverseDirectory::resetPathControls();
}

/** 全局 stop() 后遍历返回 false，且不再继续往下扫。 */
void TstDirProgress::traversalReturnsFalseWhenStopped()
{
    TraverseDirectory::resetStop();
    TraverseDirectory::resetPathControls();

    int nFileCalls = 0;
    TraverseHooks hooks;
    hooks.onFile = [&nFileCalls](const QFileInfo &) -> bool {
        ++nFileCalls;
        TraverseDirectory::stop();   // 第一个文件就要求停止
        return true;
    };

    const bool bComplete = TraverseDirectory::traverseDirectory(m_sRoot, hooks, true);
    QVERIFY(!bComplete);
    QCOMPARE(nFileCalls, 1);   // 停下之后不再继续扫

    TraverseDirectory::resetStop();
    TraverseDirectory::resetPathControls();
}

namespace {

/** 用给定的线程数跑一遍遍历，收集访问到的文件（排序后便于比较） */
QStringList runTraversal(const QString &sRoot, int nThreads,
                         const std::function<bool(const QString &)> &shouldSkip)
{
    QStringList qLFiles;
    QMutex mutex;

    ParallelTraverseCallbacks cb;
    cb.onFile = [&](const QFileInfo &info) -> bool {
        const QString sPath = QDir::fromNativeSeparators(info.absoluteFilePath());
        QMutexLocker locker(&mutex);
        qLFiles << sPath;
        return true;
    };
    cb.shouldSkipDir = shouldSkip;

    ParallelDirTraverser::run(QStringList() << sRoot, nThreads, cb);
    qLFiles.sort();
    return qLFiles;
}

}   // namespace

/** 并行和串行扫出来的文件集合必须完全一致（顺序不保证，所以排序后比较）。 */
void TstDirProgress::parallelTraversalVisitsSameFiles()
{
    TraverseDirectory::resetStop();
    TraverseDirectory::resetPathControls();

    const QStringList qLSerial = runTraversal(m_sRoot, 1, nullptr);
    const QStringList qLParallel = runTraversal(m_sRoot, 4, nullptr);

    QCOMPARE(qLSerial.size(), 6);           // a/f1, a/b/f2, keep/f3, stopme/f4, stopme/f5, node_modules/f6
    QCOMPARE(qLParallel, qLSerial);
}

void TstDirProgress::parallelTraversalHonoursSkip()
{
    TraverseDirectory::resetStop();
    TraverseDirectory::resetPathControls();

    const QStringList qLFiles = runTraversal(m_sRoot, 4, [](const QString &sPath) -> bool {
        return TraverseDirectory::isDefaultExcludedDirName(QFileInfo(sPath).fileName());
    });

    for (const QString &sFile : qLFiles)
        QVERIFY2(!sFile.contains(QStringLiteral("node_modules")), qPrintable(sFile));
    QCOMPARE(qLFiles.size(), 5);
}

void TstDirProgress::parallelTraversalHonoursStopSubtree()
{
    TraverseDirectory::resetStop();
    TraverseDirectory::resetPathControls();

    const QString sStopDir = QDir::fromNativeSeparators(m_sRoot + QStringLiteral("/stopme"));
    TraverseDirectory::requestStopPath(sStopDir);

    const QStringList qLFiles = runTraversal(m_sRoot, 4, nullptr);

    for (const QString &sFile : qLFiles)
        QVERIFY2(!sFile.contains(QStringLiteral("stopme")), qPrintable(sFile));
    QVERIFY(qLFiles.contains(QDir::fromNativeSeparators(m_sRoot + QStringLiteral("/keep/f3.txt"))));

    TraverseDirectory::resetPathControls();
}
