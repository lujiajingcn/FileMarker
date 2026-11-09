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
    m_pGeometryTimer(nullptr),
    ui(new Ui::MainWindow)
{
    g_sAppDir = QCoreApplication::applicationDirPath();

    ui->setupUi(this);

    // 窗口移动/缩放后防抖保存，避免崩溃或强杀时丢失窗口状态
    m_pGeometryTimer = new QTimer(this);
    m_pGeometryTimer->setSingleShot(true);
    m_pGeometryTimer->setInterval(500);
    connect(m_pGeometryTimer, &QTimer::timeout, this, &MainWindow::saveWindowGeometry);

    // 淇濊瘉榛樿绐楀彛涓嬭鎯呭垪琛ㄧ殑鈥滄爣绛锯€濆垪涔熻兘瀹屾暣鏄剧ず锛堝垪瀹界瓥鐣ヨ FormPageFiles锛夈€?
    setMinimumSize(1000, 650);
    // 榛樿寮€澶т竴鐐癸細994x663 鏃跺悕绉板垪浼氳鍘嬪緱寰堢獎锛涗笉瓒呰繃灞忓箷鍙敤鍖哄煙銆?
    const QRect rcAvail = QApplication::primaryScreen()->availableGeometry();
    resize(qMin(1280, rcAvail.width() - 80), qMin(820, rcAvail.height() - 80));

    // 鎭㈠涓婃鐨勭獥鍙ｅぇ灏?浣嶇疆锛涙仮澶嶄笂娆¤闂殑鐩綍锛堢瓑绐楀彛鏄剧ず鍚庢墽琛岋紝淇濊瘉妯″瀷宸插氨缁級
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

    initLogMessageHanlder();

    // 鏄惧紡杩炴帴鑿滃崟鍔ㄤ綔锛堟鍓嶄緷璧?on_<瀵硅薄鍚?_<淇″彿鍚? 鐨勮嚜鍔ㄨ繛鎺ワ級
    connect(ui->actionHelp, &QAction::triggered, this, &MainWindow::onActionHelpTriggered);
    connect(ui->actionAuthor, &QAction::triggered, this, &MainWindow::onActionAuthorTriggered);

    // 灏嗐€愭爣绛剧鐞嗐€戠獥鍙ｇ殑宸ュ叿鏍忔搷浣滄坊鍔犲埌涓荤獥鍙ｇ殑鑿滃崟涓?
    ui->dwcLabels->addToolButtonsToMenu(ui->menuLabelManage);

    // 清理未使用标签：扫描选中目录（含子目录），把没有任何文件使用的库标签删掉
    QAction *actionCleanupLabels = ui->menuLabelManage->addAction(tr("清理未使用标签…"));    connect(actionCleanupLabels, &QAction::triggered, this, [this]() {
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

    // 【C16】撤销上一次标签操作：标签写盘不可撤销，至少要能给"刚才那一步"兜底
    QAction *actionUndoTag = ui->menuFileManage->addAction(tr("撤销上一次标签操作"));
    actionUndoTag->setShortcut(QKeySequence::Undo);
    actionUndoTag->setEnabled(false);
    connect(actionUndoTag, &QAction::triggered, ui->wFileBrowser, &FormFileBrowser::undoLastTagOperation);
    connect(ui->wFileBrowser, &FormFileBrowser::sendUndoStateChanged, actionUndoTag, &QAction::setEnabled);

    // 【C15】AI 打标依赖外部 Python 脚本与依赖库，给个自检入口，免得只能看到运行失败
    QAction *actionCheckAI = ui->menuHelp->addAction(tr("检查 AI 打标环境…"));
    connect(actionCheckAI, &QAction::triggered, this, &MainWindow::checkAIEnvironment);

    // 灏嗚彍鍗曟爮涓殑鍔ㄤ綔鏀惧埌銆愭枃浠剁鐞嗐€戜腑瀹炵幇
    ui->wFileBrowser->setActions(ui->actionAddLabelbyAI, ui->actionAddLabels, ui->actionDelLabels, ui->actionSearchFilesbyLabels, ui->actionTraverseSelDirs, ui->actionSearchConfig);

    // 銆愭爣绛剧鐞嗐€戜腑閫変腑鐨勬爣绛撅紝鍚屾鍒般€愭枃浠剁鐞嗐€戜腑
    connect(ui->dwcLabels, &FormLabels::sendSelLabels, ui->wFileBrowser, &FormFileBrowser::onRecvSelLabels);

    // A4: search treats a selected parent label as "this label + all of its children".
    // Must be connected AFTER the line above (which resets groups to singletons).
    connect(ui->dwcLabels, &FormLabels::sendSelLabels, this, [this](QStringList) {
        ui->wFileBrowser->onRecvLabelGroups(ui->dwcLabels->getSelLabelGroups());
    });

    // A2: give the label library the cache DB so rename/delete can report
    // how many files use the label and optionally sync the change to files.
    ui->dwcLabels->setSqlOperation(ui->wFileBrowser->sqlOperation());

    // 銆愬綋鍓嶇洰褰曟爣绛俱€戜腑閫変腑鐨勬爣绛撅紝鍚屾鍒般€愭枃浠剁鐞嗙被銆戜腑
    connect(ui->dwcCurDirLabels, &FormCurDirLabels::sendSelLabels, ui->wFileBrowser, &FormFileBrowser::onRecvSelLabels);

    // 閬嶅巻閫変腑鏂囦欢澶癸紝妫€绱㈠嚭鍏朵腑鏂囦欢鐨勬墍鏈夋爣绛撅紝鏄剧ず鍦ㄣ€愬綋鍓嶇洰褰曟爣绛俱€?
    connect(ui->wFileBrowser, &FormFileBrowser::sendDirAndLabels, ui->dwcCurDirLabels, &FormCurDirLabels::onRecvDirAndLabels);

    // 鍒囨崲鐩綍鏃跺厛娓呯┖銆愬綋鍓嶇洰褰曟爣绛俱€戯紝鍐嶆樉绀烘柊鐩綍鐨勬爣绛?
    connect(ui->wFileBrowser, &FormFileBrowser::sendClearDirLabels, ui->dwcCurDirLabels, &FormCurDirLabels::clearLabels);

    // 鎼滅储/閬嶅巻绛夎€楁椂鎿嶄綔鐨勬枃瀛楀弽棣堬紙鐘舵€佹爮锛?
    connect(ui->wFileBrowser, &FormFileBrowser::sendStatusMessage, this, [this](const QString &sMsg) {
        statusBar()->showMessage(sMsg, 5000);
    });

    // 鎵嬪姩缂栬緫銆愭爣绛俱€戝垪鍐呭鍚庯紝灏嗙紪杈戝悗鐨勫唴瀹逛紶閫掔粰銆愭爣绛剧鐞嗐€?
    // 缂栬緫鍚庣殑鏍囩鐢辫嫳鏂囬€楀彿鍒嗛殧锛屽舰濡?鐓х墖,鏁呭,2025
    // todo 浼犻€掕矾寰勮繃闀匡紝搴旇鏄粨鏋勪笉鍚堢悊锛屽悗缁敼杩?
    connect(ui->wFileBrowser, &FormFileBrowser::sendLabels, ui->dwcLabels, &FormLabels::onRecvLabels);

    // AI鏍规嵁鏂囦欢淇℃伅鑷姩鐢熸垚鐨勬爣绛撅紝娣诲姞鍒般€愭爣绛剧鐞嗐€戜腑鐨勨€淎I鑷姩鐢熸垚鏍囩鈥?
    connect(ui->wFileBrowser, &FormFileBrowser::sendLabelsGeneratedByAI, ui->dwcLabels, &FormLabels::onRecvLabelsGeneratedByAI);
}

MainWindow::~MainWindow()
{
    // 保存窗口几何信息，下次启动恢复
    saveWindowGeometry();

    // 鍏堥攢姣佺晫闈紙鏋愭瀯杩囩▼涓骇鐢熺殑鏃ュ織浠嶅啓鍏ユ枃浠讹級锛屽啀鎶婃棩蹇楄惤鐩樺苟鏂紑璁惧锛?
    // 鏈€鍚庢仮澶嶉粯璁?handler锛氬惁鍒欑獥鍙ｉ攢姣佸悗 Qt 鏀跺熬闃舵鐨勬棩蹇椾細鍐欒繘宸插叧闂殑
    // 鏂囦欢锛屽嚭鐜?"QIODevice::write (QObject): device not open"銆?
    delete ui;

    // 鍦ㄩ攣鍐?flush 骞舵憳鎺夋棩蹇楄澶囷紝閬垮厤涓庡伐浣滅嚎绋嬫棩蹇楃珵浜?
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
    if (m_pGeometryTimer != nullptr)
        m_pGeometryTimer->start();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_pGeometryTimer != nullptr)
        m_pGeometryTimer->start();
}

void MainWindow::initLogMessageHanlder()
{
    m_logFile.setFileName(g_sAppDir + "/" + LOG_FILE);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // 渚嬪绋嬪簭瑁呭湪 C:\Program Files 杩欑被鍙鐩綍鏃舵墦寮€浼氬け璐ワ紱
        // 姝ゆ椂涓嶇粦瀹氭棩蹇楄澶囷紝鎵€鏈夋棩蹇楅潤榛樹涪寮冿紙涓嶅穿婧冿級锛屼粎缁欏嚭璀﹀憡銆?
        qWarning() << "鏃犳硶鎵撳紑鏃ュ織鏂囦欢锛屾棩蹇楀皢涓嶅啓鍏ユ枃浠?" << m_logFile.fileName();
        return;
    }
    Utility::tsLogInfo.setDevice(&m_logFile);
    qInstallMessageHandler(Utility::myMessageHandler);
}

// 鍙傝€冧簡qt creator鐨?UI Tour鍔熻兘锛屽師濮嬩唬鐮佷负 https://github.com/qt-creator/qt-creator/blob/qds/4.5/src/plugins/welcome/introductionwidget.cpp
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

/** 【C15】AI 打标环境自检：脚本 / Python / 依赖库 三项，逐项给结果。 */
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

