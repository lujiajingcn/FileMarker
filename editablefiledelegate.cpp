#include "editablefiledelegate.h"

#include <QtMath>
#include <QFileInfo>

#include "tagchipdelegate.h"
#include "utility.h"
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

// 缩略图里的标签 chip（药丸）
const QColor COLOR_CHIP_BG(0xE8, 0xF5, 0xE9);
const QColor COLOR_CHIP_BORDER(0xA5, 0xD6, 0xA7);
const QColor COLOR_CHIP_TEXT(0x1B, 0x5E, 0x20);
const QColor COLOR_CHIP_CLOSE(0x6B, 0x8E, 0x6E);

struct TagChip
{
    QString sTag;
    QRectF  rcChip;    // 相对 customRect 左上角
    QRectF  rcClose;
};

struct TagChipLayout
{
    QVector<TagChip> chips;
    int              nHeight = 0;   // 整个标签区的高度（0 表示没有标签）
};

/**
 * 把标签排成"最多 nMaxRows 行、每行居中"的 chip 布局。
 * 坐标以 (0,0) 为原点，调用方自行 translate 到目标区域。
 */
TagChipLayout buildTagChipLayout(const QFont &tagFont, const QStringList &qLTags,
                                 int nWidth, int nMaxRows)
{
    TagChipLayout layout;
    if (qLTags.isEmpty() || nWidth <= 0 || nMaxRows <= 0)
        return layout;

    const QFontMetrics fm(tagFont);
    const int nChipHeight = fm.height() + 6;
    const int nGap = 4;
    const int nCloseSize = 10;

    struct Row
    {
        QVector<int> qLIndex;
        int          nWidth = 0;
    };
    QVector<Row> rows;
    QVector<int> qLChipWidths;
    QVector<QString> qLChipTags;

    for (const QString &sTag : qLTags) {
        const int nChipWidth = 7 + fm.horizontalAdvance(sTag) + 3 + nCloseSize + 5;
        if (rows.isEmpty() || rows.last().nWidth + nGap + nChipWidth > nWidth) {
            if (rows.size() >= nMaxRows)
                break;   // 行数用完了，后面的标签不显示
            rows.append(Row());
        }
        Row &row = rows.last();
        row.qLIndex.append(qLChipTags.size());
        row.nWidth += (row.qLIndex.size() > 1 ? nGap : 0) + nChipWidth;
        qLChipTags.append(sTag);
        qLChipWidths.append(nChipWidth);
    }

    layout.nHeight = rows.size() * nChipHeight + (rows.size() - 1) * 2;
    int nTop = 0;
    for (const Row &row : rows) {
        int nLeft = qMax(0, (nWidth - row.nWidth) / 2);   // 每行居中
        for (int i = 0; i < row.qLIndex.size(); ++i) {
            const int nIndex = row.qLIndex.at(i);
            const int nChipWidth = qLChipWidths.at(nIndex);

            TagChip chip;
            chip.sTag = qLChipTags.at(nIndex);
            chip.rcChip = QRectF(nLeft, nTop, nChipWidth, nChipHeight);
            chip.rcClose = QRectF(nLeft + nChipWidth - 5 - nCloseSize,
                                  nTop + (nChipHeight - nCloseSize) / 2.0,
                                  nCloseSize, nCloseSize);
            layout.chips.append(chip);
            nLeft += nChipWidth + nGap;
        }
        nTop += nChipHeight + 2;
    }
    return layout;
}

// 文件名按字符换行，最多 maxLines 行；放不下时最后一行加省略号。
// 之前直接 drawText(TextWordWrap) 会被裁剪且没有省略号，用户看不出名字被截断。
/**
 * 取某个缩略图格子的标签列表。
 * 目录行的标签列是占位符"—"，不能当成一个标签画成 chip。
 */
QStringList tagListFor(const QModelIndex &index)
{
    const QString sText =
        index.sibling(index.row(), 4).data(Qt::DisplayRole).toString().trimmed();
    if (sText.isEmpty() || sText == QStringLiteral("\u2014"))
        return QStringList();
    return Utility::splitLabels(sText);
}

QStringList nameLinesFor(const QFontMetrics &fm, const QString &text, int width, int maxLines)
{
    QStringList lines;
    if (text.isEmpty() || width <= 0 || maxLines <= 0)
        return lines;

    QString sRest = text;
    while (!sRest.isEmpty() && lines.size() < maxLines) {
        if (fm.horizontalAdvance(sRest) <= width) {
            lines << sRest;
            sRest.clear();
            break;
        }

        int nLow = 1;
        int nHigh = sRest.size();
        int nFit = 1;
        while (nLow <= nHigh) {
            const int nMid = (nLow + nHigh) / 2;
            if (fm.horizontalAdvance(sRest.left(nMid)) <= width) {
                nFit = nMid;
                nLow = nMid + 1;
            } else {
                nHigh = nMid - 1;
            }
        }
        nFit = qBound(1, nFit, sRest.size());

        if (lines.size() + 1 >= maxLines) {
            lines << fm.elidedText(sRest, Qt::ElideRight, width);
            sRest.clear();
        } else {
            lines << sRest.left(nFit);
            sRest = sRest.mid(nFit);
        }
    }

    if (!sRest.isEmpty() && !lines.isEmpty())
        lines.last() = fm.elidedText(lines.last() + QChar(0x2026), Qt::ElideRight, width);

    return lines;
}


} // namespace

int EditableDelegate::preferredCellHeight(int iconSize, const QFont &font)
{
    const QFontMetrics fm(font);
    const QFontMetrics tagFm(tagFontFor(font));
    // 图标 + 两行文件名 + 两行标签 chip + 内外边距
    return ITEM_MARGIN + iconSize + ITEM_GAP
           + 2 * fm.lineSpacing() + ITEM_GAP
           + 2 * tagFm.lineSpacing() + ITEM_MARGIN;
}

EditableDelegate::EditableDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
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
    const QStringList qLTags = tagListFor(index);

    const int nameHeight = textHeightFor(nameFm, name, width, 2);
    const TagChipLayout chipLayout = buildTagChipLayout(
        tagFontFor(option.font), qLTags, width, 2);
    const int tagHeight = chipLayout.nHeight;

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

    // 标签双击区域：有标签时覆盖 chip 区域；没有标签时放在文件名下方的一行，
    // 这样还没打标签的文件在缩略图里也能双击这个位置进入编辑。
    if (!layout.customRect.isEmpty()) {
        layout.tagEditRect = layout.customRect;
    } else {
        const int editTop = layout.nameRect.isEmpty()
                                ? layout.iconRect.bottom() + ITEM_GAP
                                : layout.nameRect.bottom() + ITEM_GAP;
        layout.tagEditRect = QRect(option.rect.left() + ITEM_MARGIN, editTop, width, tagFm.lineSpacing());
    }

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
    const qreal dpr = painter->device() != nullptr ? painter->device()->devicePixelRatioF() : 1.0;
    QIcon icon;
    if (m_thumbnailProvider != nullptr) {
        m_thumbnailProvider->setPaintDevicePixelRatio(dpr);
        const QString sFilePath = index.data(QFileSystemModel::FilePathRole).toString();
        if (!sFilePath.isEmpty())
            icon = m_thumbnailProvider->icon(QFileInfo(sFilePath));
    }
    if (icon.isNull())
        icon = index.data(Qt::DecorationRole).value<QIcon>();

    if (!icon.isNull()) {
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

    // 文件名：主文字颜色（裁剪到文字区域，超长时加省略号，不会画到标签/相邻项上）
    if (!layout.nameRect.isEmpty()) {
        painter->save();
        painter->setClipRect(layout.nameRect);
        painter->setFont(option.font);
        painter->setPen(selected ? option.palette.color(QPalette::HighlightedText)
                                 : option.palette.color(QPalette::Text));
        const QFontMetrics fm(option.font);
        const int nMaxLines = qMax(1, layout.nameRect.height() / qMax(1, fm.lineSpacing()));
        const QStringList nameLines = nameLinesFor(fm, index.data(Qt::DisplayRole).toString(),
                                                   layout.nameRect.width(), nMaxLines);
        QRect lineRect(layout.nameRect.left(), layout.nameRect.top(),
                       layout.nameRect.width(), fm.lineSpacing());
        for (const QString &sLine : nameLines) {
            painter->drawText(lineRect, Qt::AlignHCenter | Qt::AlignVCenter, sLine);
            lineRect.translate(0, fm.lineSpacing());
        }
        painter->restore();
    }

    // 标签：画成带 ✕ 的小药丸（和详情视图的标签列保持一致）
    if (!layout.customRect.isEmpty()) {
        const QFont tagFont = tagFontFor(option.font);
        const TagChipLayout chipLayout =
            buildTagChipLayout(tagFont, tagListFor(index), layout.customRect.width(), 2);
        if (!chipLayout.chips.isEmpty()) {
            painter->save();
            painter->setClipRect(layout.customRect);
            painter->translate(layout.customRect.topLeft());
            painter->setFont(tagFont);
            painter->setRenderHint(QPainter::Antialiasing, true);

            for (const TagChip &chip : chipLayout.chips) {
                const qreal dRadius = chip.rcChip.height() / 2.0;
                painter->setPen(QPen(COLOR_CHIP_BORDER, 1.0));
                painter->setBrush(COLOR_CHIP_BG);
                painter->drawRoundedRect(chip.rcChip.adjusted(0.5, 0.5, -0.5, -0.5), dRadius, dRadius);

                const QRectF rcText(chip.rcChip.left() + 7, chip.rcChip.top(),
                                    chip.rcChip.width() - 7 - 13, chip.rcChip.height());
                painter->setPen(COLOR_CHIP_TEXT);
                painter->drawText(rcText, Qt::AlignVCenter | Qt::AlignLeft, chip.sTag);

                painter->setPen(QPen(COLOR_CHIP_CLOSE, 1.2));
                const QRectF rcClose = chip.rcClose.adjusted(2.5, 2.5, -2.5, -2.5);
                painter->drawLine(rcClose.topLeft(), rcClose.bottomRight());
                painter->drawLine(rcClose.bottomLeft(), rcClose.topRight());
            }
            painter->restore();
        }
    }

    painter->restore();
}

bool EditableDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    // 单击标签 chip 上的 ✕ = 删除这个标签（和详情视图的标签列一致）
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            const LayoutRects layout = calculateRects(option, index);
            if (!layout.customRect.isEmpty()) {
                const TagChipLayout chipLayout =
                    buildTagChipLayout(tagFontFor(option.font), tagListFor(index),
                                       layout.customRect.width(), 2);
                const QPoint local = me->pos() - layout.customRect.topLeft();
                for (const TagChip &chip : chipLayout.chips) {
                    if (chip.rcClose.contains(local)) {
                        emit sendRemoveTag(index, chip.sTag);
                        return true;   // 吃掉点击，避免同时选中/进入编辑
                    }
                }
            }
        }
    }

    if (event->type() == QEvent::MouseButtonDblClick)
    {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        const LayoutRects layout = calculateRects(option, index);
        const QPoint mousePos = me->pos();

        // 双击标签区域（没标签时是文件名下方那一行）= 打开"增删式"标签对话框，
        // 不再走"就地编辑第 5 列文本"的老路（那条路会把全部标签当成一行文本替换掉）。
        if (layout.tagEditRect.contains(mousePos)) {
            emit sendEditTag(index);
            return true;
        }
        // 双击文件名与双击其它区域语义统一为"打开/进入"，
        // 不再"双击文件名 = 就地重命名"（资源管理器里没有这个行为，容易误改名）。
        emit sendDblClick(index);
        return true;
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// 缩略图视图已把 editTriggers 设为 NoEditTriggers：
// 双击=打开、双击标签区=弹标签对话框、重命名走右键菜单。
// 因此"编辑意图"那套成员状态不再需要，这里只保留默认实现。
QWidget *EditableDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
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

// 让编辑器铺满标签列宽度（标签文字区可能只有一行高，但输入框需要足够宽）
void EditableDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const LayoutRects layout = calculateRects(option, index);
    if (isTagEditor(editor)) {
        const int width = qMax(1, option.rect.width() - 2 * ITEM_MARGIN);
        QRect editorRect(option.rect.left() + ITEM_MARGIN,
                         layout.tagEditRect.top(),
                         width,
                         qMax(layout.tagEditRect.height(), editor->sizeHint().height()));
        if (editorRect.bottom() > option.rect.bottom())
            editorRect.moveTop(option.rect.bottom() - editorRect.height() + 1);
        editor->setGeometry(editorRect);
        return;
    }
    editor->setGeometry(layout.nameRect);
}
