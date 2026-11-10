#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "dlgauthor.h"
#include "utility.h"
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QScreen>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QDirIterator>
#include <QProgressDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>
#include <QPushButton>
#include "adsoperation.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_geometryTimer(nullptr),
    ui(new Ui::MainWindow)
{
    g_sAppDir = QCoreApplication::applicationDirPath();

    // 日志要在 setupUi 之前装好：标签库（FormLabels）是在 setupUi 里构造的，
    // 它读 labels.xml 失败时的 qCritical 必须能落进日志，否则用户只看到"标签库空了"。
    initLogMessageHanlder();

    ui->setupUi(this);

    // 窗口移动/缩放后防抖保存，避免崩溃或强杀时丢失窗口状态
    m_geometryTimer = new QTimer(this);
    m_geometryTimer->setSingleShot(true);
    m_geometryTimer->setInterval(500);
    connect(m_geometryTimer, &QTimer::timeout, this, &MainWindow::saveWindowGeometry);

    // 保证默认窗口下详情列表的“标签”列也能完整显示（列宽策略见 FormPageFiles）。
    setMinimumSize(1000, 650);
    // 默认开大一点：994x663 时名称列会被压得很窄；不超过屏幕可用区域。
    const QRect rcAvail = QApplication::primaryScreen()->availableGeometry();
    resize(qMin(1280, rcAvail.width() - 80), qMin(820, rcAvail.height() - 80));

    // 恢复上次的窗口大小/位置；恢复上次访问的目录（等窗口显示后执行，保证模型已就绪）
    {
        QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        settings.setIniCodec("UTF-8");
#endif
        const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
        if (!geometry.isEmpty())
            restoreGeometry(geometry);
    }
    QTimer::singleShot(0, this, [this]() {
        ui->wFileBrowser->restoreLastDir();
    });

    // 显式连接菜单动作（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(ui->actionHelp, &QAction::triggered, this, &MainWindow::onActionHelpTriggered);
    connect(ui->actionAuthor, &QAction::triggered, this, &MainWindow::onActionAuthorTriggered);

    // 菜单栏只剩【文件】里的退出项（方案A：文件命令已下沉到文件浏览页的命令条）
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);

    // 【文件】菜单：标签导出/导入。
    // 标签存在 NTFS 数据流里，文件拷到 U 盘(FAT32)/压缩包/网盘时会被丢掉，导出是唯一的兜底手段。
    QAction *actionExportTags = new QAction(tr("导出标签…"), this);
    ui->menuFileManage->insertAction(ui->actionExit, actionExportTags);
    connect(actionExportTags, &QAction::triggered, ui->wFileBrowser, &FormFileBrowser::exportTagsToFile);

    QAction *actionImportTags = new QAction(tr("导入标签…"), this);
    ui->menuFileManage->insertAction(ui->actionExit, actionImportTags);
    connect(actionImportTags, &QAction::triggered, ui->wFileBrowser, &FormFileBrowser::importTagsFromFile);

    ui->menuFileManage->insertSeparator(ui->actionExit);

    // 清理未使用标签：扫描选中目录（含子目录），把没有任何文件使用的库标签删掉
    QAction *actionCleanupLabels = new QAction(tr("清理未使用标签…"), this);
    ui->dwcLabels->addContextMenuAction(actionCleanupLabels);
    connect(actionCleanupLabels, &QAction::triggered, this, [this]() {
        const QStringList qLDirs = ui->wFileBrowser->getSelDirs();
        if (qLDirs.isEmpty())
            return;

        QSet<QString> setUsedLower;
        QProgressDialog progress(tr("正在扫描文件标签…"), tr("取消"), 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.setMinimumDuration(0);

        int nScanned = 0;
        bool bCanceled = false;
        for (const QString &sDir : qLDirs) {
            QDirIterator it(sDir, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString sFilePath = it.next();
                if ((++nScanned % 100) == 0) {
                    progress.setLabelText(tr("正在扫描文件标签…\n%1").arg(sFilePath));
                    QCoreApplication::processEvents();
                    if (progress.wasCanceled()) {
                        bCanceled = true;
                        break;
                    }
                }
                const QStringList qLTags = ADSOperation::listADSFileName(sFilePath);
                for (const QString &sTag : qLTags)
                    setUsedLower.insert(sTag.toLower());
            }
            if (bCanceled)
                break;
        }
        progress.close();

        if (bCanceled)
            return;

        QStringList qLUnused;
        const QStringList qLLibrary = ui->dwcLabels->allLabels();
        for (const QString &sLabel : qLLibrary) {
            if (!setUsedLower.contains(sLabel.toLower()) && !qLUnused.contains(sLabel))
                qLUnused << sLabel;
        }

        if (qLUnused.isEmpty()) {
            QMessageBox::information(this, tr("清理未使用标签"),
                                     tr("扫描了 %1 个文件，没有发现未使用的标签。").arg(nScanned));
            return;
        }

        QString sPreview = qLUnused.mid(0, 30).join("、");
        if (qLUnused.size() > 30)
            sPreview += tr("…等 %1 个").arg(qLUnused.size());

        QMessageBox box(this);
        box.setWindowTitle(tr("清理未使用标签"));
        box.setIcon(QMessageBox::Warning);
        box.setText(tr("扫描了 %1 个文件，以下 %2 个标签没有被任何文件使用：\n\n%3\n\n"
                       "从标签库删除后不可撤销，是否继续？")
                        .arg(nScanned).arg(qLUnused.size()).arg(sPreview));
        QPushButton *pDelete = box.addButton(tr("删除"), QMessageBox::DestructiveRole);
        QPushButton *pCancel = box.addButton(tr("取消"), QMessageBox::RejectRole);
        box.setDefaultButton(pCancel);
        box.exec();
        if (box.clickedButton() == pDelete)
            ui->dwcLabels->removeLabels(qLUnused);
    });

    // 撤销上一次标签操作：标签写盘不可撤销，至少要能给"刚才那一步"兜底
    QAction *actionUndoTag = ui->menuFileManage->addAction(tr("撤销上一次标签操作"));
    actionUndoTag->setShortcut(QKeySequence::Undo);
    actionUndoTag->setEnabled(false);
    connect(actionUndoTag, &QAction::triggered, ui->wFileBrowser, &FormFileBrowser::undoLastTagOperation);
    connect(ui->wFileBrowser, &FormFileBrowser::sendUndoStateChanged, actionUndoTag, &QAction::setEnabled);

    // AI 打标依赖外部 Python 脚本与依赖库，给个自检入口，免得只能看到运行失败
    QAction *actionCheckAI = ui->menuHelp->addAction(tr("检查 AI 打标环境…"));
    connect(actionCheckAI, &QAction::triggered, this, &MainWindow::checkAIEnvironment);

    // 把文件命令（AI/添加/清空/查找/遍历/查找条件）交给文件浏览页，由它在命令条上展示
    ui->wFileBrowser->setActions(ui->actionAddLabelbyAI,
                                 ui->actionAddLabels,
                                 ui->actionDelLabels,
                                 ui->actionSearchFilesbyLabels,
                                 ui->actionTraverseSelDirs);

    // 【标签库】中选中的标签，同步给文件浏览页（用于添加标签/查找）
    connect(ui->dwcLabels, &FormLabels::sendSelLabels, ui->wFileBrowser, &FormFileBrowser::onRecvSelLabels);

    // search treats a selected parent label as "this label + all of its children".
    // Must be connected AFTER the line above (which resets groups to singletons).
    connect(ui->dwcLabels, &FormLabels::sendSelLabels, this, [this](QStringList) {
        ui->wFileBrowser->onRecvLabelGroups(ui->dwcLabels->getSelLabelGroups());
    });

    // give the label library the cache DB so rename/delete can report
    // how many files use the label and optionally sync the change to files.
    ui->dwcLabels->setSqlOperation(ui->wFileBrowser->sqlOperation());

    // 【当前目录中的标签】里选中的标签，同样同步给文件浏览页
    connect(ui->dwcCurDirLabels, &FormCurDirLabels::sendSelLabels, ui->wFileBrowser, &FormFileBrowser::onRecvSelLabels);

    // 遍历选中文件夹后，把"目录 → 标签集合"的结果交给【当前目录中的标签】面板
    connect(ui->wFileBrowser, &FormFileBrowser::sendDirAndLabels, ui->dwcCurDirLabels, &FormCurDirLabels::onRecvDirAndLabels);

    // 切换目录时先清空【当前目录标签】，再显示新目录的标签
    connect(ui->wFileBrowser, &FormFileBrowser::sendClearDirLabels, ui->dwcCurDirLabels, &FormCurDirLabels::clearLabels);

    // 搜索/遍历等耗时操作的文字反馈（状态栏）
    connect(ui->wFileBrowser, &FormFileBrowser::sendStatusMessage, this, [this](const QString &sMsg) {
        statusBar()->showMessage(sMsg, 5000);
    });

    // 文件的标签被编辑后（对话框/内联编辑），把结果回传给【标签库】，触发"是否加入标签库"的询问
    // 标签串由英文逗号分隔，形如 照片,故宫,2025
    // todo 这里用信号一路传到标签库，路径偏长，后续可以改成直接调用或事件总线
    connect(ui->wFileBrowser, &FormFileBrowser::sendLabels, ui->dwcLabels, &FormLabels::onRecvLabels);

    // AI 根据文件信息自动生成的标签，交给【标签库】归到"AI自动生成标签"分组下
    connect(ui->wFileBrowser, &FormFileBrowser::sendLabelsGeneratedByAI, ui->dwcLabels, &FormLabels::onRecvLabelsGeneratedByAI);
}

MainWindow::~MainWindow()
{
    // 保存窗口几何信息，下次启动恢复
    saveWindowGeometry();

    // 先销毁界面（析构过程中产生的日志仍写入文件），再把日志落盘并断开设备。
    // 最后恢复默认 handler：否则窗口销毁后 Qt 收尾阶段的日志会写进已关闭的
    // 文件，出现 "QIODevice::write (QObject): device not open"。
    delete ui;

    // 在锁内 flush 并摘掉日志设备，避免与工作线程日志竞争
    Utility::shutdownLog();
    qInstallMessageHandler(nullptr);
}

void MainWindow::saveWindowGeometry()
{
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    if (m_geometryTimer != nullptr)
        m_geometryTimer->start();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_geometryTimer != nullptr)
        m_geometryTimer->start();
}

void MainWindow::initLogMessageHanlder()
{
    m_logFile.setFileName(g_sAppDir + "/" + LOG_FILE);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // 例如程序装在 C:\Program Files 这类只读目录时打开会失败；
        // 此时不绑定日志设备，所有日志静默丢弃（不崩溃），仅给出警告。
        qWarning() << "无法打开日志文件，日志将不写入文件" << m_logFile.fileName();
        return;
    }
    Utility::tsLogInfo.setDevice(&m_logFile);
    qInstallMessageHandler(Utility::myMessageHandler);
}

// 参考了 Qt Creator 的 UI Tour 功能，原始代码为 https://github.com/qt-creator/qt-creator/blob/qds/4.5/src/plugins/welcome/introductionwidget.cpp
void MainWindow::showHintMessage(const QString &sMessage)
{
    statusBar()->showMessage(sMessage, 5000);
}

void MainWindow::onActionHelpTriggered()
{
    auto *wIntroduction = new IntroductionWidget(this);
    wIntroduction->show();
}

void MainWindow::onActionAuthorTriggered()
{
    DlgAuthor dlgAuthor(this);
    dlgAuthor.exec();
}

/** AI 打标环境自检：脚本 / Python / 依赖库 三项，逐项给结果。 */
void MainWindow::checkAIEnvironment()
{
    const QString sScript = g_sAppDir + "/extract_and_tag.py";
    QStringList qLLines;

    qLLines << (QFileInfo::exists(sScript)
                    ? tr("① 脚本：已找到\n   %1").arg(sScript)
                    : tr("① 脚本：未找到\n   期望位置：%1\n   请把 extract_and_tag.py 放到程序目录。").arg(sScript));

    QString sPython;
    QProcess proc;
    proc.start(QStringLiteral("python"),
               QStringList() << QStringLiteral("-c")
                             << QStringLiteral("import sys;print(sys.version.split()[0])"));
    if (proc.waitForFinished(5000) && proc.exitCode() == 0)
        sPython = QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();

    if (sPython.isEmpty()) {
        proc.start(QStringLiteral("python3"),
                   QStringList() << QStringLiteral("-c") << QStringLiteral("import sys;print(sys.version.split()[0])"));
        if (proc.waitForFinished(5000) && proc.exitCode() == 0)
            sPython = QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();
    }

    if (sPython.isEmpty()) {
        qLLines << tr("② Python：未找到可用的解释器（需要 python 或 python3 在 PATH 里）");
        qLLines << tr("③ 依赖库：无法检查（没有 Python）");
    } else {
        qLLines << tr("② Python：%1").arg(sPython);

        proc.start(QStringLiteral("python"),
                   QStringList() << QStringLiteral("-c")
                                 << QStringLiteral("import pypdf,docx,PIL,pytesseract,requests;print('ok')"));
        const bool bDepsOk = proc.waitForFinished(20000) && proc.exitCode() == 0;
        if (bDepsOk) {
            qLLines << tr("③ 依赖库：pypdf / python-docx / Pillow / pytesseract / requests 均已安装");
        } else {
            const QString sErr = QString::fromLocal8Bit(proc.readAllStandardError()).trimmed();
            qLLines << tr("③ 依赖库：缺少部分库\n   %1\n   可在命令行执行：pip install pypdf python-docx Pillow pytesseract requests")
                           .arg(sErr.isEmpty() ? tr("（没有拿到错误信息）") : sErr);
        }
        qLLines << tr("提示：AI 打标对每个文件启动一次脚本，结果只写进日志与进度页；"
                      "失败时会在结束时弹窗说明原因。");
    }

    QMessageBox::information(this, tr("AI 打标环境"), qLLines.join(QStringLiteral("\n\n")));
}

