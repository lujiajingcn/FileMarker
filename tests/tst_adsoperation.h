#ifndef TST_ADSOPERATION_H
#define TST_ADSOPERATION_H

#include <QObject>
#include <QString>
#include <QTemporaryDir>

/** NTFS 标签流读写的单元测试 */
class TstAdsOperation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void writeAndReadTagsKeepOrder();
    void appendTagsDedup();
    void renameTagKeepsOrder();
    void removeTagKeepsOthers();
    void removingLastTagRemovesManifest();
    void thirdPartyStreamIsPreserved();
    void deleteAllOnlyRemovesOurTags();
    void emptyFileHasNoTags();
    void replaceTagsWithEmptyListClearsAll();

private:
    /** 在临时目录里建一个空文件，返回绝对路径 */
    QString makeTempFile(const QString &sName);

    QTemporaryDir m_tempDir;
};

#endif // TST_ADSOPERATION_H
