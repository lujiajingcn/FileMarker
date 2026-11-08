#ifndef THUMBNAILICONPROVIDER_H
#define THUMBNAILICONPROVIDER_H

#include <QFileIconProvider>
#include <QCache>
#include <QHash>
#include <QPixmap>

#include "common.h"

/**
 ** 自定义图标提供器：为文件生成缩略图。
 **
 ** 1. 图片文件用 QImageReader 解码（按目标尺寸直接缩放解码，省内存）；
 ** 2. 其他类型（PDF、视频、Office 等）尝试取 Windows Shell 缩略图，
 **    Explorer 能预览的类型这里也能出图；
 ** 3. 都失败时回退到系统文件类型图标。
 **
 ** 结果按“路径 + 修改时间 + 尺寸”缓存，滚动时不会重复解码。
 **/
class ThumbnailIconProvider : public QFileIconProvider
{
public:
    explicit ThumbnailIconProvider(int iconSize = ICON_SIZE);

    /** 调整缩略图边长（逻辑像素），会清空缓存 */
    void setIconSize(int iconSize);
    int iconSize() const { return m_iconSize; }

    /** 由 delegate 在绘制前传入实际窗口的 DPR，避免多屏混合缩放时缩放不一致 */
    void setPaintDevicePixelRatio(qreal dpr) const;

    QIcon icon(const QFileInfo &info) const override;

private:
    QPixmap loadImageThumbnail(const QFileInfo &info) const;
    QPixmap loadShellThumbnail(const QFileInfo &info) const;
    QPixmap loadShellIcon(const QFileInfo &info) const;
    qreal effectiveDpr() const;

    int m_iconSize;
    mutable qreal m_paintDpr;
    mutable QCache<QString, QPixmap> m_cache;
    // 失败结果不永久缓存：记录失败时间，超过 TTL 后重新尝试（文件可能已解除占用）
    mutable QHash<QString, qint64> m_failureTime;
};

#endif // THUMBNAILICONPROVIDER_H
