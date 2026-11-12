#ifndef THUMBNAILICONPROVIDER_H
#define THUMBNAILICONPROVIDER_H

#include <QCache>
#include <QFileIconProvider>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QSet>
#include <QThreadPool>

#include <functional>

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
 **
 ** 重要：解码/Shell 取图都是**同步阻塞**的（视频、网络盘上单次可达几十上百毫秒），
 ** 所以真正的取图在后台线程池里做，icon() 只查缓存、未命中就返回类型图标占位，
 ** 结果回来后经 readyCallback 通知视图重绘。
 **/
class ThumbnailIconProvider : public QFileIconProvider
{
public:
    /** 异步缩略图就绪的回调（在 UI 线程调用，参数是文件路径） */
    using ReadyCallback = std::function<void(const QString &)>;

    explicit ThumbnailIconProvider(int iconSize = ICON_SIZE);
    ~ThumbnailIconProvider() override;

    /** 调整缩略图边长（逻辑像素），会清空缓存 */
    void setIconSize(int iconSize);
    int iconSize() const { return m_iconSize; }

    /** 由 delegate 在绘制前传入实际窗口的 DPR，避免多屏混合缩放时缩放不一致 */
    void setPaintDevicePixelRatio(qreal dpr) const;

    /** 设置回调上下文（通常是文件页控件）：控件销毁后排队中的结果会被丢弃 */
    void setCallbackContext(QObject *context) const { m_context = context; }
    void setReadyCallback(ReadyCallback callback) const { m_readyCallback = std::move(callback); }

    QIcon icon(const QFileInfo &info) const override;

private:
    /** 后台线程：算一张缩略图（纯 QImage，不碰 QPixmap / GUI） */
    QImage computeThumbnail(const QString &sFilePath, int iconSize, qreal dpr) const;

    /** 排队异步取图（同一路径不重复排队） */
    void requestAsyncThumbnail(const QFileInfo &info, const QString &key,
                               int iconSize, qreal dpr) const;

    /** 后台算完后回到 UI 线程：转 QPixmap、写缓存、通知视图 */
    void applyAsyncResult(const QString &sFilePath, const QString &key, const QImage &image) const;

    /** 未命中缓存时的占位图标：按后缀缓存，首次对每种类型只查一次 Shell */
    QIcon typeIconFor(const QFileInfo &info) const;

    qreal effectiveDpr() const;

    int m_iconSize;
    mutable qreal m_paintDpr;
    mutable QCache<QString, QPixmap> m_cache;
    // 失败结果不永久缓存：记录失败时间，超过 TTL 后重新尝试（文件可能已解除占用）
    mutable QHash<QString, qint64> m_failureTime;

    mutable QThreadPool *m_pool = nullptr;      // 懒创建
    mutable QSet<QString> m_pending;            // 已排队、还没算完的缓存键
    mutable QHash<QString, QIcon> m_typeIcons;  // 后缀 -> 占位图标
    mutable QObject *m_context = nullptr;
    mutable ReadyCallback m_readyCallback;
};

#endif // THUMBNAILICONPROVIDER_H
