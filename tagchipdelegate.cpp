#include "tagchipdelegate.h"

#include "utility.h"

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>

namespace {

// chip 配色：浅绿底 + 深绿字，和"标签"这个词的语义对齐
const QColor COLOR_CHIP_BG(0xE8, 0xF5, 0xE9);
const QColor COLOR_CHIP_BORDER(0xA5, 0xD6, 0xA7);
const QColor COLOR_CHIP_TEXT(0x1B, 0x5E, 0x20);
const QColor COLOR_CLOSE(0x6B, 0x8E, 0x6E);

const int CHIP_MARGIN_LEFT = 6;   // 单元格左内边距
const int CHIP_MARGIN_RIGHT = 4;  // 右内边距
const int CHIP_GAP = 6;           // chip 之间
const int CHIP_PADDING_LEFT = 8;  // chip 内文字左内边距
const int CHIP_CLOSE_SIZE = 12;   // ✕ 的可点区域边长
const int CHIP_PADDING_RIGHT = 6;

}   // namespace

TagChipDelegate::TagChipDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}


QVector<TagChipDelegate::Chip> TagChipDelegate::layoutChips(const QStyleOptionViewItem &option,
                                                            const QStringList &qLTags)
{
    QVector<Chip> qLChips;
    const QFontMetrics fm(option.font);
    const int nChipHeight = qMin(option.rect.height() - 4, fm.height() + 8);
    if (nChipHeight <= 6)
        return qLChips;

    const int nTop = option.rect.top() + (option.rect.height() - nChipHeight) / 2;
    int nLeft = option.rect.left() + CHIP_MARGIN_LEFT;
    const int nRightLimit = option.rect.right() - CHIP_MARGIN_RIGHT;

    for (const QString &sTag : qLTags) {
        const int nTextWidth = fm.horizontalAdvance(sTag);
        const int nChipWidth = CHIP_PADDING_LEFT + nTextWidth + 4 + CHIP_CLOSE_SIZE + CHIP_PADDING_RIGHT;
        if (nLeft + nChipWidth > nRightLimit)
            break;   // 列宽不够了，剩下的不画

        Chip chip;
        chip.sTag = sTag;
        chip.rcChip = QRect(nLeft, nTop, nChipWidth, nChipHeight);
        chip.rcClose = QRect(nLeft + nChipWidth - CHIP_PADDING_RIGHT - CHIP_CLOSE_SIZE,
                             nTop + (nChipHeight - CHIP_CLOSE_SIZE) / 2,
                             CHIP_CLOSE_SIZE, CHIP_CLOSE_SIZE);
        qLChips.append(chip);
        nLeft += nChipWidth + CHIP_GAP;
    }
    return qLChips;
}

void TagChipDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const QString sLabels = opt.text;
    opt.text.clear();

    // 背景（交替行颜色、选中高亮、焦点框）仍交给样式画
    QStyle *pStyle = opt.widget != nullptr ? opt.widget->style() : QApplication::style();
    pStyle->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QStringList qLTags = Utility::splitLabels(sLabels);
    // 目录那一行模型给的是占位符"—"，不能当成一个标签画成 chip
    const bool bHasTags = !qLTags.isEmpty() && sLabels.trimmed() != QStringLiteral("—");
    if (!bHasTags) {
        // 目录是 "—"、空标签是空白：按普通文本画
        if (!sLabels.isEmpty()) {
            painter->save();
            const bool bSelected = (opt.state & QStyle::State_Selected) != 0;
            painter->setPen(opt.palette.color(bSelected ? QPalette::HighlightedText : QPalette::Text));
            painter->drawText(opt.rect.adjusted(CHIP_MARGIN_LEFT, 0, -CHIP_MARGIN_RIGHT, 0),
                              Qt::AlignVCenter | Qt::AlignLeft, sLabels);
            painter->restore();
        }
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    for (const Chip &chip : layoutChips(opt, qLTags)) {
        const qreal dRadius = chip.rcChip.height() / 2.0;

        // 药丸底 + 描边
        painter->setPen(QPen(COLOR_CHIP_BORDER, 1.0));
        painter->setBrush(COLOR_CHIP_BG);
        painter->drawRoundedRect(QRectF(chip.rcChip).adjusted(0.5, 0.5, -0.5, -0.5),
                                 dRadius, dRadius);

        // 标签文字
        const QRect rcText(chip.rcChip.left() + CHIP_PADDING_LEFT, chip.rcChip.top(),
                           chip.rcChip.width() - CHIP_PADDING_LEFT - CHIP_CLOSE_SIZE - CHIP_PADDING_RIGHT,
                           chip.rcChip.height());
        painter->setPen(COLOR_CHIP_TEXT);
        painter->drawText(rcText, Qt::AlignVCenter | Qt::AlignLeft, chip.sTag);

        // ✕
        painter->setPen(QPen(COLOR_CLOSE, 1.4));
        const QRectF rcClose = QRectF(chip.rcClose).adjusted(3.0, 3.0, -3.0, -3.0);
        painter->drawLine(rcClose.topLeft(), rcClose.bottomRight());
        painter->drawLine(rcClose.bottomLeft(), rcClose.topRight());
    }

    painter->restore();
}

bool TagChipDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                  const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *pMouseEvent = static_cast<QMouseEvent *>(event);
        if (pMouseEvent->button() == Qt::LeftButton) {
            const QStringList qLTags = Utility::splitLabels(index.data(Qt::DisplayRole).toString());
            for (const Chip &chip : layoutChips(option, qLTags)) {
                if (chip.rcClose.contains(pMouseEvent->pos())) {
                    emit removeTagRequested(index, chip.sTag);
                    return true;   // 吃掉这次点击，避免顺带触发选中/双击
                }
            }
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
