#include "editablefiledelegate.h"

EditableDelegate::EditableDelegate(QObject *parent)
    : QStyledItemDelegate(parent), m_editTarget(TargetNone)
{

}

LayoutRects EditableDelegate::calculateRects(const QStyleOptionViewItem &option) const
{
    QRect rect = option.rect;
    int iconSize = ICON_SIZE;
    int padding = 5;

    LayoutRects layout;
    // 图标区域
    layout.iconRect = QRect(rect.center().x() - iconSize/2, rect.top() + padding, iconSize, iconSize);

    // 文件名区域 (图标下方)
    layout.nameRect = QRect(rect.left(), layout.iconRect.bottom() + padding, rect.width(), 20);

    // 自定义数据区域 (文件名下方)
    layout.customRect = QRect(rect.left(), layout.nameRect.bottom(), rect.width(), 20);

    return layout;
}

QSize EditableDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(GRID_SIZE);
    size.setWidth(qMax(size.width(), GRID_SIZE));
    return size;
}

void EditableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid())
    {
        return;
    }
    painter->save();

    LayoutRects layout = calculateRects(option);

    // 绘制选中背景
    if (option.state & QStyle::State_Selected)
    {
        painter->fillRect(option.rect, option.palette.highlight());
        painter->setPen(option.palette.highlightedText().color());
    }
    else
    {
        painter->setPen(option.palette.text().color());
    }

    // 绘制图标
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    icon.paint(painter, layout.iconRect, Qt::AlignCenter);

    // 绘制文件名
    QString fileName = index.data(Qt::DisplayRole).toString();
    painter->drawText(layout.nameRect, Qt::AlignHCenter | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(fileName, Qt::ElideRight, layout.nameRect.width()));

    // 绘制自定义数据 (第4列)
    QModelIndex customIndex = index.siblingAtColumn(4);
    QString customData = customIndex.data(Qt::DisplayRole).toString();
    QString headerName = index.model()->headerData(4, Qt::Horizontal).toString();

    // 稍微变个颜色区分一下
//    painter->save();
//    if (!(option.state & QStyle::State_Selected)){
//        painter->setPen(Qt::darkBlue);
//    }
    painter->drawText(layout.customRect, Qt::AlignHCenter | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(QString("%1").arg(customData), Qt::ElideRight, layout.nameRect.width()));
    painter->restore();
}

bool EditableDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonDblClick)
    {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        LayoutRects layout = calculateRects(option);

        // 判断点击位置
        if (layout.customRect.contains(me->pos()))
        {
            m_editTarget = TargetCustomData; // 标记为编辑自定义列

            QModelIndex customIndex = index.siblingAtColumn(4);
            QString customData = customIndex.data(Qt::DisplayRole).toString();

            // 强制触发编辑 (QListView 默认只有双击才编辑，这里我们可以手动触发)
            // 注意：通常需要发送信号或使用 View 的 edit() 接口，但在 editorEvent 内部比较难直接调 view->edit()
            // 这里的 return false 让 view 继续处理事件，通常 view 会在双击时调用 createEditor
            // 如果要单击即编辑，需要更复杂的处理。这里假设用户双击了该区域。
            return false;
        }
        else if(layout.nameRect.contains(me->pos()) )
        {
            m_editTarget = TargetFileName; // 默认编辑文件名
        }
        else
        {
            m_editTarget = TargetNone;
            emit sendDblClick(index);
            return true;
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QWidget *EditableDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (m_editTarget == TargetCustomData)
    {
        QLineEdit *editor = new QLineEdit(parent);
        QModelIndex customIndex = index.siblingAtColumn(4);
        QString customData = customIndex.data(Qt::DisplayRole).toString();
        editor->setPlaceholderText(customData);
        return editor;
    }
    // 否则返回默认编辑器 (文件名)
    return QStyledItemDelegate::createEditor(parent, option, index);
}

// 3. 从模型加载数据到编辑器
void EditableDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    if (m_editTarget == TargetCustomData)
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit)
        {
            // 关键：从第 4 列获取数据，而不是 index (第0列)
            QString data = index.siblingAtColumn(4).data(Qt::EditRole).toString();
            lineEdit->setText(data);
        }
    }
    else
    {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

// 4. 保存数据回模型
void EditableDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    if (m_editTarget == TargetCustomData)
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit)
        {
            // 关键：写入第 4 列
            QModelIndex customIndex = index.siblingAtColumn(4);
            model->setData(customIndex, lineEdit->text(), Qt::EditRole);
        }
    }
    else
    {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

// 5. 调整编辑器位置 (让输入框正好覆盖在文字上)
void EditableDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    LayoutRects layout = calculateRects(option);
    if (m_editTarget == TargetCustomData)
    {
        editor->setGeometry(layout.customRect);
    }
    else
    {
        editor->setGeometry(layout.nameRect);
    }
}
