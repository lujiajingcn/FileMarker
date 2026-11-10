/**
** 【标签列代理】把一行的标签串画成一个个带 ✕ 的小标签
**
** 详情视图第 5 列原来只是一行逗号分隔的文本，标签多了分不清边界，
** 也不方便单独删一个。这里改成"chip（药丸）"样式：
**   [电子书 ✕] [户外 ✕]
** 点某个 chip 上的 ✕ 就删除这个标签（发出信号，由页面负责写盘）。
**/

#ifndef TAGCHIPDELEGATE_H
#define TAGCHIPDELEGATE_H

#include <QModelIndex>
#include <QRect>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QVector>

class TagChipDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit TagChipDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;

    /** 标签列不做就地编辑（双击走"增删式"标签对话框），这里拒绝创建编辑器 */
    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return nullptr;
    }


signals:
    /** 用户点了某个标签上的 ✕ */
    void removeTagRequested(const QModelIndex &index, const QString &sTag);

private:
    struct Chip
    {
        QString sTag;
        QRect rcChip;    // 整个药丸
        QRect rcClose;   // 右侧 ✕ 的点击区域
    };

    /** 按列宽把标签排成一行 chip；放不下的就不画（列宽有限） */
    static QVector<Chip> layoutChips(const QStyleOptionViewItem &option, const QStringList &qLTags);

};

#endif // TAGCHIPDELEGATE_H
