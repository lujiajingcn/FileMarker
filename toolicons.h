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
    Refresh,     // 圆形箭头：刷新
    Computer,    // 显示器：路径面包屑里的"我的电脑"

    // 标签库面板的工具条
    AddNode,     // 树 + 加号：新建标签
    AddChild,    // 树 + 加号（右下）：新建子标签
    DeleteNode,  // 树 + 减号：删除标签
    MoveUp,      // 两个节点 + ↑
    MoveDown,    // 两个节点 + ↓
    MoveLeft,    // 两个节点 + ←
    MoveRight    // 两个节点 + →
};

/** 生成图标；color 是线条颜色（默认深灰，和界面文字同色）。 */
QIcon make(Kind kind, const QColor &color = QColor(0x33, 0x33, 0x33));

/**
 * 白色大图，给界面介绍里的富文本表格用（深色背景）。
 *
 * QLabel 的富文本只能引用图片 URL，塞不进 QIcon，所以这里把图标渲染成 96x96 的
 * 白色 PNG 放到缓存目录里，返回它的本地文件 URL；富文本里再按 16x16 显示，
 * 缩小显示时相当于超采样，高分屏下也是清晰的。
 */
QString whiteImageUrl(Kind kind);

}   // namespace ToolIcons

#endif   // TOOLICONS_H
