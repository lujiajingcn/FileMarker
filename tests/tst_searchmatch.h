#ifndef TST_SEARCHMATCH_H
#define TST_SEARCHMATCH_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

/** 搜索匹配的单元测试：组内 OR、组间 AND/OR */
class TstSearchMatch : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void emptyGroupsMatchNothing();
    void andRequiresEveryGroup();
    void orRequiresAnyGroup();
    void childLabelMatchesParentGroup();
    void untaggedFileNeverMatches();

    // 下面几条直接用共享匹配函数，等价于"缓存查询"走的那条路径
    void sharedMatcherAndOr();
    void sharedMatcherEmptyInputs();

private:
    /** 建一个带指定标签的文件，返回路径 */
    QString makeFileWithTags(const QString &sName, const QStringList &qLTags);

    QTemporaryDir m_tempDir;
    QString m_sFileA;        // 只有 A
    QString m_sFileAB;       // A + B
    QString m_sFileB;        // 只有 B
    QString m_sFileChild;    // 只有 A 的子标签
    QString m_sFileNoTag;    // 没有标签
};

#endif // TST_SEARCHMATCH_H
