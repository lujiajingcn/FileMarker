#include "myqfilesystemmodel.h"
#include <QDebug>
#include "adsoperation.h"
#include <QStack>

MyQFileSystemModel::MyQFileSystemModel()
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
            QStringList qLAdsFileNames = ADSOperation::listADSFileName(sFilePath);
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
    return QFileSystemModel::setData(index, value, role);
}
