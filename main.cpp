#include "mainwindow.h"

#include <QApplication>
#include <QByteArray>
#include <QString>

#include <cstdio>
#include <cstdlib>

namespace {

/**
 * 启动早期的消息处理器。
 *
 * 目的：Qt 自带的 PNG 解码器会把 libpng 的告警通过 qWarning 打到 stderr，
 * 常见的如 "libpng warning: iCCP: known incorrect sRGB profile" ——
 * 这类问题只影响图片自带的 ICC 描述，不影响显示，属于第三方图片的元数据瑕疵，
 * 每次启动/生成缩略图都刷一行没有意义，这里直接吞掉。
 *
 * 其余消息保持原来的行为（写到 stderr），等 MainWindow 装好日志系统后由它接管。
 */
void earlyMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &sMsg)
{
    if (sMsg.startsWith(QLatin1String("libpng warning:")))
        return;

    fprintf(stderr, "%s\n", sMsg.toLocal8Bit().constData());
    if (type == QtFatalMsg)
        abort();
}

}   // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // 开启高 DPI 缩放：缩略图/行高等尺寸都按逻辑像素书写，
    // 不开启时在 150%/200% 缩放的屏幕上图标与文字的比例会失衡。
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    // 在创建 QApplication 之前接管消息：Qt 初始化阶段就可能解码图片
    qInstallMessageHandler(earlyMessageHandler);

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
