#include "thumbnailiconprovider.h"

#include <QDateTime>
#include <functional>
#include <QRunnable>
#include <QMetaObject>
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

// 每个线程初始化一次 COM。注意不能用函数内的 static —— 那只会在"第一个调用它的线程"
// 里初始化，后台线程拿到 static 的 true 就跳过了，Shell 调用会失败。
static void ensureComInitialized()
{
    thread_local bool s_initialized = false;
    if (!s_initialized) {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        s_initialized = true;   // 失败（例如已处于 MTA）也不再重试，后续自动回退到类型图标
        Q_UNUSED(hr);
    }
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
    , m_paintDpr(0.0)
{
    m_cache.setMaxCost(256);
}

ThumbnailIconProvider::~ThumbnailIconProvider()
{
    // 后台线程里还在用 this 算缩略图：必须等它们退出再删线程池，
    // 否则就是典型的退出时偶发崩溃（还在跑的任务访问已析构的对象）。
    if (m_pool != nullptr) {
        m_pool->clear();
        m_pool->waitForDone();
        delete m_pool;
        m_pool = nullptr;
    }
}

void ThumbnailIconProvider::setPaintDevicePixelRatio(qreal dpr) const
{
    if (dpr > 0.0)
        m_paintDpr = dpr;
}

qreal ThumbnailIconProvider::effectiveDpr() const
{
    return m_paintDpr > 0.0 ? m_paintDpr : currentDevicePixelRatio();
}

void ThumbnailIconProvider::setIconSize(int iconSize)
{
    if (iconSize <= 0 || iconSize == m_iconSize)
        return;

    m_iconSize = iconSize;
    m_cache.clear();
    m_failureTime.clear();
}


#ifdef Q_OS_WIN
/** 通过 Shell 取一张位图（缩略图或大图标），返回 QImage；失败返回空。
 *  注意：可以在后台线程调用，内部按线程初始化 COM。 */
static QImage loadShellImage(const QString &filePath, int targetPx, DWORD flags)
{
    ensureComInitialized();

    IShellItemImageFactory *factory = nullptr;
    const HRESULT hrCreate = SHCreateItemFromParsingName(
        filePath.toStdWString().c_str(), nullptr,
        FM_IID_IShellItemImageFactory, reinterpret_cast<void **>(&factory));
    if (FAILED(hrCreate) || factory == nullptr)
        return QImage();

    SIZE size;
    size.cx = targetPx;
    size.cy = targetPx;

    HBITMAP hBitmap = nullptr;
    const HRESULT hrImage = factory->GetImage(size, flags, &hBitmap);
    factory->Release();

    if (FAILED(hrImage) || hBitmap == nullptr)
        return QImage();

    const QImage image = imageFromHBITMAP(hBitmap);
    DeleteObject(hBitmap);
    if (image.isNull())
        return QImage();

    return image.scaled(targetPx, targetPx, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
#else
static QImage loadShellImage(const QString &, int, unsigned long)
{
    return QImage();
}
#endif

QIcon ThumbnailIconProvider::icon(const QFileInfo &info) const
{
    const qreal dpr = effectiveDpr();
    const qint64 nNow = QDateTime::currentMSecsSinceEpoch();
    const qint64 nFailureTtlMs = 30000;

    auto failureIsFresh = [this, nNow, nFailureTtlMs](const QString &key) {
        const auto it = m_failureTime.constFind(key);
        return it != m_failureTime.constEnd() && (nNow - it.value()) < nFailureTtlMs;
    };

    if (info.isDir()) {
        // 文件夹也缓存：滚动/悬停会反复取图标，QFileIconProvider 每次都要查 Shell。
        const QString key = QStringLiteral("dir|%1|%2|%3")
                                .arg(info.filePath())
                                .arg(m_iconSize)
                                .arg(dpr);
        if (const QPixmap *cached = m_cache.object(key)) {
            if (!cached->isNull())
                return QIcon(*cached);
            if (failureIsFresh(key))
                return typeIconFor(info);
            m_cache.remove(key);
            m_failureTime.remove(key);
        }

        const QIcon dirIcon = QFileIconProvider::icon(info);
        const QPixmap dirPixmap = dirIcon.pixmap(QSize(m_iconSize, m_iconSize));
        if (dirPixmap.isNull()) {
            m_cache.insert(key, new QPixmap(), 1);
            m_failureTime.insert(key, nNow);
            return dirIcon;
        }
        m_cache.insert(key, new QPixmap(dirPixmap), 1);
        return QIcon(dirPixmap);
    }

    const QString key = QStringLiteral("%1|%2|%3|%4")
                            .arg(info.filePath())
                            .arg(info.lastModified().toMSecsSinceEpoch())
                            .arg(m_iconSize)
                            .arg(dpr);

    if (const QPixmap *cached = m_cache.object(key)) {
        if (!cached->isNull())
            return QIcon(*cached);
        if (!failureIsFresh(key)) {   // 失败缓存过期：允许再试一次
            m_cache.remove(key);
            m_failureTime.remove(key);
        }
    }

    // 缓存没有就排队去后台算，本次先给"文件类型图标"占位，界面不会被卡住。
    requestAsyncThumbnail(info, key, m_iconSize, dpr);
    return typeIconFor(info);
}

/** 占位图标：按后缀缓存，每种类型最多同步查一次 Shell。 */
QIcon ThumbnailIconProvider::typeIconFor(const QFileInfo &info) const
{
    const QString sKey = info.isDir() ? QStringLiteral("<dir>") : info.suffix().toLower();
    const auto it = m_typeIcons.constFind(sKey);
    if (it != m_typeIcons.constEnd())
        return it.value();

    const QIcon icon = QFileIconProvider::icon(info);
    m_typeIcons.insert(sKey, icon);
    return icon;
}

/** 后台线程里算一张缩略图：只产出 QImage，不创建任何 QPixmap / GUI 对象。 */
QImage ThumbnailIconProvider::computeThumbnail(const QString &sFilePath, int iconSize,
                                               qreal dpr) const
{
    const int targetPx = qMax(1, qRound(iconSize * dpr));

    // 1) 图片文件：直接按目标尺寸解码（QImageReader 是线程安全的，各线程读各自的文件）
    static const QStringList imageSuffixes = {"jpg", "jpeg", "png", "bmp", "gif", "svg"};
    const QString sSuffix = QFileInfo(sFilePath).suffix().toLower();
    if (imageSuffixes.contains(sSuffix)) {
        QImageReader reader(sFilePath);
        reader.setAutoTransform(true);
        if (reader.canRead()) {
            const QSize size = reader.size();
            if (size.isValid() && !size.isNull()) {
                const int maxDim = qMax(size.width(), size.height());
                if (maxDim > targetPx) {
                    const qreal scale = qreal(targetPx) / maxDim;
                    reader.setScaledSize(QSize(qRound(size.width() * scale),
                                               qRound(size.height() * scale)));
                }
            }
            const QImage image = reader.read();
            if (!image.isNull()) {
                return image.scaled(targetPx, targetPx, Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation);
            }
        }
    }

#ifdef Q_OS_WIN
    // 2) 其他类型：Windows Shell 缩略图（视频、PDF、Office…）
    QImage image = loadShellImage(sFilePath, targetPx,
                                  SIIGBF_THUMBNAILONLY | SIIGBF_RESIZETOFIT);
    if (!image.isNull())
        return image;

    // 3) 再不行就取 Shell 大图标（比类型小图标清晰，能铺满缩略图区域）
    return loadShellImage(sFilePath, targetPx, SIIGBF_ICONONLY);
#else
    Q_UNUSED(dpr);
    return QImage();
#endif
}

void ThumbnailIconProvider::requestAsyncThumbnail(const QFileInfo &info, const QString &key,
                                                  int iconSize, qreal dpr) const
{
    if (m_pending.contains(key))
        return;   // 已经在算
    m_pending.insert(key);

    if (m_pool == nullptr) {
        m_pool = new QThreadPool;
        m_pool->setMaxThreadCount(2);   // 解码是 CPU/IO 型，2 个后台线程足够
    }

    const QString sFilePath = info.filePath();
    QObject *context = m_context;
    const ThumbnailIconProvider *self = this;

    class ThumbnailTask : public QRunnable
    {
    public:
        ThumbnailTask(const ThumbnailIconProvider *provider, QObject *context,
                      const QString &filePath, const QString &key, int iconSize, qreal dpr)
            : m_provider(provider), m_context(context), m_filePath(filePath),
              m_key(key), m_iconSize(iconSize), m_dpr(dpr) {}

        void run() override
        {
            const QImage image = m_provider->computeThumbnail(m_filePath, m_iconSize, m_dpr);
            if (m_context == nullptr)
                return;
            // 回到 UI 线程再做 QPixmap / 缓存 / 通知；context 先销毁时这个调用会被丢弃
            const ThumbnailIconProvider *provider = m_provider;
            const QString sFilePath = m_filePath;
            const QString sKey = m_key;
            QMetaObject::invokeMethod(m_context, [provider, sFilePath, sKey, image]() {
                provider->applyAsyncResult(sFilePath, sKey, image);
            }, Qt::QueuedConnection);
        }

    private:
        const ThumbnailIconProvider *m_provider;
        QObject *m_context;
        QString m_filePath;
        QString m_key;
        int m_iconSize;
        qreal m_dpr;
    };

    m_pool->start(new ThumbnailTask(self, context, sFilePath, key, iconSize, dpr));
}

/** UI 线程：把后台算好的图转成 QPixmap 放进缓存，并通知视图重绘。 */
void ThumbnailIconProvider::applyAsyncResult(const QString &sFilePath, const QString &key,
                                             const QImage &image) const
{
    m_pending.remove(key);

    if (image.isNull()) {
        m_cache.insert(key, new QPixmap(), 1);   // 失败也记一笔，TTL 内不再重复请求
        m_failureTime.insert(key, QDateTime::currentMSecsSinceEpoch());
        return;
    }

    const int nBar = key.lastIndexOf(QLatin1Char('|'));
    const qreal dpr = nBar >= 0 ? key.mid(nBar + 1).toDouble() : 1.0;

    auto *pixmap = new QPixmap(QPixmap::fromImage(image));
    pixmap->setDevicePixelRatio(dpr > 0.0 ? dpr : 1.0);
    m_cache.insert(key, pixmap, 1);
    m_failureTime.remove(key);

    if (m_readyCallback)
        m_readyCallback(sFilePath);
}
