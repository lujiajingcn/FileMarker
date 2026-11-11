#include "toolicons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

#include <cmath>

namespace {

// 所有图标都在 16x16 的设计坐标里画，渲染时再按目标尺寸缩放。
const qreal kDesignSize = 16.0;
const qreal kPenWidth = 1.6;

/** 画一个箭头：从 (x1,y1) 到 (x2,y2)，箭头画在 (x2,y2) 端 */
void drawArrow(QPainter &painter, const QPointF &from, const QPointF &to)
{
    painter.drawLine(from, to);
    const QPointF dir = to - from;
    const qreal len = std::hypot(dir.x(), dir.y());
    if (len <= 0.0)
        return;
    const QPointF unit(dir.x() / len, dir.y() / len);
    const QPointF normal(-unit.y(), unit.x());
    const qreal head = 3.6;
    const QPointF wing1 = to - unit * head + normal * (head * 0.62);
    const QPointF wing2 = to - unit * head - normal * (head * 0.62);
    painter.drawLine(to, wing1);
    painter.drawLine(to, wing2);
}

void paintIcon(QPainter &painter, ToolIcons::Kind kind, const QColor &color)
{
    QPen pen(color);
    pen.setWidthF(kPenWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (kind) {
    case ToolIcons::Ai: {
        // 直接写 "AI"：比画图形更清楚，和原来的图标一致
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(13);
        painter.setFont(font);
        painter.drawText(QRectF(0, 0, kDesignSize, kDesignSize), Qt::AlignCenter,
                         QStringLiteral("AI"));
        break;
    }
    case ToolIcons::AddTag: {
        // 回形针：外圈两个圆角的 U 形 + 内侧一条直线，整体斜放
        painter.save();
        painter.translate(kDesignSize / 2.0, kDesignSize / 2.0);
        painter.rotate(-32);
        painter.drawRoundedRect(QRectF(-3.2, -6.0, 6.4, 12.0), 3.2, 3.2);
        painter.drawRoundedRect(QRectF(-1.2, -3.6, 2.4, 7.2), 1.2, 1.2);
        painter.restore();
        break;
    }
    case ToolIcons::ClearTags: {
        // 剪刀：两个把手圆 + 两片刀刃
        painter.drawEllipse(QPointF(3.4, 11.6), 2.1, 2.1);
        painter.drawEllipse(QPointF(10.2, 11.6), 2.1, 2.1);
        painter.drawLine(QPointF(4.9, 10.1), QPointF(12.4, 3.2));
        painter.drawLine(QPointF(8.7, 10.1), QPointF(1.2, 3.2));
        break;
    }
    case ToolIcons::Search: {
        painter.drawEllipse(QPointF(6.6, 6.6), 4.3, 4.3);
        painter.drawLine(QPointF(9.8, 9.8), QPointF(13.6, 13.6));
        break;
    }
    case ToolIcons::Traverse: {
        // 叠放的三层"缓存盘"
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        const qreal barHeight = 3.2;
        const qreal radius = barHeight / 2.0;
        for (int i = 0; i < 3; ++i) {
            painter.drawRoundedRect(QRectF(1.6, 1.8 + i * 4.3, 12.8, barHeight),
                                    radius, radius);
        }
        break;
    }
    case ToolIcons::Back:
        drawArrow(painter, QPointF(13.0, 8.0), QPointF(3.2, 8.0));
        break;
    case ToolIcons::Forward:
        drawArrow(painter, QPointF(3.0, 8.0), QPointF(12.8, 8.0));
        break;
    case ToolIcons::Up:
        drawArrow(painter, QPointF(8.0, 13.0), QPointF(8.0, 3.2));
        break;
    case ToolIcons::Refresh: {
        // 缺口圆 + 箭头：表示"重新加载"
        const QRectF box(2.6, 2.6, 10.8, 10.8);
        painter.drawArc(box, 40 * 16, 280 * 16);
        const qreal angle = qDegreesToRadians(40.0);
        const QPointF tip(8.0 + 5.4 * std::cos(angle), 8.0 - 5.4 * std::sin(angle));
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        QPainterPath head;
        head.moveTo(tip + QPointF(0.2, -3.2));
        head.lineTo(tip + QPointF(3.0, 1.2));
        head.lineTo(tip + QPointF(-2.2, 1.6));
        head.closeSubpath();
        painter.drawPath(head);
        break;
    }
    }
}

/** 在指定像素尺寸上渲染一张图标 */
QPixmap renderPixmap(ToolIcons::Kind kind, int pixelSize, const QColor &color)
{
    QPixmap pixmap(pixelSize, pixelSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const qreal scale = pixelSize / kDesignSize;
    painter.scale(scale, scale);
    paintIcon(painter, kind, color);
    painter.end();
    return pixmap;
}

}   // namespace

QIcon ToolIcons::make(Kind kind, const QColor &color)
{
    // 逐个尺寸渲染，QIcon 会按控件需要的实际像素大小挑最合适的一张，
    // 这样 100%/150%/200% 缩放下都不会出现放大锯齿。
    static const int kSizes[] = {16, 20, 24, 32, 40, 48, 64, 96};
    QIcon icon;
    for (int size : kSizes)
        icon.addPixmap(renderPixmap(kind, size, color));
    return icon;
}
