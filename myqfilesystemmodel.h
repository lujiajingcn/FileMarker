#ifndef MYQFILESYSTEMMODEL_H
#define MYQFILESYSTEMMODEL_H

#include <QFileSystemModel>
#include <QHash>
#include <QPair>
#include <QFileInfo>

class MyQFileSystemModel :public QFileSystemModel
{
    Q_OBJECT

public:
    explicit MyQFileSystemModel(QObject *parent = nullptr);

signals:
    void sendLabels(QString sFilePath, QString sLabels);

public:
    int columnCount( const QModelIndex & index ) const override;

    /** 将表头的字段名由默认的Name、Size、Type、Date Modified，改为名称、大小、类型、修改日期，并添加字段名ADS文件名 */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QVariant data( const QModelIndex & index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // 清空标签缓存（如刷新目录时调用），避免 ADS 被增删后界面仍显示旧标签（P2-3/P3）。
    void clearTagCache();

    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    // 缓存“文件路径 -> (文件修改时间, 标签流名列表)”，避免标签列每次重绘都枚举 NTFS 流。
    mutable QHash<QString, QPair<qint64, QStringList>> m_tagCache;
};

#endif // MYQFILESYSTEMMODEL_H
