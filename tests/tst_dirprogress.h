#ifndef TST_DIRPROGRESS_H
#define TST_DIRPROGRESS_H

#include <QObject>
#include <QString>
#include <QTemporaryDir>

/**
 * 目录进度树相关逻辑的单元测试：
 *   - 节点键规范化（dirNodeKey）
 *   - 默认排除目录名单
 *   - 层级/计数/聚合（DirProgressTracker）
 *   - 带钩子的遍历（跳过、停止子树、返回码）
 */
class TstDirProgress : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void dirNodeKeyNormalization();
    void defaultExcludedNames();
    void trackerLevelsAndCounts();
    void trackerDeepDirAggregation();
    void traversalSkipDir();
    void traversalStopSubtree();
    void traversalReturnsFalseWhenStopped();

private:
    QString makeDir(const QString &sRelPath);
    QString makeFile(const QString &sRelPath);
    QString m_sRoot;                 // 临时目录根
    QTemporaryDir *m_tempDir = nullptr;
};

#endif // TST_DIRPROGRESS_H
