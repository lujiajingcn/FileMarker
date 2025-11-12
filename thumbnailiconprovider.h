#ifndef THUMBNAILICONPROVIDER_H
#define THUMBNAILICONPROVIDER_H

#include <QFileIconProvider>

// 自定义图标提供器，用于生成图片的缩略图
class ThumbnailIconProvider : public QFileIconProvider
{
public:
    ThumbnailIconProvider();
    QIcon icon(const QFileInfo &info) const override;
};
#endif // THUMBNAILICONPROVIDER_H
