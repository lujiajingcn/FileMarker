#include "formpagefiles.h"
#include "ui_formpagefiles.h"
#include "formpagefilesutil.h"

#include <QKeySequence>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QToolButton>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QProcess>
#include <QMessageBox>
#include <QPushButton>
#include <QLineEdit>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QSettings>
#include <QElapsedTimer>

#include "adsoperation.h"
#include "tagchipdelegate.h"
#include "thumbnailiconprovider.h"
#include "editablefiledelegate.h"
#include "tageditdialog.h"
#include "utility.h"

void FormPageFiles::onCustomContextMenuRequested(const QPoint &pos)
{
    // 用右键发生位置定位，而不是 QCursor::pos()：键盘 Menu 键/触控/菜单期间鼠标移动时
    // 前者才是用户真正指向的项。
    QAbstractItemView *activeView = nullptr;
    if (m_lstFiles != nullptr && m_lstFiles->isVisible())
        activeView = m_lstFiles;
    else
        activeView = m_tvFiles;

    QModelIndex clickedIndex;
    if (activeView != nullptr)
        clickedIndex = activeView->indexAt(pos);
    if (!clickedIndex.isValid())
        clickedIndex = currentIndexInActiveView();

    const QPoint globalPos = activeView != nullptr
                                 ? activeView->viewport()->mapToGlobal(pos)
                                 : QCursor::pos();

    // 右键动作统一作用于鼠标所在项；只有空白处右键才退回当前选中项。
    // 这样多选时“打开方式/在资源管理器中显示”不会误作用到其它文件。
    QString targetFile;
    if (clickedIndex.isValid())
        targetFile = m_modelFiles->filePath(clickedIndex);
    if (targetFile.isEmpty()) {
        const QStringList qLSelFiles = getSelFilePath();
        if (!qLSelFiles.isEmpty())
            targetFile = qLSelFiles.first();
    }

    const bool hasSelection = !targetFile.isEmpty();
    const bool singleSelection = hasSelection;
    const QString firstFile = targetFile;
    // 选中的是文件夹时："打开"要在本程序里进去，"打开方式"无意义
    const bool bTargetIsDir = hasSelection && QFileInfo(targetFile).isDir();

    QMenu menu(this);

    QAction *actionOpen = menu.addAction(tr("打开"));
    actionOpen->setEnabled(hasSelection);
    QAction *actionOpenWith = menu.addAction(tr("打开方式…"));
    actionOpenWith->setEnabled(singleSelection && !bTargetIsDir);
    QAction *actionReveal = menu.addAction(tr("在资源管理器中显示"));
    actionReveal->setEnabled(hasSelection);
    QAction *actionCopyPath = menu.addAction(tr("复制路径"));
    actionCopyPath->setEnabled(hasSelection);
    QAction *actionEditTags = menu.addAction(tr("编辑标签…"));
    actionEditTags->setEnabled(singleSelection);
    // 两个视图都不再支持就地改名（双击=打开），重命名统一放在这里。
    QAction *actionRename = menu.addAction(tr("重命名…"));
    actionRename->setEnabled(singleSelection);

    menu.addSeparator();

    // 查看方式
    QMenu *viewMenu = menu.addMenu(tr("查看"));
    QAction *actionDetail = viewMenu->addAction(tr("详细信息"));
    QAction *actionThumbnail = viewMenu->addAction(tr("缩略图"));
    actionDetail->setCheckable(true);
    actionThumbnail->setCheckable(true);
    actionDetail->setChecked(ui->stackedWidget->currentIndex() == FILEPAGE::DETAIL);
    actionThumbnail->setChecked(ui->stackedWidget->currentIndex() == FILEPAGE::THUMBNAIL);

    // 缩略图尺寸档位（只在缩略图视图有意义，详情视图不显示，避免误导）
    if (ui->stackedWidget->currentIndex() == FILEPAGE::THUMBNAIL) {
        QMenu *sizeMenu = menu.addMenu(tr("缩略图大小"));
        QActionGroup *sizeGroup = new QActionGroup(sizeMenu);
        sizeGroup->setExclusive(true);
        const QList<QPair<QString, int>> sizeOptions = {
            { tr("小"), ICON_SIZE_SMALL },
            { tr("中"), ICON_SIZE_MEDIUM },
            { tr("大"), ICON_SIZE_LARGE },
        };
        for (const QPair<QString, int> &option : sizeOptions) {
            QAction *action = sizeMenu->addAction(option.first);
            action->setCheckable(true);
            action->setChecked(option.second == m_nIconSize);
            action->setData(option.second);
            sizeGroup->addAction(action);
            connect(action, &QAction::triggered, this, [this, action]() {
                setThumbnailSize(action->data().toInt());
            });
        }
    }

    // 排序：名称/大小/类型/修改日期 + 升降序
    QMenu *sortMenu = menu.addMenu(tr("排序"));
    QActionGroup *sortGroup = new QActionGroup(sortMenu);
    sortGroup->setExclusive(true);
    const QList<QPair<QString, int>> sortOptions = {
        { tr("名称"), 0 },
        { tr("大小"), 1 },
        { tr("类型"), 2 },
        { tr("修改日期"), 3 },
    };
    for (const QPair<QString, int> &option : sortOptions) {
        QAction *action = sortMenu->addAction(option.first);
        action->setCheckable(true);
        action->setChecked(option.second == m_nSortColumn);
        sortGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, option]() {
            applySort(option.second, m_sortOrder);
        });
    }
    sortMenu->addSeparator();
    QAction *actionAscending = sortMenu->addAction(tr("升序"));
    QAction *actionDescending = sortMenu->addAction(tr("降序"));
    actionAscending->setCheckable(true);
    actionDescending->setCheckable(true);
    actionAscending->setChecked(m_sortOrder == Qt::AscendingOrder);
    actionDescending->setChecked(m_sortOrder == Qt::DescendingOrder);
    connect(actionAscending, &QAction::triggered, this, [this]() {
        applySort(m_nSortColumn, Qt::AscendingOrder);
    });
    connect(actionDescending, &QAction::triggered, this, [this]() {
        applySort(m_nSortColumn, Qt::DescendingOrder);
    });

    menu.addSeparator();
    QAction *actionRefresh = menu.addAction(tr("刷新"));

    connect(actionOpen, &QAction::triggered, this, [this, targetFile, bTargetIsDir]() {
        if (targetFile.isEmpty())
            return;
        // 文件夹：在当前面板里进入该目录（和双击一致），不再弹系统资源管理器
        if (bTargetIsDir) {
            navigateToDir(targetFile);
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(targetFile));
    });
    connect(actionOpenWith, &QAction::triggered, this, [firstFile]() {
        QProcess::startDetached(QStringLiteral("rundll32.exe"),
                                QStringList() << QStringLiteral("shell32.dll,OpenAs_RunDLL")
                                              << QDir::toNativeSeparators(firstFile));
    });
    connect(actionReveal, &QAction::triggered, this, [firstFile]() {
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                QStringList() << (QStringLiteral("/select,")
                                                  + QDir::toNativeSeparators(firstFile)));
    });
    connect(actionCopyPath, &QAction::triggered, this, [targetFile]() {
        if (!targetFile.isEmpty())
            QApplication::clipboard()->setText(targetFile);
    });
    connect(actionEditTags, &QAction::triggered, this, [this, clickedIndex]() {
        onEditTag(clickedIndex.isValid() ? clickedIndex : currentIndexInActiveView());
    });
    connect(actionRename, &QAction::triggered, this, [this, clickedIndex]() {
        renameFile(clickedIndex.isValid() ? clickedIndex : currentIndexInActiveView());
    });
    connect(actionDetail, &QAction::triggered, this, &FormPageFiles::onTbDetailClicked);
    connect(actionThumbnail, &QAction::triggered, this, &FormPageFiles::onTbThumbnailClicked);
    connect(actionRefresh, &QAction::triggered, this, &FormPageFiles::onBtnRefreshClicked);

    menu.exec(globalPos);
}

void FormPageFiles::onTvFilesCustomContextMenuRequested(const QPoint &pos)
{
    onCustomContextMenuRequested(pos);
}

void FormPageFiles::setSearchButtonMenu(QAction *pSearchAction, QMenu *pMenu)
{
    // 只记录，真正建按钮在 setCommandActions() 里做。
    // 调用顺序：FormFileBrowser::setActions() 先调本函数、再调 setCommandActions()。
    m_pSearchAction = pSearchAction;
    m_pSearchMenu = pMenu;
}

void FormPageFiles::popupSearchMenu()
{
    if (m_pSearchMenu == nullptr)
        return;
    QPoint pos;
    if (m_pSearchButton != nullptr && m_pSearchButton->isVisible()) {
        pos = m_pSearchButton->mapToGlobal(QPoint(0, m_pSearchButton->height()));
    } else {
        pos = QCursor::pos();
    }
    m_pSearchMenu->popup(pos);
}

void FormPageFiles::setCommandActions(const QList<QAction *> &actions)
{
    if (m_commandBar == nullptr) {
        m_commandBar = new QToolBar(this);
        m_commandBar->setObjectName(QStringLiteral("fileCommandBar"));
        m_commandBar->setMovable(false);
        m_commandBar->setFloatable(false);
        m_commandBar->setIconSize(QSize(18, 18));
        // 只显示图标：文字全部收进 tooltip（悬停提示），命令条更紧凑
        m_commandBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
        // 放在地址栏那一行上方：ui->verticalLayout 的第 0 项
        ui->verticalLayout->insertWidget(0, m_commandBar);
    }

    m_commandBar->clear();
    bool bPendingSeparator = false;
    for (QAction *pAction : actions) {
        if (pAction == nullptr) {
            bPendingSeparator = !m_commandBar->actions().isEmpty();
            continue;
        }
        if (bPendingSeparator) {
            m_commandBar->addSeparator();
            bPendingSeparator = false;
        }
        // "搜索"做成分裂按钮：点主区域直接按上次条件搜索，点右侧小箭头出条件菜单
        if (pAction == m_pSearchAction && m_pSearchMenu != nullptr) {
            auto *pSearchBtn = new QToolButton(m_commandBar);
            m_pSearchButton = pSearchBtn;   // F3 弹菜单时用来定位
            pSearchBtn->setDefaultAction(pAction);
            pSearchBtn->setMenu(m_pSearchMenu);
            pSearchBtn->setPopupMode(QToolButton::MenuButtonPopup);
            pSearchBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
            pSearchBtn->setIconSize(QSize(18, 18));
            pSearchBtn->setAutoRaise(true);
            m_commandBar->addWidget(pSearchBtn);
            continue;
        }
        m_commandBar->addAction(pAction);
    }
}

void FormPageFiles::onTbDetailClicked()
{
    ui->stackedWidget->setCurrentIndex(FILEPAGE::DETAIL);
}

void FormPageFiles::onTbThumbnailClicked()
{
    ui->stackedWidget->setCurrentIndex(FILEPAGE::THUMBNAIL);
}

void FormPageFiles::onListViewCustomContextMenuRequested(const QPoint &pos)
{
    onCustomContextMenuRequested(pos);
}

// 切换缩略图尺寸：图标大小、格子大小、代理绘制尺寸、缩略图生成尺寸要一起改
void FormPageFiles::setThumbnailSize(int iconSize)
{
    if (iconSize <= 0 || iconSize == m_nIconSize)
        return;

    m_nIconSize = iconSize;

    if (m_delegate)
        m_delegate->setIconSize(iconSize);
    if (m_iconProvider)
        m_iconProvider->setIconSize(iconSize);

    if (m_lstFiles) {
        m_lstFiles->setIconSize(QSize(iconSize, iconSize));
        m_lstFiles->setGridSize(QSize(iconSize + THUMB_GRID_MARGIN,
                                      iconSize + THUMB_GRID_MARGIN));
        m_lstFiles->viewport()->update();
    }
}

void FormPageFiles::onThumbnailHoverChanged(const QModelIndex &index)
{
    if (m_delegate == nullptr)
        return;

    m_delegate->setHoveredIndex(index);
    m_lstFiles->viewport()->update();
}

void FormPageFiles::applySort(int column, Qt::SortOrder order)
{
    m_nSortColumn = column;
    m_sortOrder = order;

    m_modelFiles->sort(column, order);
    // 详细视图的表头同步显示排序指示；缩略图视图没有表头，靠右键菜单的勾选状态指示
    m_tvFiles->header()->setSortIndicator(column, order);
}
