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

    // 表头提示：标签列不能排序、但双击可编辑，提示出来免得用户以为坏了
    if (orientation == Qt::Horizontal && role == Qt::ToolTipRole) {
        if (section == 4)
            return tr("双击标签列可编辑该文件的标签；标签列不支持排序");
        if (section >= 0 && section <= 3)
            return tr("点击按此列排序");
        return QVariant();
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
    if (!index.isValid())
        return QFileSystemModel::data(index, role);

    // FilePathRole 必须直接走基类：若用 index.data(...) 会虚调用回本函数造成无限递归。
    if (role == QFileSystemModel::FilePathRole)
        return QFileSystemModel::data(index, role);

    const QModelIndex nameIndex = index.sibling(index.row(), 0);
    const QString sFilePath = QFileSystemModel::data(nameIndex, QFileSystemModel::FilePathRole).toString();
    const QString sName = QFileSystemModel::data(nameIndex, Qt::DisplayRole).toString();

    // 悬停提示：完整文件名 + 全部标签（缩略图里名字和标签都可能被截断）
    if (role == Qt::ToolTipRole && index.column() == 0
        && !sFilePath.isEmpty() && !sFilePath.endsWith(":/") && !sName.isEmpty())
    {
        const QStringList tags = tagListForFile(sFilePath);
        return QString("%1\n%2").arg(sName,
                                     tags.isEmpty() ? QStringLiteral("（无标签）")
                                                    : tags.join(QStringLiteral(", ")));
    }

    if (index.column() == 4)
    {
        // 只有显示/编辑/提示角色返回标签文本。
        // 特别注意不能对 Qt::CheckStateRole 返回有效值：item delegate 会据此认为
        // 该项可勾选，在标签列画出一个没有实际作用的复选框。
        if (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::ToolTipRole)
            return QVariant();

        if (sName.isEmpty() || sFilePath.isEmpty() || sFilePath.endsWith(":/"))
            return QVariant();

        const QStringList tags = tagListForFile(sFilePath);
        if (tags.isEmpty())
            return role == Qt::DisplayRole && QFileSystemModel::isDir(nameIndex)
                       ? QVariant(QStringLiteral("—"))   // 目录没有标签，给个占位符而不是空格子
                       : QVariant();

        return QVariant(tags.join(role == Qt::ToolTipRole ? QStringLiteral(", ")
                                                          : QStringLiteral(",")));
    }

    return QFileSystemModel::data(index, role);
}

QStringList MyQFileSystemModel::tagListForFile(const QString &sFilePath) const
{
    // 用文件修改时间做缓存键：标签是通过写 NTFS 流添加的，会更新文件 mtime，
    // 因此 mtime 不变即可认为标签集合未变，避免每次重绘都枚举流。
    // 注：QFileSystemModel 在 Qt5.13 无 ModifiedRole 角色，故仍需 QFileInfo。
    const qint64 mtime = QFileInfo(sFilePath).lastModified().toMSecsSinceEpoch();
    auto it = m_tagCache.find(sFilePath);
    if (it != m_tagCache.end() && it.value().first == mtime)
        return it.value().second;

    // 按用户录入顺序取标签：流内容里存了顺序号，可还原录入顺序
    const QStringList tags = ADSOperation::listADSTagsInOrder(sFilePath);

    // 缓存容量上限：长会话下避免内存只增不减。超出后整体清空（下次访问重新枚举，P3）。
    if (m_tagCache.size() > 2000)
        m_tagCache.clear();
    m_tagCache[sFilePath] = qMakePair(mtime, tags);
    return tags;
}

bool MyQFileSystemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.column() == 4 && role == Qt::EditRole)
    {
        QString sFilePath = index.sibling(index.row(), 0).data(QFileSystemModel::FilePathRole).toString();
        m_tagCache.remove(sFilePath);

        QString sLabels = value.toString();
        // 内容没变化就不写盘：历史标签可能合法包含 "，、；"，再次 split 会把一个标签拆成多个。
        const QStringList qLExisting = ADSOperation::listADSTagsInOrder(sFilePath);
        if (sLabels == qLExisting.join(","))
            return true;

        emit sendLabels(sFilePath, sLabels);
        return true;
    }
    return QFileSystemModel::setData(index, value, role);
}

void MyQFileSystemModel::clearTagCache()
{
    m_tagCache.clear();
}

void MyQFileSystemModel::clearTagCacheForFile(const QString &sFilePath)
{
    if (!sFilePath.isEmpty())
        m_tagCache.remove(sFilePath);
}

void MyQFileSystemModel::cacheTagsForFile(const QString &sFilePath, const QStringList &tags)
{
    if (sFilePath.isEmpty())
        return;
    const qint64 mtime = QFileInfo(sFilePath).lastModified().toMSecsSinceEpoch();
    if (m_tagCache.size() > 2000)
        m_tagCache.clear();
    m_tagCache[sFilePath] = qMakePair(mtime, tags);
}

// 让第4列【标签】可编辑
Qt::ItemFlags MyQFileSystemModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QFileSystemModel::flags(index);
    if (!index.isValid())
        return f;

    // 只有"文件"的标签列才可编辑。
    // - 目录没有标签，放开编辑只会让用户输完再弹一次写盘失败；
    // - 不可用的项（权限/盘符失效）也不该给出编辑入口，否则委托记录了编辑意图、
    // 视图却不会创建编辑器，正是编辑意图残留的成因之一。
    if (index.column() == 4 && !isDir(index) && (f & Qt::ItemIsEnabled))
        f |= Qt::ItemIsEditable;

    return f;
}
