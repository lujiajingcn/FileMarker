#include "formfilebrowser.h"
#include "ui_formfilebrowser.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QActionGroup>
#include <QMenu>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QDebug>
#include <QSettings>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>


#include "adsoperation.h"
#include "configoperation.h"
#include "tagexport.h"
#include "utility.h"

namespace {
// 左侧目录树收起后留下的窄条宽度（正好放下那个收起/展开按钮）
const int kNavRailWidth = 36;
// 展开时目录树的最小宽度，避免被 splitter 压成一条缝
const int kNavExpandedMinWidth = 160;

/**
 * 目录树"收起/展开"按钮的图标：一个圆角矩形 + 左侧分栏线，
 * 与 DeepSeek Harness 左侧栏的折叠图标同款。
 * 直接画出来，不依赖外部图片资源；按 2 倍分辨率绘制，高分屏下也清晰。
 */
QIcon makePanelToggleIcon(const QColor &color, int nSize = 20)
{
    const int nScale = 2;
    QPixmap pixmap(nSize * nScale, nSize * nScale);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(nScale);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    // 注意：pixmap 已经设了 devicePixelRatio，QPainter 会自动按逻辑像素绘制，
    // 这里不能再 scale()，否则图形会被放大一倍后裁掉右下角。

    const qreal dMargin = 1.4;
    const QRectF rcPanel(dMargin + 0.7, dMargin + 1.2,
                         nSize - 2 * dMargin - 1.4, nSize - 2 * dMargin - 2.4);

    QPen pen(color);
    pen.setWidthF(1.5);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rcPanel, 2.4, 2.4);

    // 左侧分栏：比外框略粗一点，缩小后仍然能看出"侧边栏"的形状
    QPen penDivider(color);
    penDivider.setWidthF(2.0);
    painter.setPen(penDivider);
    const qreal xDivider = rcPanel.left() + rcPanel.width() * 0.34;
    painter.drawLine(QPointF(xDivider, rcPanel.top() + 0.5),
                     QPointF(xDivider, rcPanel.bottom() - 0.5));

    painter.end();
    return QIcon(pixmap);
}
}

FormFileBrowser::FormFileBrowser(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormFileBrowser)
{
    ui->setupUi(this);

    setWindowStyleSheet();

    // 左侧【文件导航】面板：顶部加一个"收起/展开"按钮，可以像侧边栏一样隐藏起来
    initNavPanelToggle();

    m_sqlOperation = new SqliteOperation;
    m_threadSearch = nullptr;
    m_threadTraverseDirs = nullptr;
    m_threadAddLabelByAI = nullptr;
    m_nThread = -1;  // 初始化为非法值，防止 onRecvStopSearch 提前触发时 switch 走入意外分支

    // 点击左侧【文件导航】中节点，将该节点下的文件夹及文件显示在右侧【文件列表】
    connect(ui->wFileNavigation, &FormFileNavigation::sendNavigationClicked, this, &FormFileBrowser::onTvNavigationClicked);

    // 点击左侧【文件导航】中节点时，无论右侧显示的是【查询结果】界面还是正在【查询过程】界面（触发停止查询操作），都统一跳转到【文件列表】界面
    connect(this, &FormFileBrowser::sendNavigationClicked, ui->pageFiles, &FormPageFiles::onTvNavigationClicked);

    connect(ui->pageFilteredFiles, &FormPageFilterdFiles::sendGotoFile, this, &FormFileBrowser::onGotoFilePath);
    // 查询结果页的“返回目录”按钮：切回文件列表页面
    connect(ui->pageFilteredFiles, &FormPageFilterdFiles::sendBackRequested, this, [this]() {
        showFilesWidget();
    });
    // 结果页的【重新扫描】：不用缓存，直接走磁盘
    connect(ui->pageFilteredFiles, &FormPageFilterdFiles::sendRescanRequested,
            this, &FormFileBrowser::onRescanFromDiskRequested);
    connect(this, &FormFileBrowser::sendGotoFile, ui->pageFiles, &FormPageFiles::recvGotoFile);

    connect(this, &FormFileBrowser::sendShowFilteredFiles, ui->pageFilteredFiles, &FormPageFilterdFiles::onShowFilteredFiles);
    connect(this, &FormFileBrowser::sendRefresh, ui->pageFiles, &FormPageFiles::onBtnRefreshClicked);
    connect(ui->pageProgressInfo, &FormPageProgressInfo::sendStop, this, &FormFileBrowser::onRecvStopSearch);

    connect(ui->pageFiles, &FormPageFiles::sendLabels, this, &FormFileBrowser::onRecvLabels);

    // 进入目录后立即把该目录的标签送到【当前目录标签】面板（不再依赖先执行“遍历文件”）。
    connect(ui->pageFiles, &FormPageFiles::sendDirLabels, this,
            [this](const QString &sDir, const QSet<QString> &setLabels) {
                emit sendClearDirLabels();
                if (sDir.isEmpty())
                    return;
                QMap<QString, QSet<QString>> mapDirAndLabels;
                mapDirAndLabels.insert(sDir, setLabels);
                emit sendDirAndLabels(mapDirAndLabels);
            });
    // 页面内的提示（如目录文件过多只统计了前 N 个）转发到主窗口状态栏
    connect(ui->pageFiles, &FormPageFiles::sendHintMessage, this, &FormFileBrowser::sendStatusMessage);
    // 标签写盘后让相关目录的搜索缓存失效，避免按旧标签命中/漏掉新标签
    connect(ui->pageFiles, &FormPageFiles::sendTagsChanged, this,
            [this](const QStringList &qLFilePaths) {
                if (m_sqlOperation == nullptr)
                    return;
                // 缓存按"遍历根"存，文件可能在根的子孙目录里，
                // 必须按文件反查所有包含它的根目录，不能只删 absolutePath()。
                for (const QString &sFilePath : qLFilePaths)
                    m_sqlOperation->invalidateCacheForFile(sFilePath);
            });

    // 页面里要改标签之前先广播，这里记住改动前的标签用于撤销
    connect(ui->pageFiles, &FormPageFiles::sendBeforeTagsChanged,
            this, &FormFileBrowser::onBeforeTagsChanged);
}

FormFileBrowser::~FormFileBrowser()
{
    // 退出前先停掉并等待后台线程：QThread 运行中被销毁会触发 Qt 警告/崩溃，
    // 线程还可能继续回调已销毁的 UI 或写 ADS。
    // 先对所有线程发停止信号，让它们并行退出，再逐个等待，避免串行等待累计超时。
    QThread *pSearch = m_threadSearch;
    QThread *pTraverse = m_threadTraverseDirs;
    QThread *pAI = m_threadAddLabelByAI;

    if (m_threadSearch != nullptr)
        m_threadSearch->stopThread();
    if (m_threadTraverseDirs != nullptr)
        m_threadTraverseDirs->stopThread();
    if (m_threadAddLabelByAI != nullptr)
        m_threadAddLabelByAI->stopThread();

    auto waitThread = [](QThread *pThread, const QString &sName) {
        if (pThread == nullptr)
            return;
        if (!pThread->wait(2000)) {
            qWarning() << sName << "未能在 2 秒内退出，退出时将其分离";
            pThread->setParent(nullptr);
        }
    };
    waitThread(pSearch, QStringLiteral("搜索线程"));
    waitThread(pTraverse, QStringLiteral("遍历线程"));
    waitThread(pAI, QStringLiteral("AI 线程"));

    // m_sqlOperation 在构造函数中 new 在堆上，从未释放，导致内存泄漏。
    delete m_sqlOperation;
    delete ui;
}

/**
 * 左侧【文件导航】面板的收起/展开按钮。
 *
 * 做法：把原本直接放在 QSplitter 里的 wFileNavigation 包进一个容器 m_navPanel，
 * 容器顶部放一个"◀/▶"按钮；收起时隐藏目录树、把容器限宽成 24px 的窄条，
 * 按钮仍留在窄条上，点一下即可展开（和 DeepSeek Harness 左侧栏的折叠按钮一个意思）。
 */
void FormFileBrowser::initNavPanelToggle()
{
    m_navPanel = new QWidget(this);
    m_navPanel->setObjectName(QStringLiteral("navPanel"));
    auto *pLayout = new QVBoxLayout(m_navPanel);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);

    // 顶部条形控件：高度固定，收起后按钮仍然贴在最上面
    auto *pHeaderWidget = new QWidget(m_navPanel);
    pHeaderWidget->setObjectName(QStringLiteral("navHeader"));
    pHeaderWidget->setFixedHeight(36);
    auto *pHeader = new QHBoxLayout(pHeaderWidget);
    pHeader->setContentsMargins(2, 2, 2, 2);
    pHeader->setSpacing(0);

    m_btnToggleNav = new QToolButton(pHeaderWidget);
    m_btnToggleNav->setObjectName(QStringLiteral("btnToggleNav"));
    m_btnToggleNav->setAutoRaise(true);
    m_btnToggleNav->setFixedSize(30, 30);
    m_btnToggleNav->setIconSize(QSize(20, 20));
    m_btnToggleNav->setIcon(makePanelToggleIcon(QColor(0x33, 0x33, 0x33)));
    m_btnToggleNav->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_btnToggleNav->setCursor(Qt::PointingHandCursor);
    m_btnToggleNav->setFocusPolicy(Qt::NoFocus);
    m_btnToggleNav->setStyleSheet(QStringLiteral(
        "QToolButton{border:1px solid transparent;border-radius:4px;padding:0;}"
        "QToolButton:hover{background:#e3e3e3;border-color:#d0d0d0;}"));
    pHeader->addWidget(m_btnToggleNav, 0, Qt::AlignLeft | Qt::AlignVCenter);
    pHeader->addStretch(1);
    // 对齐到顶部：目录树收起后布局里只剩这一条，不加 AlignTop 会被垂直居中
    pLayout->addWidget(pHeaderWidget, 0, Qt::AlignTop);

    // 先占住 splitter 的第 0 个位置，再把原来的目录树搬进容器
    ui->splitter->insertWidget(0, m_navPanel);
    m_navPanel->setMinimumWidth(kNavExpandedMinWidth);
    pLayout->addWidget(ui->wFileNavigation, 1);

    // 记住目录树原本的宽度，收起/展开时按它恢复
    m_nNavExpandedWidth = qBound(kNavExpandedMinWidth, ui->wFileNavigation->sizeHint().width(), 320);

    connect(m_btnToggleNav, &QToolButton::clicked, this, [this]() {
        setNavPanelVisible(!m_bNavPanelVisible);
    });

    // 首帧布局完成后再设置一次宽度：此时 splitter 才有真实宽度，否则会被压缩成一条缝
    QTimer::singleShot(0, this, [this]() {
        if (!m_bNavPanelVisible || m_navPanel == nullptr)
            return;
        const int nTotal = ui->splitter->width();
        if (nTotal < 300)
            return;
        const int nNav = qBound(kNavExpandedMinWidth, m_nNavExpandedWidth, qMax(kNavExpandedMinWidth, nTotal / 2));
        ui->splitter->setSizes(QList<int>() << nNav << (nTotal - nNav));
    });

    // 恢复上次的状态（默认展开）
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    const bool bVisible = settings.value(QStringLiteral("view/navPanelVisible"), true).toBool();
    setNavPanelVisible(bVisible, false);
}

void FormFileBrowser::setNavPanelVisible(bool bVisible, bool bSave)
{
    if (m_navPanel == nullptr || m_btnToggleNav == nullptr)
        return;

    // 收起前记住当前宽度，展开时按它恢复
    if (!bVisible && m_bNavPanelVisible && m_navPanel->width() > 60)
        m_nNavExpandedWidth = m_navPanel->width();

    m_bNavPanelVisible = bVisible;
    ui->wFileNavigation->setVisible(bVisible);

    if (bVisible) {
        m_navPanel->setMinimumWidth(kNavExpandedMinWidth);
        m_navPanel->setMaximumWidth(QWIDGETSIZE_MAX);
        m_btnToggleNav->setToolTip(tr("隐藏目录树"));

        // 用户手动展开时才按记忆宽度恢复；启动时保持 splitter 原有的比例
        if (bSave) {
            const int nTotal = ui->splitter->width();
            if (nTotal > 200) {
                const int nNav = qBound(kNavExpandedMinWidth, m_nNavExpandedWidth,
                                        qMax(kNavExpandedMinWidth, nTotal / 2));
                ui->splitter->setSizes(QList<int>() << nNav << (nTotal - nNav));
            }
        }
    } else {
        // 收起后只留一条窄条，按钮还在上面
        m_navPanel->setMinimumWidth(kNavRailWidth);
        m_navPanel->setMaximumWidth(kNavRailWidth);
        m_btnToggleNav->setToolTip(tr("显示目录树"));
    }

    if (bSave)
        saveNavPanelVisible(bVisible);
}

void FormFileBrowser::saveNavPanelVisible(bool bVisible) const
{
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    settings.setValue(QStringLiteral("view/navPanelVisible"), bVisible);
}

void FormFileBrowser::setWindowStyleSheet()
{
    QList<int> sizes;
    // 在保持比例的情况下，绝对值要尽量大
    sizes << 10000 << 40000;
    ui->splitter->setSizes(sizes);

    // 设置初始窗口大小
    resize(1500,800);

    // 初始显示文件列表
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}

void FormFileBrowser::setActions(QAction *aAddLabelByAI, QAction *aAddLabel, QAction *aDelLabel, QAction *aSearch, QAction *aTraverse)
{
    m_aAddLabelByAI = aAddLabelByAI;
    m_aAddLabel = aAddLabel;
    m_aDelLabel = aDelLabel;
    m_aSearch = aSearch;
    m_aTraverse = aTraverse;

    connect(m_aAddLabelByAI, &QAction::triggered, this, &FormFileBrowser::onActionAddLabelByAITriggered);
    connect(m_aAddLabel, &QAction::triggered, this, &FormFileBrowser::onActionAddLabelsTriggered);
    connect(m_aSearch, &QAction::triggered, this, &FormFileBrowser::onActionSearchFilesbyLabelsTriggered);
    connect(m_aTraverse, &QAction::triggered, this, &FormFileBrowser::onActionTraverseSelDirsTriggered);

    ui->pageFiles->setAction(aDelLabel);

    // 搜索按钮右侧的小箭头菜单（先建好再交给页面，页面建命令条时用它做分裂按钮）
    initSearchButtonMenu();
    ui->pageFiles->setSearchButtonMenu(aSearch, m_searchMenu);

    // 方案A：把"作用于文件列表"的命令做成页面顶部的命令条（主窗口不再有全局工具栏）。
    // nullptr 表示分隔符。
    ui->pageFiles->setCommandActions(QList<QAction *>()
                                     << aAddLabelByAI << nullptr
                                     << aAddLabel << aDelLabel << nullptr
                                     << aSearch << aTraverse);
}

QStringList FormFileBrowser::getSelFilePath()
{
    return ui->pageFiles->getSelFilePath();
}

QStringList FormFileBrowser::getSelDirs()
{
    return ui->pageFiles->getSelDirs();
}

void FormFileBrowser::restoreLastDir()
{
    ui->pageFiles->restoreLastDir();
}


void FormFileBrowser::showProcessPage(RUNNINGTHREAD rt)
{
    ui->stackedWidget->setCurrentWidget(ui->pageProgressInfo);
    m_nThread = rt;
}

void FormFileBrowser::showFilesWidget()
{
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}

void FormFileBrowser::showFilteredFile(QStringList qLFilteredFiles)
{
    // 将命中缓存的部分与磁盘查找得到的部分合并，避免部分目录命中缓存时被整体丢弃。
    if(!m_qLFilePathsFromCache.isEmpty())
    {
        qLFilteredFiles = m_qLFilePathsFromCache + qLFilteredFiles;
        m_qLFilePathsFromCache.clear();
    }
    setActionState(SEARCH_FINISH);
    ui->stackedWidget->setCurrentWidget(ui->pageFilteredFiles);
    ui->pageFilteredFiles->setSearchSource(m_sSearchSource);   // 结果来源
    emit sendShowFilteredFiles(qLFilteredFiles);
    ui->pageProgressInfo->clear();

    qInfo() << "搜索完成: results=" << qLFilteredFiles.size();
}

void FormFileBrowser::onGotoFilePath(QString sFilePath)
{
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
    emit sendGotoFile(sFilePath);
}

void FormFileBrowser::onTvNavigationClicked(const QModelIndex &index)
{
    if(ui->stackedWidget->currentWidget() == ui->pageProgressInfo)
    {
        emit sendStop();
        return;
    }

    emit sendNavigationClicked(index);

    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}


/**
 * 使用标签名作为ads文件名，创建一系列文件，形如
 * filepath1:label1
 * filepath1:label2
 * filepath2:label3
 * todo: 后续可以在ads文件中写入一些内容，比如标签的简介
*/

/**
 * 根据给定的标签查找具有这些标签的文件。
 * 根据配置信息，选择从缓存数据库中查找还是直接从磁盘查找。
 * 从缓存数据库中查找速度会很快，适用于上次标记完文件后，文件夹中文件未发生变动的情况。
 * 如果上次标记完以后移动或删除文件，则可能查找不准确。
 * 选中多个文件夹时，有可能有些文件夹在缓存数据库中没有信息，这些文件夹直接从磁盘查找
*/

