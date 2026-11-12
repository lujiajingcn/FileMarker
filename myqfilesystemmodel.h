#ifndef MYQFILESYSTEMMODEL_H
#define MYQFILESYSTEMMODEL_H

#include <QFileSystemModel>
#include <QCache>
#include <QHash>
#include <QPair>
#include <QFileInfo>
#include <QSet>
#include <QThreadPool>

class MyQFileSystemModel :public QFileSystemModel
{
    Q_OBJECT

public:
    explicit MyQFileSystemModel(QObject *parent = nullptr);
    ~MyQFileSystemModel() override;

signals:
    void sendLabels(const QString &sFilePath, const QString &sLabels);

public:
    int columnCount( const QModelIndex & index ) const override;

    /** 将表头的字段名由默认的Name、Size、Type、Date Modified，改为名称、大小、类型、修改日期，并添加字段名ADS文件名 */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QVariant data( const QModelIndex & index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // 清空标签缓存（如刷新目录时调用），避免 ADS 被增删后界面仍显示旧标签。
    void clearTagCache();
    /** 只让某个文件的标签缓存失效（改过标签后用，避免整目录重读 ADS） */
    void clearTagCacheForFile(const QString &sFilePath);

    // 把目录标签扫描已经读到的结果预热进模型缓存，绘制时不必再同步枚举 ADS
    void cacheTagsForFile(const QString &sFilePath, const QStringList &tags);

    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    /** 只查缓存；未命中就排队异步读，立即返回空串（绘制时绝不碰磁盘） */
    QStringList cachedTagsOnly(const QString &sFilePath) const;
    /** 把一次标签读取排进线程池（同一文件不重复排） */
    void scheduleTagLoad(const QString &sFilePath) const;
    /** 后台读完回到 UI 线程：写缓存 + 通知视图刷新那一格 */
    void onTagLoaded(const QString &sFilePath, const QStringList &tags);

    // 缓存“文件路径 -> (文件修改时间, 标签列表)”，避免标签列每次重绘都枚举 NTFS 流。
    // 标签缓存：键 = 文件路径。用 QCache 做 LRU 淘汰，
    // 原来"超过 2000 条就整体清空"会在大目录里反复失效，等于缓存从没生效过。
    struct TagCacheEntry
    {
        qint64 mtime = 0;
        QStringList tags;
    };
    mutable QCache<QString, TagCacheEntry> m_tagCache;
    mutable QSet<QString> m_pendingTagLoads;    // 已排队、还没读完的路径
    mutable QThreadPool *m_tagPool = nullptr;   // 懒创建，最多 2 个后台线程
};

#endif // MYQFILESYSTEMMODEL_H
