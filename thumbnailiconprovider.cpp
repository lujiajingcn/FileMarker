#include "thumbnailiconprovider.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QScreen>
#include <QtMath>

#ifdef Q_OS_WIN
// 需要 Vista 及以上的 Shell 接口（IShellItemImageFactory / SHCreateItemFromParsingName）
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0A00
#endif
#include <QtCore/qt_windows.h>
#include <objbase.h>
#include <shobjidl.h>

// IShellItemImageFactory 的 IID。MinGW 没有 __uuidof，这里直接用常量。
static const GUID FM_IID_IShellItemImageFactory =
    { 0xbcc18b79, 0xba16, 0x442f, { 0x80, 0xc4, 0x8a, 0x59, 0xc3, 0x0c, 0x46, 0x3b } };

// Qt 通常已经初始化过 COM；这里只做兜底，失败（例如已处于 MTA）时后续会回退到文件类型图标。
static void ensureComInitialized()
{
    static const bool s_initialized = []() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    }();
    Q_UNUSED(s_initialized);
}

// 把 Shell 返回的 HBITMAP 转成 QImage（Qt5 没有 QImage::fromHBITMAP）。
// 注意：部分 Shell 缩略图返回 32bpp 但 alpha 全为 0，此时退回不透明格式，否则会全透明。
static QImage imageFromHBITMAP(HBITMAP hBitmap)
{
    if (hBitmap == nullptr)
        return QImage();

    BITMAP bm;
    if (GetObject(hBitmap, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 || bm.bmHeight <= 0)
        return QImage();

    BITMAPINFOHEADER bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = -bm.bmHeight; // 负数表示自上而下
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    QImage image(bm.bmWidth, bm.bmHeight, QImage::Format_ARGB32);
    if (image.isNull())
        return QImage();

    HDC hdc = GetDC(nullptr);
    const int lines = GetDIBits(hdc, hBitmap, 0, UINT(bm.bmHeight), image.bits(),
                                reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);

    if (lines == 0)
        return QImage();

    const QRgb *pixels = reinterpret_cast<const QRgb *>(image.constBits());
    const int count = image.width() * image.height();
    bool hasAlpha = false;
    for (int i = 0; i < count; ++i) {
        if (qAlpha(pixels[i]) != 0) {
            hasAlpha = true;
            break;
        }
    }
    if (!hasAlpha)
        image = image.convertToFormat(QImage::Format_RGB32);

    return image;
}
#endif

// 取当前屏幕的缩放比：缩略图按物理像素生成，再标回逻辑尺寸，避免高 DPI 下模糊。
static qreal currentDevicePixelRatio()
{
    const QScreen *screen = QGuiApplication::primaryScreen();
    return screen != nullptr ? screen->devicePixelRatio() : 1.0;
}

ThumbnailIconProvider::ThumbnailIconProvider(int iconSize)
    : m_iconSize(iconSize > 0 ? iconSize : ICON_SIZE)
{
    m_cache.setMaxCost(256);
}

void ThumbnailIconProvider::setIconSize(int iconSize)
{
    if (iconSize <= 0 || iconSize == m_iconSize)
        return;

    m_iconSize = iconSize;
    m_cache.clear();
}

QIcon ThumbnailIconProvider::icon(const QFileInfo &info) const
{
    const qreal dpr = currentDevicePixelRatio();

    if (info.isDir()) {
        // 文件夹也缓存：滚动/悬停会反复取图标，QFileIconProvider 每次都要查 Shell。
        const QString key = QStringLiteral("dir|%1|%2|%3")
                                .arg(info.filePath())
                                .arg(m_iconSize)
                                .arg(dpr);
        if (const QPixmap *cached = m_cache.object(key)) {
            if (cached->isNull())
                return QFileIconProvider::icon(info);
            return QIcon(*cached);
        }

        const QIcon dirIcon = QFileIconProvider::icon(info);
        const QPixmap dirPixmap = dirIcon.pixmap(QSize(m_iconSize, m_iconSize));
        m_cache.insert(key, new QPixmap(dirPixmap), 1);
        return dirPixmap.isNull() ? dirIcon : QIcon(dirPixmap);
    }

    const QString key = QStringLiteral("%1|%2|%3|%4")
                            .arg(info.filePath())
                            .arg(info.lastModified().toMSecsSinceEpoch())
                            .arg(m_iconSize)
                            .arg(dpr);

    if (const QPixmap *cached = m_cache.object(key)) {
        // 之前已确认缩略图/大图标都取不到：直接回退，避免每次重绘都重试 Shell。
        if (cached->isNull())
            return QFileIconProvider::icon(info);
        return QIcon(*cached);
    }

    QPixmap thumbnail = loadImageThumbnail(info);
    if (thumbnail.isNull())
        thumbnail = loadShellThumbnail(info);
    // 缩略图不可用时用 Shell 大图标兜底，保证图标能铺满图标区域
    if (thumbnail.isNull())
        thumbnail = loadShellIcon(info);
    // 最后回退到文件类型图标，并把这个回退结果缓存起来
    if (thumbnail.isNull()) {
        const QIcon fallback = QFileIconProvider::icon(info);
        thumbnail = fallback.pixmap(QSize(m_iconSize, m_iconSize));
    }

    if (thumbnail.isNull()) {
        // 连文件类型图标都取不到：缓存空结果，避免每帧重复 QImageReader + Shell 调用
        m_cache.insert(key, new QPixmap(), 1);
        return QFileIconProvider::icon(info);
    }

    m_cache.insert(key, new QPixmap(thumbnail), 1);
    return QIcon(thumbnail);
}

QPixmap ThumbnailIconProvider::loadImageThumbnail(const QFileInfo &info) const
{
    static const QStringList imageSuffixes = { "jpg", "jpeg", "png", "bmp", "gif", "svg" };
    if (!imageSuffixes.contains(info.suffix().toLower()))
        return QPixmap();

    QImageReader reader(info.filePath());
    reader.setAutoTransform(true);
    if (!reader.canRead())
        return QPixmap();

    const qreal dpr = currentDevicePixelRatio();
    const int targetPx = qMax(1, qRound(m_iconSize * dpr));

    // 以目标尺寸直接解码，避免先把整张大图载入内存再缩放
    const QSize size = reader.size();
    if (size.isValid() && !size.isNull()) {
        const int maxDim = qMax(size.width(), size.height());
        if (maxDim > targetPx) {
            const qreal scale = qreal(targetPx) / maxDim;
            reader.setScaledSize(QSize(qRound(size.width() * scale), qRound(size.height() * scale)));
        }
    }

    const QImage image = reader.read();
    if (image.isNull())
        return QPixmap();

    QPixmap pixmap = QPixmap::fromImage(
        image.scaled(targetPx, targetPx, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    if (pixmap.isNull())
        return QPixmap();

    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

#ifdef Q_OS_WIN
// 通过 Shell 取位图（缩略图或大图标），失败返回空
static QPixmap loadShellImage(const QString &filePath, int targetPx, DWORD flags)
{
    ensureComInitialized();

    IShellItemImageFactory *factory = nullptr;
    const HRESULT hrCreate = SHCreateItemFromParsingName(
        filePath.toStdWString().c_str(), nullptr,
        FM_IID_IShellItemImageFactory, reinterpret_cast<void **>(&factory));
    if (FAILED(hrCreate) || factory == nullptr)
        return QPixmap();

    SIZE size;
    size.cx = targetPx;
    size.cy = targetPx;

    HBITMAP hBitmap = nullptr;
    const HRESULT hrImage = factory->GetImage(size, flags, &hBitmap);
    factory->Release();

    if (FAILED(hrImage) || hBitmap == nullptr)
        return QPixmap();

    const QImage image = imageFromHBITMAP(hBitmap);
    DeleteObject(hBitmap);
    if (image.isNull())
        return QPixmap();

    return QPixmap::fromImage(
        image.scaled(targetPx, targetPx, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QPixmap ThumbnailIconProvider::loadShellThumbnail(const QFileInfo &info) const
{
    const qreal dpr = currentDevicePixelRatio();
    const int targetPx = qMax(1, qRound(m_iconSize * dpr));

    // 只要真正的缩略图（没有就失败），不要系统图标放大图
    QPixmap pixmap = loadShellImage(info.filePath(), targetPx,
                                    SIIGBF_THUMBNAILONLY | SIIGBF_RESIZETOFIT);
    if (!pixmap.isNull())
        pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

// 缩略图不可用时取 Shell 大图标：ICONONLY 能拿到 128/256 级别的大图标，
// 比 QFileIconProvider 默认的小图标清晰，也能让图标铺满缩略图区域。
QPixmap ThumbnailIconProvider::loadShellIcon(const QFileInfo &info) const
{
    const qreal dpr = currentDevicePixelRatio();
    const int targetPx = qMax(1, qRound(m_iconSize * dpr));

    QPixmap pixmap = loadShellImage(info.filePath(), targetPx, SIIGBF_ICONONLY);
    if (!pixmap.isNull())
        pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}
#else
QPixmap ThumbnailIconProvider::loadShellThumbnail(const QFileInfo &) const
{
    return QPixmap();
}

QPixmap ThumbnailIconProvider::loadShellIcon(const QFileInfo &) const
{
    return QPixmap();
}
#endif
