#include "toolicons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QUrl>
#include <QStandardPaths>
#include <QHash>
#include <QFileInfo>
#include <QDir>
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


/** 画一个"标签树"的图形：上面一个方框、下面两个方框，用折线连起来 */
void drawTreeNodes(QPainter &painter, const QRectF &top, const QRectF &leftBottom,
                   const QRectF &rightBottom)
{
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(top);
    painter.drawRect(leftBottom);
    painter.drawRect(rightBottom);

    const qreal midX = top.center().x();
    const qreal midY = (top.bottom() + leftBottom.top()) / 2.0;
    painter.drawLine(QPointF(midX, top.bottom()), QPointF(midX, midY));
    painter.drawLine(QPointF(leftBottom.center().x(), midY),
                     QPointF(rightBottom.center().x(), midY));
    painter.drawLine(QPointF(leftBottom.center().x(), midY),
                     QPointF(leftBottom.center().x(), leftBottom.top()));
    painter.drawLine(QPointF(rightBottom.center().x(), midY),
                     QPointF(rightBottom.center().x(), rightBottom.top()));
}

/** 画加减号 */
void drawPlusMinus(QPainter &painter, const QPointF &center, qreal half, bool plus)
{
    painter.drawLine(QPointF(center.x() - half, center.y()),
                     QPointF(center.x() + half, center.y()));
    if (plus) {
        painter.drawLine(QPointF(center.x(), center.y() - half),
                         QPointF(center.x(), center.y() + half));
    }
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
    case ToolIcons::Computer: {
        painter.drawRoundedRect(QRectF(1.4, 2.6, 13.2, 8.8), 1.6, 1.6);
        painter.drawLine(QPointF(8.0, 11.4), QPointF(8.0, 13.4));
        painter.drawLine(QPointF(4.8, 13.6), QPointF(11.2, 13.6));
        break;
    }
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
    case ToolIcons::AddNode:
        drawTreeNodes(painter, QRectF(3.0, 1.6, 4.6, 4.6),
                      QRectF(0.8, 9.4, 4.6, 4.6), QRectF(5.8, 9.4, 4.6, 4.6));
        drawPlusMinus(painter, QPointF(13.0, 4.0), 2.0, true);
        break;
    case ToolIcons::AddChild:
        drawTreeNodes(painter, QRectF(3.0, 1.6, 4.6, 4.6),
                      QRectF(0.8, 9.4, 4.6, 4.6), QRectF(5.8, 9.4, 4.6, 4.6));
        drawPlusMinus(painter, QPointF(13.0, 12.0), 2.0, true);
        break;
    case ToolIcons::DeleteNode:
        drawTreeNodes(painter, QRectF(3.0, 1.6, 4.6, 4.6),
                      QRectF(0.8, 9.4, 4.6, 4.6), QRectF(5.8, 9.4, 4.6, 4.6));
        drawPlusMinus(painter, QPointF(13.0, 8.0), 2.2, false);
        break;
    case ToolIcons::MoveUp:
    case ToolIcons::MoveDown: {
        // 两个节点 + 箭头，箭头在上/下
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(1.2, 1.5, 4.6, 4.6));
        painter.drawRect(QRectF(1.2, 9.9, 4.6, 4.6));
        if (kind == ToolIcons::MoveUp)
            drawArrow(painter, QPointF(12.0, 14.5), QPointF(12.0, 2.5));
        else
            drawArrow(painter, QPointF(12.0, 1.5), QPointF(12.0, 13.5));
        break;
    }
    case ToolIcons::MoveLeft:
    case ToolIcons::MoveRight: {
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(2.0, 1.2, 4.4, 4.4));
        painter.drawRect(QRectF(2.0, 9.2, 4.4, 4.4));
        if (kind == ToolIcons::MoveLeft)
            drawArrow(painter, QPointF(15.0, 7.8), QPointF(8.6, 7.8));
        else
            drawArrow(painter, QPointF(8.0, 7.8), QPointF(14.4, 7.8));
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

namespace {

/** 把图标渲染成指定像素尺寸的图片（供白图 URL 用） */
QImage renderImage(int kind, int pixelSize, const QColor &color)
{
    QImage image(pixelSize, pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const qreal scale = pixelSize / kDesignSize;
    painter.scale(scale, scale);
    paintIcon(painter, static_cast<ToolIcons::Kind>(kind), color);
    painter.end();
    return image;
}

}   // namespace

QString ToolIcons::whiteImageUrl(Kind kind)
{
    // 深色背景下用白图；渲染成 96x96，富文本里再按 16x16 显示，
    // 相当于超采样缩小，高分屏下不会糊。
    static QHash<int, QString> cache;
    if (cache.contains(kind))
        return cache.value(kind);

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                        + QStringLiteral("/icon-cache");
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/white-%1.png").arg(int(kind));

    QFileInfo info(path);
    if (!info.exists() || info.size() == 0) {
        renderImage(kind, 96, Qt::white).save(path, "PNG");
    }
    const QString url = QUrl::fromLocalFile(path).toString();
    cache.insert(kind, url);
    return url;
}

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
