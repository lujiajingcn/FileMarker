/**
** 【可编辑代理】
**
** 缩略图模式下绘制文件：图标 + 文件名（最多两行）+ 标签（次要颜色，最多两行）。
** 标签为空时不占行，整个内容块在格子里垂直居中；鼠标悬停时给淡色圆角底。
** 双击文件名区域编辑文件名，双击标签区域就地编辑标签（写回第 5 列）。
**/
#ifndef EDITABLEDELEGATE_H
#define EDITABLEDELEGATE_H

#include <QStyledItemDelegate>
#include <QPersistentModelIndex>
#include <QPainter>
#include <QMouseEvent>
#include <QLineEdit>
#include <QFileSystemModel>
#include <QMap>
#include "common.h"

class ThumbnailIconProvider;

// 标记当前处于什么编辑模式
enum EditTarget { TargetNone, TargetFileName, TargetCustomData };

// 统一管理布局，保证 paint 和 editorEvent 使用相同的坐标逻辑
struct LayoutRects {
    QRect iconRect;
    QRect nameRect;
    QRect customRect;    // 标签文字区域（没有标签时为空）
    QRect tagEditRect;   // 标签编辑区域：没有标签时是文件名下方的一行，保证仍可双击编辑
};

class EditableDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:

    explicit EditableDelegate(QObject *parent = nullptr);

    /** 设置图标边长（逻辑像素），格子大小由调用方同步设置 QListView */
    void setIconSize(int iconSize);
    int iconSize() const { return m_iconSize; }

    /** 按当前字体计算缩略图格子所需高度，避免大字体/文本缩放时内容溢出 */
    static int preferredCellHeight(int iconSize, const QFont &font);

    /** 设置当前鼠标悬停的项，为空表示没有悬停项 */
    void setHoveredIndex(const QModelIndex &index);

    /**
     ** 设置缩略图生成器。图标在 paint() 里（GUI 线程）按需获取，
     ** 不走 QFileSystemModel 的图标提供器（那是在后台线程里调用的，
     ** 而 QPixmap/Shell COM 只能在 GUI 线程安全使用）。
     **/
    void setThumbnailProvider(ThumbnailIconProvider *provider);

    LayoutRects calculateRects(const QStyleOptionViewItem &option, const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // 捕获鼠标点击，决定编辑目标
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

    // 创建编辑器
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    // 从模型加载数据到编辑器
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    // 保存数据回模型
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

    // 调整编辑器位置 (让输入框正好覆盖在文字上)
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

signals:
    void sendDblClick(const QModelIndex &index);
    // 保留给需要弹框编辑标签的场景（右键菜单“编辑标签”）
    void sendEditTag(const QModelIndex &index);

private:
    // 双击语义已经简化（双击=打开、双击标签区=弹标签对话框、重命名走右键菜单），
    // 视图也设为 NoEditTriggers，因此不再需要"编辑意图"这类跨调用的可变状态。
    int m_iconSize;
    QPersistentModelIndex m_hoveredIndex;
    ThumbnailIconProvider *m_thumbnailProvider;
};

#endif // EDITABLEDELEGATE_H
