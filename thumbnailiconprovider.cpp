#include "thumbnailiconprovider.h"

#include <QImageReader>
#include <QtMath>

ThumbnailIconProvider::ThumbnailIconProvider()
{

}

QIcon ThumbnailIconProvider::icon(const QFileInfo &info) const
{
    // 检查文件是否为图片格式
    QString suffix = info.suffix().toLower();
    if (suffix == "jpg" || suffix == "jpeg" || suffix == "png" ||
        suffix == "bmp" || suffix == "gif" || suffix == "svg") {

        // 以缩略图尺寸直接解码，避免先把整张大图载入内存再缩放（大图省内存、更快）。
        QImageReader reader(info.filePath());
        if (!reader.canRead()) {
            return QFileIconProvider::icon(info);
        }

        QSize sz = reader.size();
        if (sz.isValid() && !sz.isNull()) {
            int maxDim = qMax(sz.width(), sz.height());
            if (maxDim > 128) {
                qreal scale = 128.0 / maxDim;
                reader.setScaledSize(QSize(qRound(sz.width() * scale),
                                           qRound(sz.height() * scale)));
            }
        }

        QImage image = reader.read();
        if (image.isNull()) {
            // 解码失败，回退到系统默认图标
            return QFileIconProvider::icon(info);
        }

        // 生成缩略图，保持宽高比
        QImage thumbnail = image.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 创建带缩略图的图标
        return QIcon(QPixmap::fromImage(thumbnail));
    }

    // 对于非图片文件或目录，返回系统默认图标
    return QFileIconProvider::icon(info);
}
