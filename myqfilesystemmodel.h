#ifndef MYQFILESYSTEMMODEL_H
#define MYQFILESYSTEMMODEL_H

#include <QFileSystemModel>

class MyQFileSystemModel :public QFileSystemModel
{
    Q_OBJECT

public:
    explicit MyQFileSystemModel(QObject *parent = nullptr);

signals:
    void sendLabels(QString sLabels);

public:
    int columnCount( const QModelIndex & index ) const override;

    /** 将表头的字段名由默认的Name、Size、Type、Date Modified，改为名称、大小、类型、修改日期，并添加字段名ADS文件名 */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QVariant data( const QModelIndex & index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
};

#endif // MYQFILESYSTEMMODEL_H
