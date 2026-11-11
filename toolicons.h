#ifndef TOOLICONS_H
#define TOOLICONS_H

#include <QColor>
#include <QIcon>

/**
 * 工具条图标（纯代码绘制，不依赖位图资源）。
 *
 * 原来这些图标是 16x16 的 PNG，在 150%/200% 缩放的屏幕上会被放大 2~3 倍，
 * 边缘全是锯齿。这里用 QPainter 在**目标像素尺寸**上直接画出来：
 * 每种尺寸单独渲染一张，QIcon 会自己挑最合适的那张，高分屏下就清晰了。
 */
namespace ToolIcons
{
enum Kind
{
    Ai,          // "AI" 字样
    AddTag,      // 回形针：添加标签
    ClearTags,   // 剪刀：清空标签
    Search,      // 放大镜：查找文件
    Traverse,    // 叠放的磁盘：遍历文件（写缓存）
    Back,        // ←
    Forward,     // →
    Up,          // ↑
    Refresh      // 圆形箭头：刷新
};

/** 生成图标；color 是线条颜色（默认深灰，和界面文字同色）。 */
QIcon make(Kind kind, const QColor &color = QColor(0x33, 0x33, 0x33));

}   // namespace ToolIcons

#endif   // TOOLICONS_H
