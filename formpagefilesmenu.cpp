// FormPageFiles 的菜单实现：右键菜单、排序、视图切换、命令条。

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
    if (m_filesList != nullptr && m_filesList->isVisible())
        activeView = m_filesList;
    else
        activeView = m_filesTree;

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
    const QString firstFile = targetFile;
    // 选中的是文件夹时："打开"要在本程序里进去，"打开方式"无意义
    const bool bTargetIsDir = hasSelection && QFileInfo(targetFile).isDir();

    QMenu menu(this);

    // 文件操作项（打开 / 打开方式 / 资源管理器 / 复制路径 / 编辑标签 / 重命名）
    addFileActions(&menu, targetFile, firstFile, clickedIndex, bTargetIsDir);

    // 查看方式 / 排序：菜单项构建各自独立，主函数只负责组织顺序
    QMenu *viewMenu = menu.addMenu(tr("查看"));
    fillViewMenu(viewMenu);
    QMenu *sortMenu = menu.addMenu(tr("排序"));
    fillSortMenu(sortMenu);

    menu.addSeparator();
    QAction *actionRefresh = menu.addAction(tr("刷新"));

    connect(actionRefresh, &QAction::triggered, this, &FormPageFiles::onBtnRefreshClicked);

    menu.exec(globalPos);
}

/** 右键菜单里的文件操作项：打开 / 打开方式 / 在资源管理器中显示 / 复制路径 / 编辑标签 / 重命名。 */
void FormPageFiles::addFileActions(QMenu *menu, const QString &targetFile,
                                  const QString &firstFile, const QModelIndex &clickedIndex,
                                  bool bTargetIsDir)
{
    const bool hasSelection = !targetFile.isEmpty();
    const bool singleSelection = hasSelection;

    QAction *actionOpen = menu->addAction(tr("打开"));
    actionOpen->setEnabled(hasSelection);
    QAction *actionOpenWith = menu->addAction(tr("打开方式…"));
    actionOpenWith->setEnabled(singleSelection && !bTargetIsDir);
    QAction *actionReveal = menu->addAction(tr("在资源管理器中显示"));
    actionReveal->setEnabled(hasSelection);
    QAction *actionCopyPath = menu->addAction(tr("复制路径"));
    actionCopyPath->setEnabled(hasSelection);
    QAction *actionEditTags = menu->addAction(tr("编辑标签…"));
    actionEditTags->setEnabled(singleSelection);
    // 两个视图都不再支持就地改名（双击=打开），重命名统一放在这里。
    QAction *actionRename = menu->addAction(tr("重命名…"));
    actionRename->setEnabled(singleSelection);

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
}

/** 右键菜单里的【查看】子菜单：详情显示 / 缩略图显示，以及缩略图尺寸档位。 */
void FormPageFiles::fillViewMenu(QMenu *menu)
{
    // 查看方式
    QAction *actionDetail = menu->addAction(tr("详情显示"));
    QAction *actionThumbnail = menu->addAction(tr("缩略图显示"));
    actionDetail->setCheckable(true);
    actionThumbnail->setCheckable(true);
    actionDetail->setChecked(ui->stackedWidget->currentIndex() == FILEPAGE::DETAIL);
    actionThumbnail->setChecked(ui->stackedWidget->currentIndex() == FILEPAGE::THUMBNAIL);

    // 缩略图尺寸档位（只在缩略图视图有意义，详情视图不显示，避免误导）
    if (ui->stackedWidget->currentIndex() == FILEPAGE::THUMBNAIL) {
        QMenu *sizeMenu = menu->addMenu(tr("缩略图大小"));
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
            action->setChecked(option.second == m_iconSize);
            action->setData(option.second);
            sizeGroup->addAction(action);
            connect(action, &QAction::triggered, this, [this, action]() {
                setThumbnailSize(action->data().toInt());
            });
        }
    }

    connect(actionDetail, &QAction::triggered, this, &FormPageFiles::onTbDetailClicked);
    connect(actionThumbnail, &QAction::triggered, this, &FormPageFiles::onTbThumbnailClicked);
}

/** 右键菜单里的【排序】子菜单：按哪一列排、升序还是降序。 */
void FormPageFiles::fillSortMenu(QMenu *menu)
{
    // 排序：名称/大小/类型/修改日期 + 升降序
    QActionGroup *sortGroup = new QActionGroup(menu);
    sortGroup->setExclusive(true);
    const QList<QPair<QString, int>> sortOptions = {
        { tr("名称"), 0 },
        { tr("大小"), 1 },
        { tr("类型"), 2 },
        { tr("修改日期"), 3 },
    };
    for (const QPair<QString, int> &option : sortOptions) {
        QAction *action = menu->addAction(option.first);
        action->setCheckable(true);
        action->setChecked(option.second == m_sortColumn);
        sortGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, option]() {
            applySort(option.second, m_sortOrder);
        });
    }
    menu->addSeparator();
    QAction *actionAscending = menu->addAction(tr("升序"));
    QAction *actionDescending = menu->addAction(tr("降序"));
    actionAscending->setCheckable(true);
    actionDescending->setCheckable(true);
    actionAscending->setChecked(m_sortOrder == Qt::AscendingOrder);
    actionDescending->setChecked(m_sortOrder == Qt::DescendingOrder);
    connect(actionAscending, &QAction::triggered, this, [this]() {
        applySort(m_sortColumn, Qt::AscendingOrder);
    });
    connect(actionDescending, &QAction::triggered, this, [this]() {
        applySort(m_sortColumn, Qt::DescendingOrder);
    });
}

void FormPageFiles::onTvFilesCustomContextMenuRequested(const QPoint &pos)
{
    onCustomContextMenuRequested(pos);
}

void FormPageFiles::setSearchButtonMenu(QAction *searchAction, QMenu *contextMenu)
{
    // 只记录，真正建按钮在 setCommandActions() 里做。
    // 调用顺序：FormFileBrowser::setActions() 先调本函数、再调 setCommandActions()。
    m_searchAction = searchAction;
    m_searchMenu = contextMenu;
}

void FormPageFiles::popupSearchMenu()
{
    if (m_searchMenu == nullptr)
        return;
    QPoint pos;
    if (m_searchButton != nullptr && m_searchButton->isVisible()) {
        pos = m_searchButton->mapToGlobal(QPoint(0, m_searchButton->height()));
    } else {
        pos = QCursor::pos();
    }
    m_searchMenu->popup(pos);
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

        // 命令条本身靠右对齐（左侧留给弹性空白），和资源管理器把"命令区"放在右侧一致。
        // 包一层行容器是因为 QToolBar 自己不带"前置弹性空白"，直接插进 verticalLayout
        // 只能左对齐；注意行容器要在 setCommandActions() 之外只建一次，
        // 因为下面 m_commandBar->clear() 会清掉工具栏里的内容。
        m_commandRow = new QWidget(this);
        auto *rowLayout = new QHBoxLayout(m_commandRow);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(0);

        auto *spacer = new QWidget(m_commandRow);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        rowLayout->addWidget(spacer);
        rowLayout->addWidget(m_commandBar);

        // 放在地址栏那一行上方：ui->verticalLayout 的第 0 项
        ui->verticalLayout->insertWidget(0, m_commandRow);
    }

    m_commandBar->clear();
    bool bPendingSeparator = false;
    for (QAction *menuAction : actions) {
        if (menuAction == nullptr) {
            bPendingSeparator = !m_commandBar->actions().isEmpty();
            continue;
        }
        if (bPendingSeparator) {
            m_commandBar->addSeparator();
            bPendingSeparator = false;
        }
        // "搜索"做成分裂按钮：点主区域直接按上次条件搜索，点右侧小箭头出条件菜单
        if (menuAction == m_searchAction && m_searchMenu != nullptr) {
            auto *searchBtn = new QToolButton(m_commandBar);
            m_searchButton = searchBtn;   // F3 弹菜单时用来定位
            searchBtn->setDefaultAction(menuAction);
            searchBtn->setMenu(m_searchMenu);
            searchBtn->setPopupMode(QToolButton::MenuButtonPopup);
            searchBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
            searchBtn->setIconSize(QSize(18, 18));
            searchBtn->setAutoRaise(true);
            m_commandBar->addWidget(searchBtn);
            continue;
        }
        m_commandBar->addAction(menuAction);
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
    if (iconSize <= 0 || iconSize == m_iconSize)
        return;

    m_iconSize = iconSize;

    if (m_delegate)
        m_delegate->setIconSize(iconSize);
    if (m_iconProvider)
        m_iconProvider->setIconSize(iconSize);

    if (m_filesList) {
        m_filesList->setIconSize(QSize(iconSize, iconSize));
        m_filesList->setGridSize(QSize(iconSize + THUMB_GRID_HMARGIN,
                                      iconSize + THUMB_GRID_VMARGIN));
        m_filesList->viewport()->update();
    }
}

void FormPageFiles::onThumbnailHoverChanged(const QModelIndex &index)
{
    if (m_delegate == nullptr)
        return;

    m_delegate->setHoveredIndex(index);
    m_filesList->viewport()->update();
}

void FormPageFiles::applySort(int column, Qt::SortOrder order)
{
    m_sortColumn = column;
    m_sortOrder = order;

    m_modelFiles->sort(column, order);
    // 详细视图的表头同步显示排序指示；缩略图视图没有表头，靠右键菜单的勾选状态指示
    m_filesTree->header()->setSortIndicator(column, order);
}
