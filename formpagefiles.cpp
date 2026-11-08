#include "formpagefiles.h"
#include "ui_formpagefiles.h"

#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QToolButton>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QProcess>

#include "adsoperation.h"
#include "thumbnailiconprovider.h"
#include "editablefiledelegate.h"
#include "utility.h"

FormPageFiles::FormPageFiles(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormPageFiles)
{
    ui->setupUi(this);

    m_delegate = nullptr;
    m_iconProvider = nullptr;
    m_nIconSize = ICON_SIZE;
    m_nSortColumn = 0;
    m_sortOrder = Qt::AscendingOrder;

    m_modelFiles = new MyQFileSystemModel(this);
    // 设置根路径（例如当前目录）
    m_modelFiles->setRootPath("");

    initDetailFileList();
    initThumbnailFileList();

    // 显式连接工具栏按钮与文件列表信号（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(ui->backButton, &QToolButton::clicked, this, &FormPageFiles::onBackButtonClicked);
    connect(ui->forwardButton, &QToolButton::clicked, this, &FormPageFiles::onForwardButtonClicked);
    connect(ui->toParentButton, &QToolButton::clicked, this, &FormPageFiles::onToParentButtonClicked);
    connect(ui->btnRefresh, &QToolButton::clicked, this, &FormPageFiles::onBtnRefreshClicked);
    connect(ui->tbDetail, &QToolButton::clicked, this, &FormPageFiles::onTbDetailClicked);
    connect(ui->tbThumbnail, &QToolButton::clicked, this, &FormPageFiles::onTbThumbnailClicked);
    connect(ui->tvFiles, &QTreeView::doubleClicked, this, &FormPageFiles::onTvFilesDoubleClicked);
    connect(ui->tvFiles, &QTreeView::customContextMenuRequested,
            this, &FormPageFiles::onTvFilesCustomContextMenuRequested);
    connect(ui->listView, &QListView::customContextMenuRequested,
            this, &FormPageFiles::onListViewCustomContextMenuRequested);

    // todo 后续将已经访问过的路径都添加上
    ui->lookInCombo->setEditText(m_sCurDir);

    connect(m_modelFiles, &MyQFileSystemModel::sendLabels, this, &FormPageFiles::onRecvLabels);
}

FormPageFiles::~FormPageFiles()
{
    delete m_iconProvider;   // 非 QObject，需要手动释放
    delete ui;
}

void FormPageFiles::setAction(QAction *actionDeleteLabels)
{
    m_actionDeleteLabels = actionDeleteLabels;
    connect(m_actionDeleteLabels, &QAction::triggered, this, &FormPageFiles::onDeleteLabels);
}

QString FormPageFiles::getCurDir()
{
    return m_sCurDir;
}

void FormPageFiles::onDeleteLabels()
{
    QStringList qLSelFilePaths = getSelFilePath();
    foreach(QString sSelFilePath, qLSelFilePaths)
    {
        ADSOperation::deleteADSFiles(sSelFilePath);
    }
    onBtnRefreshClicked();
}

void FormPageFiles::initDetailFileList()
{
    m_tvFiles = ui->tvFiles;
     // 设置可以选中多行
    m_tvFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tvFiles->setModel(m_modelFiles);

    m_tvFiles->setRootIndex(m_modelFiles->index(""));
    m_tvFiles->setSortingEnabled(true);
    m_tvFiles->header()->setSortIndicator(0, Qt::AscendingOrder);

    // 设置不能展开节点，以使得QTreeView表现的像QListView一样。
    m_tvFiles->setItemsExpandable(false);

    // 设置表头
    // 拷贝自Qt源码的Src\qtbase\src\widgets\dialogs\qfiledialg.cpp
    // 根据实际数据稍作修改，增加了列的宽度
    QHeaderView *treeHeader = m_tvFiles->header();
    QFontMetrics fm(m_tvFiles->font());
    treeHeader->resizeSection(0, fm.horizontalAdvance(QLatin1String("wwwwwwwwwwwwwwwwwwwwwwwwww")));
    treeHeader->resizeSection(1, fm.horizontalAdvance(QLatin1String("128.88 GB")) + 15);
    treeHeader->resizeSection(2, fm.horizontalAdvance(QLatin1String("mp3Folder")) + 35);
    treeHeader->resizeSection(3, fm.horizontalAdvance(QLatin1String("10/29/1981 02:02PM")) + 5);
    treeHeader->resizeSection(4, fm.horizontalAdvance(QLatin1String("10/29/1981 02:02PM")));
    treeHeader->setContextMenuPolicy(Qt::ActionsContextMenu);

    m_sCurDir = "我的电脑";
}

void FormPageFiles::initThumbnailFileList()
{
    m_lstFiles = ui->listView;

    // 缩略图生成器：由 delegate 在 GUI 线程按需调用。
    // 不设置为 model 的 iconProvider：QFileSystemModel 是在后台线程取图标的，
    // 而 QPixmap / Shell COM 只能在 GUI 线程安全使用。
    m_iconProvider = new ThumbnailIconProvider(m_nIconSize);

    m_modelFiles->setReadOnly(false);

    // 创建列表视图
    m_lstFiles->setModel(m_modelFiles);

    // QTreeView和QListView共享同一个选择模型
    QItemSelectionModel *selectionModel = m_tvFiles->selectionModel();
    m_lstFiles->setSelectionModel(selectionModel);

    // 设置视图属性以显示缩略图
    m_lstFiles->setViewMode(QListView::IconMode);    // 图标模式
    m_lstFiles->setIconSize(QSize(m_nIconSize, m_nIconSize));    // 设置图标大小
    m_lstFiles->setGridSize(QSize(m_nIconSize + THUMB_GRID_MARGIN,
                                  m_nIconSize + THUMB_GRID_MARGIN)); // 设置网格大小，为图标和两行文字留出空间
    m_lstFiles->setResizeMode(QListView::Adjust);    // 自动调整布局
    m_lstFiles->setMovement(QListView::Static);      // 禁止拖动
    m_lstFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_lstFiles->setContextMenuPolicy(Qt::CustomContextMenu);
    m_lstFiles->setMouseTracking(true);              // 悬停高亮需要鼠标移动事件

    m_lstFiles->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    m_delegate = new EditableDelegate(this);
    m_delegate->setIconSize(m_nIconSize);
    m_delegate->setThumbnailProvider(m_iconProvider);
    m_lstFiles->setItemDelegate(m_delegate);

    connect(m_delegate, &EditableDelegate::sendDblClick, this, &FormPageFiles::recvDblClick);
    connect(m_delegate, &EditableDelegate::sendEditTag, this, &FormPageFiles::onEditTag);

    // 悬停高亮：进入某项时高亮，鼠标离开视图时清除
    connect(m_lstFiles, &QAbstractItemView::entered, this, &FormPageFiles::onThumbnailHoverChanged);
    connect(m_lstFiles, &QAbstractItemView::viewportEntered, this, [this]() {
        if (m_delegate)
            m_delegate->setHoveredIndex(QModelIndex());
        m_lstFiles->viewport()->update();
    });
    m_lstFiles->viewport()->installEventFilter(this);

    // 设置根索引为当前目录
    m_lstFiles->setRootIndex(m_modelFiles->index(""));
}

void FormPageFiles::curDirChanged(QString sCurDir)
{
    ui->lookInCombo->setEditText(sCurDir);
    m_skBackward.push_back(m_sCurDir);
    m_skForward.clear();
    m_sCurDir = sCurDir;
    // P2-16：导航栈变化时直接更新按钮状态，不再依赖 event() 拦截 Paint。
    updateToolButtons();
}

// P2-16 删除：不再需要拦截 Paint 事件来更新工具按钮。
// 原 event() 重写中每次重绘都调用 updateToolButtons()，降低 UI 性能。
// 按钮状态仅在导航栈变化时（back/forward/parent 点击、双击目录、onTvNavigationClicked）
// 需要更新，这些调用点均已直接调用 updateToolButtons()。

void FormPageFiles::updateToolButtons()
{
    if(m_skBackward.count() == 0)
        ui->backButton->setEnabled(false);
    else
        ui->backButton->setEnabled(true);

    if(m_skForward.count() == 0)
        ui->forwardButton->setEnabled(false);
    else
        ui->forwardButton->setEnabled(true);

    QModelIndex rootIndex = m_tvFiles->rootIndex();
    QString sFilePath = m_modelFiles->filePath(rootIndex);
    if(sFilePath.isEmpty())
        ui->toParentButton->setEnabled(false);
    else
        ui->toParentButton->setEnabled(true);
}

QStringList FormPageFiles::getSelFilePath()
{
    // selectedIndexes() 会为同一行的每一列都返回一个索引，
    // 若直接收集会导致每个文件被重复返回 5 次（打标签/删标签时重复操作）。
    // 这里只取第 0 列（每选中的行一个索引），自动去重。
    QStringList selFilePath;
    QModelIndexList selected = m_tvFiles->selectionModel()->selectedRows(0);
    for (QModelIndexList::const_iterator cit = selected.begin(); cit != selected.end(); ++cit)
    {
        selFilePath << m_modelFiles->filePath(*cit);
    }
    return selFilePath;
}

QStringList FormPageFiles::getSelDirs()
{
    QStringList qLSelDirs;
    QModelIndexList selected = m_tvFiles->selectionModel()->selectedRows();
    QString sSelect;
    for (QModelIndexList::const_iterator cit = selected.begin(); cit != selected.end(); ++cit)
    {
        QString sFilePath = m_modelFiles->filePath(*cit);
        QFileInfo fileInfo(sFilePath);
        if(fileInfo.isDir())
        {
            qLSelDirs << sFilePath;
        }
    }
    if(qLSelDirs.isEmpty())
    {
        qLSelDirs << m_sCurDir;
    }
    return qLSelDirs;
}

void FormPageFiles::onTvFilesDoubleClicked(const QModelIndex &index)
{
    onDoubleClicked(index);
}

void FormPageFiles::onCustomContextMenuRequested(const QPoint &pos)
{
    Q_UNUSED(pos);

    const QStringList files = getSelFilePath();
    const bool hasSelection = !files.isEmpty();
    const bool singleSelection = (files.count() == 1);
    const QString firstFile = hasSelection ? files.first() : QString();

    QMenu menu(this);

    QAction *actionOpen = menu.addAction(tr("打开"));
    actionOpen->setEnabled(hasSelection);
    QAction *actionOpenWith = menu.addAction(tr("打开方式…"));
    actionOpenWith->setEnabled(singleSelection);
    QAction *actionReveal = menu.addAction(tr("在资源管理器中显示"));
    actionReveal->setEnabled(hasSelection);
    QAction *actionCopyPath = menu.addAction(tr("复制路径"));
    actionCopyPath->setEnabled(hasSelection);
    QAction *actionEditTags = menu.addAction(tr("编辑标签…"));
    actionEditTags->setEnabled(singleSelection);

    menu.addSeparator();

    // 查看方式
    QMenu *viewMenu = menu.addMenu(tr("查看"));
    QAction *actionDetail = viewMenu->addAction(tr("详细信息"));
    QAction *actionThumbnail = viewMenu->addAction(tr("缩略图"));
    actionDetail->setCheckable(true);
    actionThumbnail->setCheckable(true);
    actionDetail->setChecked(ui->stackedWidget->currentIndex() == FILEPAGE::DETAIL);
    actionThumbnail->setChecked(ui->stackedWidget->currentIndex() == FILEPAGE::THUMBNAIL);

    // 缩略图尺寸档位
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

    connect(actionOpen, &QAction::triggered, this, [files]() {
        for (const QString &file : files)
            QDesktopServices::openUrl(QUrl::fromLocalFile(file));
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
    connect(actionCopyPath, &QAction::triggered, this, [files]() {
        QApplication::clipboard()->setText(files.join("\n"));
    });
    connect(actionEditTags, &QAction::triggered, this, [this]() {
        onEditTag(currentIndexInActiveView());
    });
    connect(actionDetail, &QAction::triggered, this, &FormPageFiles::onTbDetailClicked);
    connect(actionThumbnail, &QAction::triggered, this, &FormPageFiles::onTbThumbnailClicked);
    connect(actionRefresh, &QAction::triggered, this, &FormPageFiles::onBtnRefreshClicked);

    menu.exec(QCursor::pos());
}

void FormPageFiles::onTvFilesCustomContextMenuRequested(const QPoint &pos)
{
    onCustomContextMenuRequested(pos);
}

void FormPageFiles::onBackButtonClicked()
{
    m_skForward.push_back(m_sCurDir);
    QString sDir = m_skBackward.pop();
    bothSetRootIndex(m_modelFiles->index(sDir));
    ui->lookInCombo->setEditText(sDir);
    m_sCurDir = sDir;
    updateToolButtons();  // P2-16：导航栈变化后直接更新按钮
}

void FormPageFiles::onForwardButtonClicked()
{
    m_skBackward.push_back(m_sCurDir);
    QString sDir = m_skForward.pop();
    bothSetRootIndex(m_modelFiles->index(sDir));
    ui->lookInCombo->setEditText(sDir);
    m_sCurDir = sDir;
    updateToolButtons();  // P2-16：导航栈变化后直接更新按钮
}

void FormPageFiles::onToParentButtonClicked()
{
    QModelIndex rootIndex = m_tvFiles->rootIndex();
    QString sFilePath = m_modelFiles->filePath(rootIndex);
    QDir dir(sFilePath);
    QString sNewDirectory;
    if (dir.isRoot())
    {
        sNewDirectory = m_modelFiles->myComputer().toString();
    }
    else
    {
        dir.cdUp();
        sNewDirectory = dir.absolutePath();
    }
    bothSetRootIndex(m_modelFiles->index(sNewDirectory));
    m_skBackward.push_back(sFilePath);
    m_sCurDir = sNewDirectory;
    updateToolButtons();  // P2-16：导航栈变化后直接更新按钮
}

void FormPageFiles::onBtnRefreshClicked()
{
    QModelIndex curIndex = m_modelFiles->index(m_sCurDir);
    if (curIndex.isValid())
    {
        // P2-3：清空标签缓存，强制重新枚举 ADS（QFileSystemModel 本身通过文件监视器自动感知文件增删，
        // 这里只需保证标签列刷新；Qt5 的 QFileSystemModel 没有 refresh() 接口）。
        m_modelFiles->clearTagCache();
        m_tvFiles->setRootIndex(curIndex);
        m_lstFiles->setRootIndex(curIndex);
    }
    m_tvFiles->viewport()->update();
    m_lstFiles->viewport()->update();
}

void FormPageFiles::recvGotoFile(QString sFilePath)
{
    // P2-4：用 QFileInfo 取父目录，正确处理盘符根（如 C:/file.txt 下 lastIndexOf('/') 会失效）。
    QFileInfo fi(sFilePath);
    QString sFileDir = fi.absolutePath();
    QModelIndex dirIndex = m_modelFiles->index(sFileDir);
    // P1-7：使用 bothSetRootIndex 同时更新详细视图和缩略图视图，
    // 否则缩略图模式下跳转后仍停留在旧目录。
    bothSetRootIndex(dirIndex);
    m_tvFiles->setCurrentIndex(m_modelFiles->index(sFilePath));
}

void FormPageFiles::onTvNavigationClicked(const QModelIndex &index)
{
    QString path = index.data(QFileSystemModel::FilePathRole).toString();
    m_tvFiles->setRootIndex(m_modelFiles->index(path));
    ui->stackedWidget->setCurrentIndex(FILEPAGE::DETAIL);
    curDirChanged(path);
}

void FormPageFiles::onTbDetailClicked()
{
    ui->stackedWidget->setCurrentIndex(FILEPAGE::DETAIL);
}

void FormPageFiles::onTbThumbnailClicked()
{
    ui->stackedWidget->setCurrentIndex(FILEPAGE::THUMBNAIL);
}

void FormPageFiles::onDoubleClicked(const QModelIndex &index)
{
    QFileInfo fileInfo = m_modelFiles->fileInfo(index);
    QString path = index.data(QFileSystemModel::FilePathRole).toString();
    if(fileInfo.isDir())
    {
        bothSetRootIndex(m_modelFiles->index(path));
        curDirChanged(path);
    }
    else if(fileInfo.isFile())
    {
        QUrl url = QUrl::fromLocalFile(path);
        QDesktopServices::openUrl(url);
    }
}

void FormPageFiles::bothSetRootIndex(const QModelIndex &index)
{
    m_tvFiles->setRootIndex(index);
    m_lstFiles->setRootIndex(index);
}

void FormPageFiles::onListViewCustomContextMenuRequested(const QPoint &pos)
{
    onCustomContextMenuRequested(pos);
}

void FormPageFiles::onRecvLabels(QString sFilePath, QString sLabels)
{
    emit sendLabels(sFilePath, sLabels);
}

void FormPageFiles::onEditTag(const QModelIndex &index)
{
    QString sFilePath = m_modelFiles->filePath(index);
    // 只对文件弹编辑框，目录无需标签
    if (sFilePath.isEmpty() || m_modelFiles->isDir(index))
        return;

    // 按录入顺序显示当前标签（NTFS 枚举流是按流名排序的，不能直接用它）
    QStringList adsNames = ADSOperation::listADSTagsInOrder(sFilePath);
    bool ok = false;
    QString sNewTags = QInputDialog::getText(this, tr("编辑标签"),
                                             tr("多个标签用逗号、顿号或分号分隔（支持中英文）："),
                                             QLineEdit::Normal,
                                             adsNames.join(","), &ok);
    if (!ok)
        return;

    // 先清空该文件全部标签，再按用户输入的顺序重新写入（P2-2）。
    // 支持英文逗号(,)、中文逗号(，)、顿号(、)、英文分号(;)、中文分号(；) 分隔。
    const QStringList qLLabels = Utility::splitLabels(sNewTags);
    ADSOperation::replaceTags(sFilePath, qLLabels);

    emit sendLabels(sFilePath, qLLabels.join(","));
    onBtnRefreshClicked();
}

void FormPageFiles::recvDblClick(const QModelIndex &index)
{
    onDoubleClicked(index);
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

bool FormPageFiles::eventFilter(QObject *watched, QEvent *event)
{
    if (m_lstFiles != nullptr && watched == m_lstFiles->viewport() && event->type() == QEvent::Leave) {
        if (m_delegate)
            m_delegate->setHoveredIndex(QModelIndex());
        m_lstFiles->viewport()->update();
    }
    return QWidget::eventFilter(watched, event);
}

void FormPageFiles::applySort(int column, Qt::SortOrder order)
{
    m_nSortColumn = column;
    m_sortOrder = order;

    m_modelFiles->sort(column, order);
    // 详细视图的表头同步显示排序指示；缩略图视图没有表头，靠右键菜单的勾选状态指示
    m_tvFiles->header()->setSortIndicator(column, order);
}

QModelIndex FormPageFiles::currentIndexInActiveView() const
{
    if (m_lstFiles != nullptr && m_lstFiles->isVisible())
        return m_lstFiles->currentIndex();
    return m_tvFiles->currentIndex();
}
