#ifndef TST_UTILITY_H
#define TST_UTILITY_H

#include <QObject>

/** Utility 里几个纯函数的单元测试（标签拆分、标签名合法性判断等） */
class TstUtility : public QObject
{
    Q_OBJECT

private slots:
    void splitLabelsBasic();
    void splitLabelsFullWidthSeparators();
    void splitLabelsTrimsAndDedups();
    void splitLabelsEmptyOrOnlySeparators();
    void splitLabelsPreservingExisting();
    void isADSNameValidBoundaries();
    void adsNameWithoutType();
};

#endif // TST_UTILITY_H
