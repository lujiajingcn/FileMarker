#include "thumbnailiconprovider.h"

ThumbnailIconProvider::ThumbnailIconProvider()
{

}

QIcon ThumbnailIconProvider::icon(const QFileInfo &info) const
{
    // 检查文件是否为图片格式
    QString suffix = info.suffix().toLower();
    if (suffix == "jpg" || suffix == "jpeg" || suffix == "png" ||
        suffix == "bmp" || suffix == "gif" || suffix == "svg") {

        // 获取原始图片
        QPixmap pixmap(info.filePath());
        if(pixmap.isNull()) {
            // 如果加载失败，返回默认图标
            return QFileIconProvider::icon(info);
        }

        // 生成缩略图，这里缩放到128x128，保持宽高比
        QPixmap thumbnail = pixmap.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // 创建带缩略图的图标
        return QIcon(thumbnail);
    }

    // 对于非图片文件或目录，返回系统默认图标
    return QFileIconProvider::icon(info);
}
