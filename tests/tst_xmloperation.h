#ifndef TST_XMLOPERATION_H
#define TST_XMLOPERATION_H

#include <QObject>
#include <QString>
#include <QTemporaryDir>

#include "xmloperation.h"

/** 标签库 XML 读写的单元测试 */
class TstXmlOperation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void roundTripKeepsTree();
    void legacyWrapperFormatIsFlattened();
    void missingFileReturnsEmpty();
    void corruptFileIsBackedUpAndReturnsEmpty();

private:
    /** 递归比较两棵树：名称、UUID、子标签数量与顺序 */
    static void compareTree(const QList<LabelInfo> &qLExpected, const QList<LabelInfo> &qLActual);

    QTemporaryDir m_tempDir;
};

#endif // TST_XMLOPERATION_H
