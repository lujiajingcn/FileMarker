#include <windows.h>

#include "traversedirectory.h"
#include "mainwindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QScreen>
#include <QSettings>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QLibraryInfo>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>
#include <QTranslator>

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

namespace {

/** 取 Windows 的"消息字体"（界面默认字体），按当前缩放比换算成像素后设为应用字体。
 *
 * 换算方式：lfHeight 是 96 DPI 下的逻辑高度（负数，例如 -12 = 9pt），
 * 想要的实际物理高度 = |lfHeight| × 系统缩放比，再除以 Qt 的 devicePixelRatio
 * （Qt 画像素时会再乘回 dpr）。这样 150% 屏上 12px 的字会画成 18px，和系统一致性最好。
 */
void applySystemUiFont()
{
#ifdef Q_OS_WIN
    NONCLIENTMETRICSW ncm;
    ZeroMemory(&ncm, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);
    if (!::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
        return;

    const QScreen *screen = QGuiApplication::primaryScreen();
    if (screen == nullptr)
        return;

    const qreal dpr = screen->devicePixelRatio();
    // 本进程是 PerMonitorV2，SPI_GETNONCLIENTMETRICS 返回的字号**已经按系统缩放比放大过**
    // （150% 的屏上是 18，而不是 96 DPI 下的 12）。所以这里只需要除回 Qt 的
    // devicePixelRatio：18 / 2 = 9 逻辑像素，画出来正好是 18 个物理像素。
    // 之前误用了 physicalDotsPerInch() —— 它由显示器自报的物理尺寸算出，很多屏不准
    // （这台机器算出来只有 81 DPI），结果字被再次缩小。
    const int scaledHeight = qAbs(int(ncm.lfMessageFont.lfHeight));

    QFont uiFont(QString::fromWCharArray(ncm.lfMessageFont.lfFaceName));
    const int px = qMax(8, qRound(scaledHeight / qMax<qreal>(1.0, dpr)));
    uiFont.setPixelSize(px);
    QApplication::setFont(uiFont);

#endif
}

/** 自动界面缩放比例：高分屏 + 系统还是 100% 缩放时，把界面放大到 125%。
 *
 * 没做这一步时，2560x1440 / 3840x2160 这类屏幕在 Windows 默认 100% 下，
 * 9pt 的系统字体会小到看不清 —— 用户换显示器后抱怨"字变小了"多半就是这种情形。
 */
double autoUiScale()
{
#ifdef Q_OS_WIN
    // 进程已经是 PerMonitorV2，GetDeviceCaps 拿到的是真实 DPI
    HDC screenDc = ::GetDC(nullptr);
    const int systemDpi = screenDc != nullptr ? ::GetDeviceCaps(screenDc, LOGPIXELSY) : 96;
    if (screenDc != nullptr) {
        ::ReleaseDC(nullptr, screenDc);
    }
    const int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);   // 物理像素
    const double systemScale = systemDpi > 0 ? systemDpi / 96.0 : 1.0;

    // 系统自己已经放大过了（125% 及以上），不要叠加
    if (systemScale > 1.05) {
        return 1.0;
    }
    // 屏幕够大：1080p 及以下保持原样，2K/4K 放大到 125%
    if (screenWidth >= 1920) {
        return 1.25;
    }
#endif
    return 1.0;
}

}   // namespace

int main(int argc, char *argv[])
{
    // 界面缩放：
    //  1) 用户在 exe 旁边的 FileMarker.ini 里写了 uiScale，就完全按它来；
    //  2) 没写则自动判断：Windows 缩放比还是 100%、但屏幕分辨率很高
    //     （笔记本/显示器常见的 2K、4K 屏）时，界面按 125% 画 —— 否则 9pt 系统字体
    //     在高分屏上会小到看不清，这也是"换显示器后字变小"的真正原因；
    //  3) 系统已经放大过（125%/150%/200%）就原样跟随，不再叠加。
    // 必须在 QApplication 之前设置 QT_SCALE_FACTOR，Qt 只在启动时读一次。
    {
        // 注意：这里 QApplication 还没建出来，applicationDirPath() 会返回空，
        // 只能从 argv[0] 推 exe 所在目录。
        const QString exeDir = QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath();
        QSettings uiIni(exeDir + QStringLiteral("/FileMarker.ini"), QSettings::IniFormat);
        double uiScale = uiIni.value(QStringLiteral("uiScale"), 0.0).toDouble();
        if (uiScale <= 0.5 || uiScale >= 4.0) {
            uiScale = autoUiScale();
        }
        if (uiScale > 1.01) {
            qputenv("QT_SCALE_FACTOR", QByteArray::number(uiScale, 'f', 2));
        }
    }

    // 在创建 QApplication 之前接管消息：Qt 初始化阶段就可能解码图片
    qInstallMessageHandler(earlyMessageHandler);

    QApplication a(argc, argv);

    // 目录进度快照要在工作线程和 UI 线程之间用 queued 连接传递，先注册类型
    qRegisterMetaType<DirProgressSnapshot>("DirProgressSnapshot");

    // 显式设置界面字体。
    // Qt 5 在 Windows 上会拿 SPI_GETNONCLIENTMETRICS 里的字号，再按**物理** DPI 折算成磅值；
    // 150% 缩放的 4K 屏上会算出 6pt（还退化成宋体），实际画出来只有 12 物理像素 ——
    // 这就是"换显示器后字变小"的根因。这里改成按像素设定，直接算成"系统字号 × 缩放比"。
    applySystemUiFont();

    a.setApplicationName(QStringLiteral("FileMarker"));
    a.setApplicationVersion(QStringLiteral("1.0.0"));

    // Qt 自带控件（文件对话框、消息框的标准按钮等）的中文文案要靠翻译文件。
    // Qt6 把控件文案拆成了 qtbase_zh_CN.qm（Qt5 时代是 qt_zh_CN.qm），两个都试一遍：
    // 先找 exe 旁边的 translations（打包版），再退回 Qt 安装目录。
    QTranslator qtTranslator;
    const QString sExeTr = a.applicationDirPath() + QStringLiteral("/translations/");
    const QString sQtTr = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    QStringList qLCandidates;
    for (const QString &sFile : {QStringLiteral("qtbase_zh_CN.qm"), QStringLiteral("qt_zh_CN.qm")}) {
        qLCandidates << sExeTr + sFile;
        if (!sQtTr.isEmpty())
            qLCandidates << sQtTr + QLatin1Char('/') + sFile;
    }
    bool bTranslatorLoaded = false;
    for (const QString &sQmPath : qLCandidates) {
        if (QFile::exists(sQmPath) && qtTranslator.load(sQmPath)) {
            a.installTranslator(&qtTranslator);
            bTranslatorLoaded = true;
            break;
        }
    }
    if (!bTranslatorLoaded)
        qWarning() << "未找到 Qt 中文翻译（qtbase_zh_CN.qm / qt_zh_CN.qm），部分系统按钮会显示英文";

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
        while (QLocalSocket *localSocket = singleInstanceServer.nextPendingConnection()) {
            localSocket->readAll();
            localSocket->disconnectFromServer();
            localSocket->deleteLater();
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
