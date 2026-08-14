/**
** 【可编辑代理】
**
** 使用自定义代理，实现当缩略图显示文件时，将文件的标签显示在该文件下方，并且可以手动编辑标签
**/
#ifndef EDITABLEDELEGATE_H
#define EDITABLEDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QMouseEvent>
#include <QLineEdit>
#include <QFileSystemModel>
#include "common.h"

// 标记当前处于什么编辑模式
enum EditTarget { TargetNone, TargetFileName, TargetCustomData };

// 统一管理布局，保证 paint 和 editorEvent 使用相同的坐标逻辑
struct LayoutRects {
    QRect iconRect;
    QRect nameRect;
    QRect customRect;
};

class EditableDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:

    explicit EditableDelegate(QObject *parent = nullptr);

    LayoutRects calculateRects(const QStyleOptionViewItem &option) const;

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
    // 缩略图模式下双击标签区域时发出，由 FormPageFiles 弹出输入框编辑该文件标签（P2-2）。
    void sendEditTag(const QModelIndex &index);

private:
    // mutable 允许在 const 函数中修改 (虽然 editorEvent 不是 const，但 createEditor 是)
    // 实际使用中，editorEvent 会先于 createEditor 调用
    mutable EditTarget m_editTarget;
};

#endif // EDITABLEDELEGATE_H
