#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // 开启高 DPI 缩放：缩略图/行高等尺寸都按逻辑像素书写，
    // 不开启时在 150%/200% 缩放的屏幕上图标与文字的比例会失衡。
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
