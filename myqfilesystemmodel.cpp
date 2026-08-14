#include "myqfilesystemmodel.h"
#include <QDebug>
#include "adsoperation.h"
#include <QStack>
#include <QDateTime>
#include <QFileInfo>

MyQFileSystemModel::MyQFileSystemModel(QObject *parent) :
    QFileSystemModel(parent)
{
}

int MyQFileSystemModel::columnCount( const QModelIndex & index ) const
{
    return QFileSystemModel::columnCount( index ) + 1;
}

QVariant MyQFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    switch (role) {
    case Qt::DecorationRole:
        if (section == 0) {
            // ### TODO oh man this is ugly and doesn't even work all the way!
            // it is still 2 pixels off
            QImage pixmap(16, 1, QImage::Format_Mono);
            pixmap.fill(0);
            pixmap.setAlphaChannel(pixmap.createAlphaMask());
            return pixmap;
        }
        break;
    case Qt::TextAlignmentRole:
        return Qt::AlignLeft;
    }

    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractItemModel::headerData(section, orientation, role);

    QString returnValue;
    switch (section) {
    case 0: returnValue = tr("名称");
            break;
    case 1: returnValue = tr("大小");
            break;
    case 2: returnValue =tr("类型");
           break;
    case 3: returnValue = tr("修改日期");
            break;
    case 4: returnValue = tr("标签");
            break;
    default: return QVariant();
    }
    return returnValue;
}

QVariant MyQFileSystemModel::data(const QModelIndex & index, int role) const
{
    if (index.column() == 4)
    {
        QModelIndex indexPath = index.sibling(index.row(), 0);
        QString sFilePath = indexPath.data(QFileSystemModel::FilePathRole).toString();
        if(QFileSystemModel::data( indexPath, role ).toString().isEmpty() || sFilePath.endsWith(":/"))
        {
            return QVariant();
        }
        if(sFilePath.isEmpty())
        {
            return QVariant();
        }
        else
        {
            // 用文件修改时间做缓存键：标签是通过写 NTFS 流添加的，会更新文件 mtime，
            // 因此 mtime 不变即可认为标签集合未变，避免每次重绘都枚举流。
            // 注：QFileSystemModel 在 Qt5.13 无 ModifiedRole 角色，故仍需 QFileInfo。
            qint64 mtime = QFileInfo(sFilePath).lastModified().toMSecsSinceEpoch();
            auto it = m_tagCache.find(sFilePath);
            if (it != m_tagCache.end() && it.value().first == mtime)
                return QVariant(it.value().second.join(","));

            QStringList qLAdsFileNames = ADSOperation::listADSFileName(sFilePath);
            m_tagCache[sFilePath] = qMakePair(mtime, qLAdsFileNames);
            // 缓存容量上限：长会话下避免内存只增不减。超出后整体清空（下次访问重新枚举，P3）。
            if (m_tagCache.size() > 2000)
                m_tagCache.clear();
            QString sAdsFileNames = qLAdsFileNames.join(",");
            if(sAdsFileNames.isEmpty())
            {
                return QVariant();
            }
            else
            {
                return QVariant(sAdsFileNames);
            }
        }
    }
    else
    {
        return QFileSystemModel::data( index, role );
    }
}

bool MyQFileSystemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.column() == 4 && role == Qt::EditRole)
    {
        // 标签被编辑后，失效该文件的缓存，下次绘制重新枚举。
        QString sFilePath = index.sibling(index.row(), 0).data(QFileSystemModel::FilePathRole).toString();
        m_tagCache.remove(sFilePath);

        QString sLabels = value.toString();
        emit sendLabels(sFilePath, sLabels);
        return true;
    }
    return QFileSystemModel::setData(index, value, role);
}

void MyQFileSystemModel::clearTagCache()
{
    m_tagCache.clear();
}

// 让第4列【标签】可编辑
Qt::ItemFlags MyQFileSystemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QFileSystemModel::flags(index);
    if (!index.isValid())
        return f;

    if (index.column() == 4)
        f |= Qt::ItemIsEditable;

    return f;
}
