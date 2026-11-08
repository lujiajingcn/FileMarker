#include "editablefiledelegate.h"

#include <QtMath>
#include <QFileInfo>

#include "thumbnailiconprovider.h"

namespace {

const int ITEM_MARGIN = 6;      // 文本区域左右留白
const int ITEM_GAP = 4;         // 图标与文字、文件名与标签之间的间距

// 按可用宽度计算文本需要几行（最多两行），并返回对应高度
int textHeightFor(const QFontMetrics &fm, const QString &text, int width, int maxLines)
{
    if (text.isEmpty() || width <= 0)
        return 0;

    const QRect bound = fm.boundingRect(QRect(0, 0, width, fm.lineSpacing() * (maxLines + 2)),
                                        Qt::AlignHCenter | Qt::TextWordWrap, text);
    int lines = qCeil(qreal(bound.height()) / qMax(1, fm.lineSpacing()));
    lines = qBound(1, lines, maxLines);
    return lines * fm.lineSpacing();
}

QFont tagFontFor(const QFont &base)
{
    QFont font = base;
    if (font.pointSizeF() > 1.0)
        font.setPointSizeF(font.pointSizeF() - 1.0);
    return font;
}

} // namespace

EditableDelegate::EditableDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
    , m_editTarget(TargetNone)
    , m_iconSize(ICON_SIZE)
    , m_thumbnailProvider(nullptr)
{
}

void EditableDelegate::setIconSize(int iconSize)
{
    if (iconSize > 0)
        m_iconSize = iconSize;
}

void EditableDelegate::setHoveredIndex(const QModelIndex &index)
{
    m_hoveredIndex = index;
}

void EditableDelegate::setThumbnailProvider(ThumbnailIconProvider *provider)
{
    m_thumbnailProvider = provider;
}

LayoutRects EditableDelegate::calculateRects(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    LayoutRects layout;

    const int width = qMax(1, option.rect.width() - 2 * ITEM_MARGIN);
    const QFontMetrics nameFm(option.font);
    const QFontMetrics tagFm(tagFontFor(option.font));

    const QString name = index.data(Qt::DisplayRole).toString();
    const QString tags = index.sibling(index.row(), 4).data(Qt::DisplayRole).toString();

    const int nameHeight = textHeightFor(nameFm, name, width, 2);
    const int tagHeight = textHeightFor(tagFm, tags, width, 2);

    const int totalHeight = m_iconSize
                            + (nameHeight > 0 ? ITEM_GAP + nameHeight : 0)
                            + (tagHeight > 0 ? ITEM_GAP + tagHeight : 0);

    // 内容整体垂直居中：没有标签时不会在下方留出一条空白
    const int top = option.rect.top() + qMax(0, (option.rect.height() - totalHeight) / 2);

    layout.iconRect = QRect(option.rect.center().x() - m_iconSize / 2, top, m_iconSize, m_iconSize);

    int y = layout.iconRect.bottom() + ITEM_GAP;
    if (nameHeight > 0) {
        layout.nameRect = QRect(option.rect.left() + ITEM_MARGIN, y, width, nameHeight);
        y = layout.nameRect.bottom() + ITEM_GAP;
    }
    if (tagHeight > 0)
        layout.customRect = QRect(option.rect.left() + ITEM_MARGIN, y, width, tagHeight);

    return layout;
}

QSize EditableDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);

    const int side = m_iconSize + THUMB_GRID_MARGIN;
    return QSize(side, side);
}

void EditableDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    painter->save();

    const LayoutRects layout = calculateRects(option, index);
    const bool selected = (option.state & QStyle::State_Selected) != 0;
    const bool hovered = m_hoveredIndex.isValid() && m_hoveredIndex == index;

    // 背景：选中用系统高亮；悬停只铺一层淡色圆角，避免整格填满显得沉重
    if (selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    } else if (hovered) {
        QColor hoverColor = option.palette.color(QPalette::Highlight);
        hoverColor.setAlpha(28);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(hoverColor);
        painter->drawRoundedRect(option.rect.adjusted(2, 2, -2, -2), 6, 6);
    }

    // 图标：在 GUI 线程里向缩略图生成器要图（带缓存）。
    // 不用 model 的 DecorationRole，是因为 QFileSystemModel 在后台线程取图标，
    // 而 QPixmap / Shell COM 只能在 GUI 线程安全使用。
    QIcon icon;
    if (m_thumbnailProvider != nullptr) {
        const QString sFilePath = index.data(QFileSystemModel::FilePathRole).toString();
        if (!sFilePath.isEmpty())
            icon = m_thumbnailProvider->icon(QFileInfo(sFilePath));
    }
    if (icon.isNull())
        icon = index.data(Qt::DecorationRole).value<QIcon>();

    if (!icon.isNull()) {
        const qreal dpr = painter->device() != nullptr ? painter->device()->devicePixelRatioF() : 1.0;
        const int targetPx = qMax(1, qRound(m_iconSize * dpr));
        const QPixmap pixmap = icon.pixmap(QSize(targetPx, targetPx),
                                           selected ? QIcon::Selected : QIcon::Normal);
        if (!pixmap.isNull()) {
            const qreal pixmapDpr = pixmap.devicePixelRatio() > 0 ? pixmap.devicePixelRatio() : 1.0;
            const QSize logicalSize(qRound(pixmap.width() / pixmapDpr),
                                    qRound(pixmap.height() / pixmapDpr));
            const QSize drawSize = logicalSize.scaled(layout.iconRect.size(), Qt::KeepAspectRatio);
            const QRect drawRect(layout.iconRect.center()
                                     - QPoint(drawSize.width() / 2, drawSize.height() / 2),
                                 drawSize);
            painter->drawPixmap(drawRect, pixmap);
        }
    }

    // 文件名：主文字颜色（裁剪到文字区域，超长时不会画到标签/相邻项上）
    if (!layout.nameRect.isEmpty()) {
        painter->save();
        painter->setClipRect(layout.nameRect);
        painter->setFont(option.font);
        painter->setPen(selected ? option.palette.color(QPalette::HighlightedText)
                                 : option.palette.color(QPalette::Text));
        painter->drawText(layout.nameRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                          index.data(Qt::DisplayRole).toString());
        painter->restore();
    }

    // 标签：次要样式（小一号、半透明），与文件名区分层次
    if (!layout.customRect.isEmpty()) {
        QColor tagColor = selected ? option.palette.color(QPalette::HighlightedText)
                                   : option.palette.color(QPalette::Text);
        tagColor.setAlpha(selected ? 230 : 150);

        painter->save();
        painter->setClipRect(layout.customRect);
        painter->setFont(tagFontFor(option.font));
        painter->setPen(tagColor);
        painter->drawText(layout.customRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                          index.sibling(index.row(), 4).data(Qt::DisplayRole).toString());
        painter->restore();
    }

    painter->restore();
}

bool EditableDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonDblClick)
    {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        const LayoutRects layout = calculateRects(option, index);
        const QPoint mousePos = me->pos();

        // 双击标签区域：切换为“就地编辑标签”，交给视图打开第 5 列的编辑器
        if (!layout.customRect.isEmpty() && layout.customRect.contains(mousePos)) {
            m_editTarget = TargetCustomData;
            return false;
        }
        // 双击文件名区域：编辑文件名（沿用原有行为）
        if (layout.nameRect.contains(mousePos)) {
            m_editTarget = TargetFileName;
            return false;
        }
        // 其他区域：双击直接打开文件
        m_editTarget = TargetNone;
        emit sendDblClick(index);
        return true;
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QWidget *EditableDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const EditTarget target = m_editTarget;
    // 编辑意图只对本次创建编辑器有效，用掉即清空，
    // 否则之后按 F2 等其它方式编辑时还会沿用上一次“编辑标签”的目标。
    m_editTarget = TargetNone;

    if (target == TargetCustomData)
    {
        QLineEdit *editor = new QLineEdit(parent);
        editor->setFont(tagFontFor(option.font));
        editor->setPlaceholderText(index.sibling(index.row(), 4).data(Qt::DisplayRole).toString());
        editor->setProperty("fmEditTarget", int(TargetCustomData));
        return editor;
    }
    // 否则返回默认编辑器（文件名）
    return QStyledItemDelegate::createEditor(parent, option, index);
}

// 编辑器的编辑目标记录在编辑器自身的属性上，避免成员变量残留影响后续编辑
static bool isTagEditor(const QWidget *editor)
{
    return editor != nullptr && editor->property("fmEditTarget").toInt() == int(TargetCustomData);
}

// 从第 5 列（标签列）加载数据到编辑器
void EditableDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    if (isTagEditor(editor))
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit)
            lineEdit->setText(index.sibling(index.row(), 4).data(Qt::EditRole).toString());
        return;
    }
    QStyledItemDelegate::setEditorData(editor, index);
}

// 把编辑器内容写回第 5 列（标签列），由模型通知 FormFileBrowser 写 ADS
void EditableDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    if (isTagEditor(editor))
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
        if (lineEdit)
            model->setData(index.sibling(index.row(), 4), lineEdit->text(), Qt::EditRole);
        return;
    }
    QStyledItemDelegate::setModelData(editor, model, index);
}

// 让编辑器正好覆盖在对应的文字上
void EditableDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const LayoutRects layout = calculateRects(option, index);
    if (isTagEditor(editor)) {
        editor->setGeometry(layout.customRect.isEmpty() ? layout.nameRect : layout.customRect);
        return;
    }
    editor->setGeometry(layout.nameRect);
}
