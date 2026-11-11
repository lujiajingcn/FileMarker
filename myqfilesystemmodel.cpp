#include "myqfilesystemmodel.h"
#include <QDebug>
#include "adsoperation.h"
#include <QStack>
#include <QDateTime>
#include <functional>

#include <QFileInfo>
#include <QMetaObject>
#include <QRunnable>
#include <QThreadPool>


namespace {

/** 把一段后台任务包成 QRunnable（Qt5.13 还没有 QRunnable::create）。 */
class LambdaRunnable : public QRunnable
{
public:
    explicit LambdaRunnable(std::function<void()> fn) : m_fn(std::move(fn)) {}
    void run() override
    {
        if (m_fn)
            m_fn();
    }

private:
    std::function<void()> m_fn;
};

}   // namespace

MyQFileSystemModel::MyQFileSystemModel(QObject *parent) :
    QFileSystemModel(parent)
{
    // 标签缓存最多留 4000 个文件（每条 1 个单位），超出按 LRU 淘汰最久没用的，
    // 不再像以前那样整体清空（整体清空会让大目录里每行都重新读盘）。
    m_tagCache.setMaxCost(4000);
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
            // 名称列表头不画排序箭头：返回一张透明的 16x1 位图占位，
            // 否则 QFileSystemModel 会给第 0 列画上系统默认的排序指示器。
            // 已知副作用：表头上的箭头文字位置会偏 2 像素，属于外观瑕疵，可接受。
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
        const QStringList tags = cachedTagsOnly(sFilePath);
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

        const QStringList tags = cachedTagsOnly(sFilePath);
        if (tags.isEmpty())
            return role == Qt::DisplayRole && QFileSystemModel::isDir(nameIndex)
                       ? QVariant(QStringLiteral("—"))   // 目录没有标签，给个占位符而不是空格子
                       : QVariant();

        return QVariant(tags.join(role == Qt::ToolTipRole ? QStringLiteral(", ")
                                                          : QStringLiteral(",")));
    }

    return QFileSystemModel::data(index, role);
}

/**
 * 绘制时取标签：**只查缓存，绝不碰磁盘**。
 *
 * 以前这里会同步 QFileInfo + 枚举 NTFS 流，大目录里滚动/重绘时每行都要读盘，
 * 界面直接卡死。现在缓存未命中就先返回空，同时把这次读取排进线程池，
 * 读完后由 onTagLoaded() 写回缓存并通知视图刷新那一格。
 */
QStringList MyQFileSystemModel::cachedTagsOnly(const QString &sFilePath) const
{
    if (const TagCacheEntry *cached = m_tagCache.object(sFilePath))
        return cached->tags;   // 命中（命中路径不 stat，省一次系统调用）

    scheduleTagLoad(sFilePath);
    return QStringList();
}

/** 把一次标签读取排进线程池；同一个文件在队列里只保留一个请求。 */
void MyQFileSystemModel::scheduleTagLoad(const QString &sFilePath) const
{
    if (sFilePath.isEmpty() || m_pendingTagLoads.contains(sFilePath))
        return;
    m_pendingTagLoads.insert(sFilePath);

    if (m_tagPool == nullptr) {
        m_tagPool = new QThreadPool;
        m_tagPool->setMaxThreadCount(2);   // 读 ADS 是 IO 型，2 个线程足够又不抢占 UI
    }

    // 用本模型对象做回调上下文：模型销毁后排队中的回调会被丢弃，不会访问野指针
    QObject *context = const_cast<MyQFileSystemModel *>(this);
    m_tagPool->start(new LambdaRunnable([this, context, sFilePath]() {
        // 线程里只做纯 Win32/IO 操作，不碰任何 GUI 对象
        const QStringList tags = ADSOperation::listADSTagsInOrder(sFilePath);
        const qint64 mtime = QFileInfo(sFilePath).lastModified().toMSecsSinceEpoch();
        QMetaObject::invokeMethod(context, [this, sFilePath, tags, mtime]() {
            auto *entry = new TagCacheEntry;
            entry->mtime = mtime;
            entry->tags = tags;
            m_tagCache.insert(sFilePath, entry, 1);
            m_pendingTagLoads.remove(sFilePath);
            const_cast<MyQFileSystemModel *>(this)->onTagLoaded(sFilePath, tags);
        }, Qt::QueuedConnection);
    }));
}

/** 后台读完：通知视图刷新这个文件的标签列。 */
void MyQFileSystemModel::onTagLoaded(const QString &sFilePath, const QStringList &)
{
    const QModelIndex base = index(sFilePath);
    if (!base.isValid())
        return;
    const QModelIndex tagIndex = index(base.row(), 4, base.parent());
    if (tagIndex.isValid())
        emit dataChanged(tagIndex, tagIndex, QVector<int>() << Qt::DisplayRole << Qt::ToolTipRole);
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
    auto *entry = new TagCacheEntry;
    entry->mtime = mtime;
    entry->tags = tags;
    m_tagCache.insert(sFilePath, entry, 1);
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
