#include "mainwindow.h"

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
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
    a.setApplicationName(QStringLiteral("FileMarker"));
    a.setApplicationVersion(QStringLiteral("1.0.0"));

    // ---- 单实例保护 ----
    // labels.xml 是在退出时才整体写一遍的，两个实例同时开着，后关掉的会把
    // 先关掉那次的标签库编辑整体覆盖掉；sqlite 缓存也会互相抢锁。
    // 所以第二次启动不去开新窗口，而是把已有窗口叫到前台。
    //
    // 用 QLockFile 而不是 QLocalServer 本身来判重：Windows 的命名管道支持多个
    // 同名实例，QLocalServer::listen 在 Windows 上第二次也能成功，靠它判重不可靠。
    // 锁文件放临时目录，进程异常退出时 QLockFile 会按 PID 判断成失效锁并接管。
    const QString sLockPath = QDir(QDir::tempPath()).filePath(QStringLiteral("FileMarker.lock"));
    QLockFile singleInstanceLock(sLockPath);
    singleInstanceLock.setStaleLockTime(30000);

    if (!singleInstanceLock.tryLock(200)) {
        // 已有实例在跑：尽量把它叫到前台，然后自己退出
        const QString sServerName = QStringLiteral("FileMarker.SingleInstance");
        QLocalSocket socket;
        socket.connectToServer(sServerName);
        if (socket.waitForConnected(500)) {
            socket.write("raise");
            socket.flush();
            socket.waitForBytesWritten(500);
            socket.disconnectFromServer();
        }
        return 0;
    }

    // 锁拿到了：再起一个本地 socket 服务，用来接收"把窗口叫出来"的请求
    const QString sServerName = QStringLiteral("FileMarker.SingleInstance");
    QLocalServer singleInstanceServer;
    QLocalServer::removeServer(sServerName);   // 清掉上次异常退出的残留
    if (!singleInstanceServer.listen(sServerName)) {
        qWarning() << "单实例通知服务创建失败（不影响程序运行）:" << singleInstanceServer.errorString();
    }

    MainWindow w;
    QObject::connect(&singleInstanceServer, &QLocalServer::newConnection, &w, [&w, &singleInstanceServer]() {
        while (QLocalSocket *pSocket = singleInstanceServer.nextPendingConnection()) {
            pSocket->readAll();
            pSocket->disconnectFromServer();
            pSocket->deleteLater();
        }
        // 第二个实例被拦下了，给用户一点反馈，避免"双击没反应"的错觉
        w.showNormal();
        w.raise();
        w.activateWindow();
        w.showHintMessage(QObject::tr("FileMarker 已经在运行，已切换到现有窗口。"));
    });

    w.show();

    return a.exec();
}
