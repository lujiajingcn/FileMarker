#include "formpagefiles.h"

#include "pathbreadcrumb.h"
#include "toolicons.h"
#include "formpagefilesutil.h"
#include "ui_formpagefiles.h"

#include <QKeySequence>
#include <QMenu>
#include <QDebug>
#include <QMimeData>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QToolButton>
#include <QActionGroup>
#include <QApplication>
#include <QDrag>
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

namespace {

// 详情视图第 0~3 列（名称/大小/类型/修改日期）禁止就地编辑。
// 原因：重命名走的是 QFileSystemModel::setData()，而那要求模型不是只读的，
// 所以代码里调用了 m_modelFiles->setReadOnly(false)；副作用是 flags() 会把第 0 列
// 也标成 ItemIsEditable —— 双击文件名或按 F2 会直接就地改名（无确认、无撤销，
// 实测把 audio.wav 改错了名）。
// 这里用"拒绝创建编辑器"的代理把这些列锁死；标签列用的是 TagChipDelegate（chip + ✕）。
class NoEditDelegate : public QStyledItemDelegate
{
public:
    explicit NoEditDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return nullptr;   // 不创建编辑器 → 无法改名（视图对 nullptr 有判空）
    }
};

} // namespace

FormPageFiles::FormPageFiles(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormPageFiles)
{
    ui->setupUi(this);

    // 导航行（后退/前进/上一级/刷新）的图标同样改成代码绘制，高分屏下不糊
    ui->backButton->setIcon(ToolIcons::make(ToolIcons::Back));
    ui->forwardButton->setIcon(ToolIcons::make(ToolIcons::Forward));
    ui->toParentButton->setIcon(ToolIcons::make(ToolIcons::Up));
    ui->btnRefresh->setIcon(ToolIcons::make(ToolIcons::Refresh));

    m_delegate = nullptr;
    m_iconProvider = nullptr;
    m_iconSize = ICON_SIZE;
    m_sortColumn = 0;
    m_sortOrder = Qt::AscendingOrder;
    m_scanTimer = nullptr;
    m_scanRow = 0;
    m_scanTotal = 0;

    m_modelFiles = new MyQFileSystemModel(this);
    // 设置根路径（例如当前目录）
    m_modelFiles->setRootPath("");

    // 当前目录标签的分片扫描定时器：每片最多 15ms，避免一次枚举上千个文件卡住界面
    m_scanTimer = new QTimer(this);
    m_scanTimer->setInterval(0);
    connect(m_scanTimer, &QTimer::timeout, this, &FormPageFiles::continueDirLabelScan);

    // 目录内容异步加载完成后再扫一次标签（导航时立刻扫描时 rowCount 可能还是 0）
    connect(m_modelFiles, &QFileSystemModel::directoryLoaded, this, [this](const QString &sPath) {
        if (FmPageUtil::isSameDirPath(sPath, m_curDir))
            scanDirLabels(m_curDir);
    });
    // 大目录加载时 QFileSystemModel 是**分批**插入行的。以前每插入一批就从第 0 行
    // 重扫一遍（scanDirLabels(..., true) 会把 m_scanRow 归零），几万个文件等于 O(N²)，
    // 表现就是"进大目录后一直卡着、永远扫不完"。
    // 现在改成续扫：行号跟着插入/删除平移，只在真正的重排/重置时才重来。
    connect(m_modelFiles, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &parent, int first, int last) {
                if (m_scanDir.isEmpty() || !FmPageUtil::isSameDirPath(m_scanDir, m_curDir))
                    return;
                if (m_scanRootIndex.isValid() && parent != m_scanRootIndex)
                    return;   // 只关心当前目录这一层的行
                if (first < m_scanRow)
                    m_scanRow += (last - first + 1);   // 插在已扫区域之前，行号整体后移
                const int nRows = m_modelFiles->rowCount(m_scanRootIndex);
                m_scanTotal = qMin(qMax(m_scanTotal, nRows), LABEL_SCAN_MAX_FILES);
                if (m_scanTimer != nullptr && !m_scanTimer->isActive() && m_scanRow < m_scanTotal)
                    m_scanTimer->start();   // 已经扫完的目录，追加文件后继续补扫
            });
    connect(m_modelFiles, &QAbstractItemModel::rowsRemoved, this,
            [this](const QModelIndex &parent, int first, int last) {
                if (m_scanDir.isEmpty() || !FmPageUtil::isSameDirPath(m_scanDir, m_curDir))
                    return;
                if (m_scanRootIndex.isValid() && parent != m_scanRootIndex)
                    return;
                const int nRemoved = last - first + 1;
                if (first < m_scanRow)
                    m_scanRow = qMax(0, m_scanRow - nRemoved);
                const int nRows = m_modelFiles->rowCount(m_scanRootIndex);
                m_scanTotal = qBound(m_scanRow, m_scanTotal - nRemoved, nRows);
                if (m_scanTimer != nullptr && !m_scanTimer->isActive() && m_scanRow < m_scanTotal)
                    m_scanTimer->start();
            });
    // 排序/重排会让所有行号失效，只能重来（只有用户主动排序才会触发，代价可接受）。
    auto restartScanIfActive = [this]() {
        if (m_scanTimer != nullptr && m_scanTimer->isActive() && !m_curDir.isEmpty())
            scanDirLabels(m_curDir, true);
    };
    connect(m_modelFiles, &QAbstractItemModel::layoutChanged, this, [restartScanIfActive]() {
        restartScanIfActive();
    });
    connect(m_modelFiles, &QAbstractItemModel::modelReset, this,
            [restartScanIfActive]() { restartScanIfActive(); });

    initDetailFileList();
    initThumbnailFileList();

    // 恢复上次的列宽/列顺序（窗口大小在 MainWindow 中恢复）
    {
        QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        settings.setIniCodec("UTF-8");
#endif
        const QByteArray headerState = settings.value(QStringLiteral("view/headerState")).toByteArray();
        const int nSettingsVersion = settings.value(QStringLiteral("settings/version"), 0).toInt();
        if (nSettingsVersion >= 2 && !headerState.isEmpty()
                && m_filesTree != nullptr && m_filesTree->header() != nullptr) {
            m_filesTree->header()->restoreState(headerState);
            m_headerRestored = true;
            // 旧版本保存的状态可能把列模式/表头可点击性也恢复成旧值，这里重申当前策略：
            // 所有列都可拖动调宽（与 Windows 资源管理器一致），最后一列不再自动撑满。
            QHeaderView *header = m_filesTree->header();
            header->setSectionsClickable(true);
            for (int i = 0; i < header->count(); ++i)
                header->setSectionResizeMode(i, QHeaderView::Interactive);
            header->setStretchLastSection(false);
        }

        // restoreState 会恢复上次的表头排序箭头，但模型排序状态仍是初始值；
        // 用我们保存的排序列/方向重新 applySort，保证箭头、模型和右键勾选一致。
        const int nSortColumn = settings.value(QStringLiteral("view/sortColumn"), 0).toInt();
        const int nSortOrder = settings.value(QStringLiteral("view/sortOrder"),
                                              static_cast<int>(Qt::AscendingOrder)).toInt();
        if (nSortColumn >= 0 && nSortColumn <= 3)
            applySort(nSortColumn, static_cast<Qt::SortOrder>(nSortOrder));
    }

    // 显式连接工具栏按钮与文件列表信号（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(ui->backButton, &QToolButton::clicked, this, &FormPageFiles::onBackButtonClicked);
    connect(ui->forwardButton, &QToolButton::clicked, this, &FormPageFiles::onForwardButtonClicked);

    // 后退/前进按钮带历史下拉：点按钮右侧的小箭头（或按住不放）能看到走过的目录，
    // 直接点某一项就跳过去（资源管理器同款）。菜单在弹出前按当前栈重建。
    auto *backHistoryMenu = new QMenu(ui->backButton);
    ui->backButton->setMenu(backHistoryMenu);
    ui->backButton->setPopupMode(QToolButton::MenuButtonPopup);
    connect(backHistoryMenu, &QMenu::aboutToShow, this, [this, backHistoryMenu]() {
        rebuildHistoryMenu(backHistoryMenu, true);
    });

    auto *forwardHistoryMenu = new QMenu(ui->forwardButton);
    ui->forwardButton->setMenu(forwardHistoryMenu);
    ui->forwardButton->setPopupMode(QToolButton::MenuButtonPopup);
    connect(forwardHistoryMenu, &QMenu::aboutToShow, this, [this, forwardHistoryMenu]() {
        rebuildHistoryMenu(forwardHistoryMenu, false);
    });
    connect(ui->toParentButton, &QToolButton::clicked, this, &FormPageFiles::onToParentButtonClicked);
    connect(ui->btnRefresh, &QToolButton::clicked, this, &FormPageFiles::onBtnRefreshClicked);
    // 「详情显示 / 缩略图显示」收进右键菜单的【查看】子菜单（见 fillViewMenu），
    // 工具条上这两个按钮不再显示，把它所在的那一行整行隐藏掉。
    if (ui->widget_3 != nullptr)
        ui->widget_3->hide();

    // 按钮没了，补两个快捷键，切换视图不用每次都去右键
    auto *actionDetail = new QAction(this);
    actionDetail->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D));
    actionDetail->setShortcutContext(Qt::WindowShortcut);
    connect(actionDetail, &QAction::triggered, this, &FormPageFiles::onTbDetailClicked);
    addAction(actionDetail);

    auto *actionThumb = new QAction(this);
    actionThumb->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_T));
    actionThumb->setShortcutContext(Qt::WindowShortcut);
    connect(actionThumb, &QAction::triggered, this, &FormPageFiles::onTbThumbnailClicked);
    addAction(actionThumb);
    connect(ui->tbDetail, &QToolButton::clicked, this, &FormPageFiles::onTbDetailClicked);
    connect(ui->tbThumbnail, &QToolButton::clicked, this, &FormPageFiles::onTbThumbnailClicked);
    connect(ui->tvFiles, &QTreeView::doubleClicked, this, &FormPageFiles::onTvFilesDoubleClicked);
    connect(ui->tvFiles, &QTreeView::customContextMenuRequested,
            this, &FormPageFiles::onTvFilesCustomContextMenuRequested);
    connect(ui->listView, &QListView::customContextMenuRequested,
            this, &FormPageFiles::onListViewCustomContextMenuRequested);

    // F2 重命名：作用于当前视图里选中的那一项（两个视图都支持）。
    // 用 WidgetWithChildrenShortcut：只有焦点在文件页里时才生效，
    // 不会在标签库/地址栏等地方误触发。
    m_actionRename = new QAction(tr("重命名…"), this);
    m_actionRename->setShortcut(QKeySequence(Qt::Key_F2));
    m_actionRename->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_actionRename, &QAction::triggered, this, [this]() {
        const QModelIndex index = currentIndexInActiveView();
        if (index.isValid())
            renameFile(index);
    });
    addAction(m_actionRename);

    // 下面几个动作只挂到两个文件视图上（WidgetWithChildrenShortcut），
    // 这样地址栏的输入框、右侧标签库的 Ctrl+A 都不会被抢走。
    auto addToFileViews = [this](QAction *menuAction) {
        if (m_filesTree != nullptr)
            m_filesTree->addAction(menuAction);
        if (m_filesList != nullptr)
            m_filesList->addAction(menuAction);
    };

    // Ctrl+A：全选当前目录里的文件。焦点在地址栏时仍然让输入框自己全选文本。
    m_actionSelectAll = new QAction(tr("全选"), this);
    m_actionSelectAll->setShortcut(QKeySequence(QKeySequence::SelectAll));
    m_actionSelectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_actionSelectAll, &QAction::triggered, this, [this]() {
        if (auto *edit = qobject_cast<QLineEdit *>(QApplication::focusWidget())) {
            edit->selectAll();
            return;
        }
        if (m_filesList != nullptr && m_filesList->isVisible())
            m_filesList->selectAll();
        else if (m_filesTree != nullptr)
            m_filesTree->selectAll();
    });
    addAction(m_actionSelectAll);

    // Enter：打开文件 / 进入目录（和双击一致）
    m_actionOpen = new QAction(tr("打开"), this);
    m_actionOpen->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::Key_Return)
                                                     << QKeySequence(Qt::Key_Enter));
    m_actionOpen->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_actionOpen, &QAction::triggered, this, [this]() {
        const QModelIndex index = currentIndexInActiveView();
        if (index.isValid())
            onDoubleClicked(index);
    });
    addToFileViews(m_actionOpen);

    // Delete：清空选中文件的全部标签（等同命令条的"清空文件标签"，会先确认）
    m_actionClearTags = new QAction(tr("清空选中文件的标签"), this);
    m_actionClearTags->setShortcut(QKeySequence(Qt::Key_Delete));
    m_actionClearTags->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_actionClearTags, &QAction::triggered, this, &FormPageFiles::onDeleteLabels);
    addToFileViews(m_actionClearTags);

    // F3 / Ctrl+Shift+F：弹出搜索按钮右侧的"标签关系 / 数据来源"菜单
    m_actionSearchMenu = new QAction(tr("查找条件…"), this);
    m_actionSearchMenu->setShortcuts(QList<QKeySequence>() << QKeySequence(Qt::Key_F3)
                                                           << QKeySequence(Qt::CTRL + Qt::SHIFT
                                                                           + Qt::Key_F));
    m_actionSearchMenu->setShortcutContext(Qt::WindowShortcut);
    connect(m_actionSearchMenu, &QAction::triggered, this, &FormPageFiles::popupSearchMenu);
    addAction(m_actionSearchMenu);

    // 地址栏：资源管理器风格的路径面包屑（我的电脑 › 盘符 › 目录…，点哪段跳哪层）。
    // 原来的 QComboBox 换成 PathBreadcrumb；历史记录仍然由 rememberPath() 维护，
    // 通过面包屑末尾的 ▾ 按钮或右键弹出。
    m_breadcrumb = new PathBreadcrumb(this);

    // 找出真正装着 lookInCombo 的那个布局（它就在导航行的 QGridLayout 里，
    // 而 lookInCombo->parentWidget()->layout() 拿到的是外层布局，不一定是它）。
    QLayout *navLayout = nullptr;
    for (QLayout *layout : findChildren<QLayout *>()) {
        if (layout->indexOf(ui->lookInCombo) >= 0) {
            navLayout = layout;
            break;
        }
    }
    if (navLayout != nullptr) {
        // 原来那一格靠 QComboBox 的 hsizetype=Ignored 撑满；换成面包屑后要按它
        // 实际所在的列设置拉伸，否则只占 sizeHint 宽度、并且被挤成几十像素宽。
        if (auto *grid = qobject_cast<QGridLayout *>(navLayout)) {
            int row = 0;
            int column = 0;
            int rowSpan = 1;
            int columnSpan = 1;
            grid->getItemPosition(grid->indexOf(ui->lookInCombo), &row, &column,
                                  &rowSpan, &columnSpan);
            // gridLayout_2 原来给第 0 列（后退按钮所在列）设了 stretch=2，
            // 只加不减的话多出来的宽度会被它吃掉，面包屑仍然被挤成一小条。
            for (int i = 0; i < grid->columnCount(); ++i)
                grid->setColumnStretch(i, 0);
            grid->setColumnStretch(column, 1);
        }
        navLayout->replaceWidget(ui->lookInCombo, m_breadcrumb);
        ui->lookInCombo->hide();   // 只留下当历史数据的壳，不再显示
    }
    connect(m_breadcrumb, &PathBreadcrumb::pathActivated, this, [this](const QString &sPath) {
        navigateToDir(sPath.isEmpty() ? FmPageUtil::myComputerName() : sPath);
    });
    connect(m_breadcrumb, &PathBreadcrumb::editCommitted, this, &FormPageFiles::navigateToDir);
    connect(m_breadcrumb, &PathBreadcrumb::recentDirsRequested, this, [this](const QPoint &globalPos) {
        QMenu menu;
        for (const QString &sDir : m_recentDirs)
            menu.addAction(sDir);
        if (menu.isEmpty())
            menu.addAction(tr("（还没有最近访问记录）"))->setEnabled(false);
        QAction *chosen = menu.exec(globalPos);
        if (chosen != nullptr && chosen->isEnabled())
            navigateToDir(chosen->text());
    });

    ui->lookInCombo->setInsertPolicy(QComboBox::NoInsert);
    ui->lookInCombo->setMaxVisibleItems(10);
    loadRecentDirs();
    ui->lookInCombo->setEditText(m_curDir);
    m_breadcrumb->setPath(m_curDir);

    // Ctrl+L：把面包屑切换成可输入路径的输入框（和资源管理器一样）
    auto *actionFocusPath = new QAction(this);
    actionFocusPath->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_L));
    actionFocusPath->setShortcutContext(Qt::WindowShortcut);
    connect(actionFocusPath, &QAction::triggered, this, [this]() { m_breadcrumb->startEditing(); });
    addAction(actionFocusPath);

    connect(m_modelFiles, &MyQFileSystemModel::sendLabels, this, &FormPageFiles::onRecvLabels);
}

FormPageFiles::~FormPageFiles()
{
    // 保存列宽与上次目录，下次启动恢复
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    if (m_filesTree != nullptr && m_filesTree->header() != nullptr)
        settings.setValue(QStringLiteral("view/headerState"), m_filesTree->header()->saveState());
    settings.setValue(QStringLiteral("settings/version"), 2);
    settings.setValue(QStringLiteral("view/sortColumn"), m_sortColumn);
    settings.setValue(QStringLiteral("view/sortOrder"), static_cast<int>(m_sortOrder));
    if (!m_curDir.isEmpty())
        settings.setValue(QStringLiteral("view/lastDir"), m_curDir);

    delete m_iconProvider;   // 非 QObject，需要手动释放
    delete ui;
}


void FormPageFiles::initDetailFileList()
{
    m_filesTree = ui->tvFiles;
     // 设置可以选中多行
    m_filesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // 接受从【标签库】拖过来的标签（见 eventFilter / applyDroppedLabels）
    m_filesTree->setAcceptDrops(true);
    m_filesTree->viewport()->setAcceptDrops(true);
    // 拖出由本类自己处理（见 eventFilter → startFilesDrag）：视图内建拖拽在多选时
    // 只会带走"按下那一行"，支持不了多文件。这里只保留"能接收拖入"。
    m_filesTree->setDragEnabled(false);
    m_filesTree->setDragDropMode(QAbstractItemView::DropOnly);
    m_filesTree->viewport()->installEventFilter(this);
    m_filesTree->setModel(m_modelFiles);
    // 第 0~3 列装"拒绝编辑"代理，禁止 F2/双击把文件名改到磁盘上。
    for (int nCol = 0; nCol <= 3; ++nCol)
        m_filesTree->setItemDelegateForColumn(nCol, new NoEditDelegate(m_filesTree));
    // 标签列也不再就地编辑：双击标签单元格改为弹出"增删式"标签对话框
    // （在 onDoubleClicked 里处理），与缩略图的双击语义统一。
    // 标签列改成 chip 样式：每个标签一个带 ✕ 的小药丸，点 ✕ 单独删该标签
    m_tagChipDelegate = new TagChipDelegate(m_filesTree);
    m_filesTree->setItemDelegateForColumn(4, m_tagChipDelegate);
    connect(m_tagChipDelegate, &TagChipDelegate::removeTagRequested,
            this, &FormPageFiles::onRemoveTagFromRow);
    m_filesTree->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_filesTree->setRootIndex(m_modelFiles->index(""));
    // 排序统一由 applySort 控制：表头可点击排序，但“标签”列（第 5 列）不参与，
    // 否则 QFileSystemModel 会按名称兜底排序，与表头指示不一致。
    m_filesTree->setSortingEnabled(false);
    // setSortingEnabled(false) 不会把表头设为可点击；Qt 的 sectionClicked 只在
    // sectionsClickable 为 true 时发出，必须显式打开，否则表头点击排序失效。
    m_filesTree->header()->setSectionsClickable(true);
    m_filesTree->header()->setSortIndicatorShown(true);
    m_filesTree->header()->setSortIndicator(m_sortColumn, m_sortOrder);
    m_modelFiles->sort(m_sortColumn, m_sortOrder);
    connect(m_filesTree->header(), &QHeaderView::sectionClicked, this, [this](int section) {
        // 标签列不支持排序：给一句提示，而不是"点了完全没反应"
        if (section == 4) {
            emit sendHintMessage(tr("标签列暂不支持排序；双击标签列可以编辑该文件的标签。"));
            return;
        }
        if (section < 0 || section > 3)
            return;
        const Qt::SortOrder order = (section == m_sortColumn && m_sortOrder == Qt::AscendingOrder)
                                        ? Qt::DescendingOrder
                                        : Qt::AscendingOrder;
        applySort(section, order);
    });

    // 设置不能展开节点，以使得QTreeView表现的像QListView一样。
    m_filesTree->setItemsExpandable(false);

    // 设置表头
    // 拷贝自Qt源码的Src\qtbase\src\widgets\dialogs\qfiledialg.cpp
    // 根据实际数据稍作修改，增加了列的宽度
    QHeaderView *treeHeader = m_filesTree->header();
    QFontMetrics fm(m_filesTree->font());
    treeHeader->setMinimumSectionSize(60);
    // 所有列都允许拖动调整宽度（和 Windows 资源管理器一致），最后一列不再自动撑满，
    // 拖动结果由 view/headerState 持久化。
    treeHeader->setSectionResizeMode(QHeaderView::Interactive);
    treeHeader->setStretchLastSection(false);
    treeHeader->resizeSection(1, fm.horizontalAdvance(QLatin1String("128.88 GB")) + 15);
    treeHeader->resizeSection(2, fm.horizontalAdvance(QLatin1String("mp3Folder")) + 35);
    treeHeader->resizeSection(3, fm.horizontalAdvance(QLatin1String("10/29/1981 02:02PM")) + 5);
    treeHeader->resizeSection(4, 180);
    treeHeader->setContextMenuPolicy(Qt::ActionsContextMenu);

    // 名称列默认吃掉剩余宽度；只设一次初始值，之后完全交给用户拖动。
    // 放到事件循环里做，构造时 viewport 还没有真实宽度。
    QTimer::singleShot(0, this, [this]() {
        if (m_headerRestored || m_filesTree == nullptr || m_filesTree->header() == nullptr)
            return;
        QHeaderView *h = m_filesTree->header();
        int nUsed = 0;
        for (int i = 1; i < h->count(); ++i)
            nUsed += h->sectionSize(i);
        const int nNameWidth = m_filesTree->viewport()->width() - nUsed;
        if (nNameWidth > 160)
            h->resizeSection(0, nNameWidth);
    });

    m_curDir = FmPageUtil::myComputerName();
}

void FormPageFiles::initThumbnailFileList()
{
    m_filesList = ui->listView;
    // 同样接受从【标签库】拖过来的标签
    m_filesList->setAcceptDrops(true);
    m_filesList->viewport()->setAcceptDrops(true);
    // 缩略图视图同样：拖出由自己处理，这里只接收拖入
    m_filesList->setDragEnabled(false);
    m_filesList->setDragDropMode(QAbstractItemView::DropOnly);
    // eventFilter 在下面（悬停高亮那里）已经装过，这里不重复装

    // 缩略图生成器：由 delegate 在 GUI 线程按需调用。
    // 不设置为 model 的 iconProvider：QFileSystemModel 是在后台线程取图标的，
    // 而 QPixmap / Shell COM 只能在 GUI 线程安全使用。
    m_iconProvider = new ThumbnailIconProvider(m_iconSize);

    m_modelFiles->setReadOnly(false);

    // 创建列表视图
    m_filesList->setModel(m_modelFiles);

    // QTreeView和QListView共享同一个选择模型
    QItemSelectionModel *selectionModel = m_filesTree->selectionModel();
    m_filesList->setSelectionModel(selectionModel);

    // 设置视图属性以显示缩略图
    m_filesList->setViewMode(QListView::IconMode);    // 图标模式
    m_filesList->setIconSize(QSize(m_iconSize, m_iconSize));    // 设置图标大小
    // 横向只稍微比图标宽一点（和资源管理器一样贴着图标排），纵向才留出文字空间
    m_filesList->setGridSize(QSize(m_iconSize + THUMB_GRID_HMARGIN,
                                    m_iconSize + THUMB_GRID_VMARGIN));
    m_filesList->setResizeMode(QListView::Adjust);    // 自动调整布局
    m_filesList->setMovement(QListView::Static);      // 禁止拖动
    m_filesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_filesList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_filesList->setMouseTracking(true);              // 悬停高亮需要鼠标移动事件

    // 缩略图不再用"双击文件名/F2 就地重命名"：双击统一为"打开/进入"，
    // 标签编辑走双击标签区（弹对话框），重命名走右键菜单。
    m_filesList->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_delegate = new EditableDelegate(this);
    m_delegate->setIconSize(m_iconSize);
    m_delegate->setThumbnailProvider(m_iconProvider);
    m_filesList->setItemDelegate(m_delegate);

    connect(m_delegate, &EditableDelegate::sendDblClick, this, &FormPageFiles::recvDblClick);
    connect(m_delegate, &EditableDelegate::sendEditTag, this, &FormPageFiles::onEditTag);
    // 缩略图里点标签 chip 上的 ✕：删除该标签（与详情视图同一套逻辑）
    connect(m_delegate, &EditableDelegate::sendRemoveTag, this, &FormPageFiles::onRemoveTagFromRow);

    // 悬停高亮：进入某项时高亮，鼠标离开视图时清除
    connect(m_filesList, &QAbstractItemView::entered, this, &FormPageFiles::onThumbnailHoverChanged);
    connect(m_filesList, &QAbstractItemView::viewportEntered, this, [this]() {
        if (m_delegate)
            m_delegate->setHoveredIndex(QModelIndex());
        m_filesList->viewport()->update();
    });
    m_filesList->viewport()->installEventFilter(this);

    // 设置根索引为当前目录
    m_filesList->setRootIndex(m_modelFiles->index(""));
}


/** 读取地址栏下拉历史（最多 10 条），并填充到 QComboBox。 */

/** 把地址栏下拉历史写回 settings。 */


// 不再需要拦截 Paint 事件来更新工具按钮。
// 原 event() 重写中每次重绘都调用 updateToolButtons()，降低 UI 性能。
// 按钮状态仅在导航栈变化时（back/forward/parent 点击、双击目录、onTvNavigationClicked）
// 需要更新，这些调用点均已直接调用 updateToolButtons()。


QStringList FormPageFiles::getSelFilePath()
{
    // 注意不能用 selectionModel()->selectedRows(0)：它只返回"整行都被选中"的行，
    // 而缩略图视图是 IconMode、只选到第 0 列，QFileSystemModel 却仍有 5 列，
    // 于是 selectedRows(0) 在缩略图里永远为空 —— 表现为"框选了一堆文件，
    // 点添加标签却提示未选中文件"。
    // 改成取 selectedIndexes() 再只保留第 0 列，既覆盖两种视图，也不会重复 5 次。
    QStringList selFilePath;
    QItemSelectionModel *model = m_filesTree->selectionModel();
    if (model == nullptr)
        return selFilePath;
    const QModelIndexList selected = model->selectedIndexes();
    for (const QModelIndex &index : selected) {
        if (index.column() != 0)
            continue;
        const QString sPath = m_modelFiles->filePath(index);
        if (!sPath.isEmpty() && !selFilePath.contains(sPath))
            selFilePath << sPath;
    }
    return selFilePath;
}

QStringList FormPageFiles::getSelDirs()
{
    QStringList qLSelDirs;
    QItemSelectionModel *selModel = m_filesTree->selectionModel();
    const QModelIndexList selected = selModel != nullptr ? selModel->selectedIndexes()
                                                         : QModelIndexList();
    QString sSelect;
    for (const QModelIndex &index : selected)
    {
        if (index.column() != 0)
            continue;   // 同一行只取一次（缩略图里只选到第 0 列，详情里整行都被选中）
        QString sFilePath = m_modelFiles->filePath(index);
        QFileInfo fileInfo(sFilePath);
        if (fileInfo.isDir() && !qLSelDirs.contains(sFilePath))
        {
            qLSelDirs << sFilePath;
        }
    }
    if (qLSelDirs.isEmpty())
    {
        qLSelDirs << m_curDir;
    }
    return qLSelDirs;
}


/** F3 / Ctrl+Shift+F：把"查找条件"菜单弹在搜索按钮下方（键盘也能改条件）。 */


/** 标签写盘后只刷新受影响的文件（不再整目录重读 ADS）。 */
void FormPageFiles::refreshAfterTagChange(const QStringList &qLFilePaths)
{
    for (const QString &sFilePath : qLFilePaths)
        m_modelFiles->clearTagCacheForFile(sFilePath);

    if (m_filesTree != nullptr)
        m_filesTree->viewport()->update();
    if (m_filesList != nullptr)
        m_filesList->viewport()->update();

    // 【当前目录标签】面板还是要重算（它展示整个目录的标签集合）
    emit sendDirLabels(m_curDir, QSet<QString>());
    QTimer::singleShot(100, this, [this]() { scanDirLabels(m_curDir, true); });
}

void FormPageFiles::onBtnRefreshClicked()
{
    QModelIndex curIndex = m_modelFiles->index(m_curDir);
    if (curIndex.isValid())
    {
        // 清空标签缓存，强制重新枚举 ADS（QFileSystemModel 本身通过文件监视器自动感知文件增删，
        // 这里只需保证标签列刷新；Qt5 的 QFileSystemModel 没有 refresh() 接口）。
        m_modelFiles->clearTagCache();
        m_filesTree->setRootIndex(curIndex);
        m_filesList->setRootIndex(curIndex);
    }
    m_filesTree->viewport()->update();
    m_filesList->viewport()->update();

    // 标签可能刚被编辑过：同步刷新【当前目录标签】面板（强制重扫）
    emit sendDirLabels(m_curDir, QSet<QString>());
    QTimer::singleShot(100, this, [this]() { scanDirLabels(m_curDir, true); });
}


/**
 * 点了标签 chip 上的 ✕：把该标签从文件上删掉。
 * 走和"编辑标签"同一套流程：先记撤销现场，再写盘，失败时提示并刷新。
 */


/** 统一的重命名入口（右键菜单）。 */


bool FormPageFiles::eventFilter(QObject *watched, QEvent *event)
{
    if (m_filesList != nullptr && watched == m_filesList->viewport() && event->type() == QEvent::Leave) {
        if (m_delegate)
            m_delegate->setHoveredIndex(QModelIndex());
        m_filesList->viewport()->update();
    }

    const bool bIsFileView = (m_filesTree != nullptr && watched == m_filesTree->viewport())
                             || (m_filesList != nullptr && watched == m_filesList->viewport());

    // 把选中的文件拖出去（拖到标签库的某个标签上 = 打标签）
    if (bIsFileView) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_ptDragStart = mouseEvent->pos();
                m_mayStartDrag = true;
                // 立刻记住选择：紧接着视图自己处理按下事件时会把多选收窄成一行
                m_pendingDragFiles = getSelFilePath();
            }
        } else if (event->type() == QEvent::MouseMove && m_mayStartDrag) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton)
                && (mouseEvent->pos() - m_ptDragStart).manhattanLength()
                       >= QApplication::startDragDistance()) {
                m_mayStartDrag = false;
                startFilesDrag();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_mayStartDrag = false;
            m_pendingDragFiles.clear();
        }
    }

    // 从【标签库】把标签拖到文件列表 = 给文件打标签
    if (bIsFileView && (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove)) {
        auto *dragEvent = static_cast<QDragMoveEvent *>(event);
        if (dragEvent->mimeData() != nullptr
            && dragEvent->mimeData()->hasFormat(QString::fromLatin1(MIME_FILEMARKER_LABEL))) {
            dragEvent->setDropAction(Qt::CopyAction);
            dragEvent->accept();
            return true;
        }
    } else if (bIsFileView && event->type() == QEvent::Drop) {
        auto *dropEvent = static_cast<QDropEvent *>(event);
        if (dropEvent->mimeData() != nullptr
            && dropEvent->mimeData()->hasFormat(QString::fromLatin1(MIME_FILEMARKER_LABEL))) {
            const QString sPayload = QString::fromUtf8(
                dropEvent->mimeData()->data(QString::fromLatin1(MIME_FILEMARKER_LABEL)));
            QStringList qLLabels;
            for (const QString &sLabel : sPayload.split(QLatin1Char('\n'))) {
                const QString sTrimmed = sLabel.trimmed();
                if (!sTrimmed.isEmpty() && !qLLabels.contains(sTrimmed))
                    qLLabels << sTrimmed;
            }

            QAbstractItemView *activeView = nullptr;
            if (m_filesList != nullptr && watched == m_filesList->viewport())
                activeView = m_filesList;
            else
                activeView = m_filesTree;
            const QModelIndex dropIndex = activeView != nullptr ? activeView->indexAt(dropEvent->pos()) : QModelIndex();
            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
            applyDroppedLabels(qLLabels, dropIndex);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

/**
 * 把选中的文件打包成拖拽数据（拖到标签库的标签上 = 给这些文件加这个标签）。
 *
 * 不用视图内建的拖拽：QAbstractItemView 在多选时按下某一行会把选择收窄成那一行，
 * 真正拖起来时就只剩一个文件了。这里在按下时就把文件列表记下来，自己发 QDrag。
 */
void FormPageFiles::startFilesDrag()
{
    QStringList qLFiles = m_pendingDragFiles;
    if (qLFiles.isEmpty())
        qLFiles = getSelFilePath();
    m_pendingDragFiles.clear();
    if (qLFiles.isEmpty())
        return;

    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(MIME_FILEMARKER_FILES), qLFiles.join("\n").toUtf8());
    // 同时给出标准 uri-list，方便拖到资源管理器/其它程序
    QList<QUrl> qLUrls;
    for (const QString &sFile : qLFiles)
        qLUrls << QUrl::fromLocalFile(sFile);
    mime->setUrls(qLUrls);

    // 拖拽开始前把多选恢复回来：按下鼠标时 QAbstractItemView 已经把选择收窄成一行，
    // 不恢复的话拖拽过程中界面上只亮着一个文件，看起来像"只拖了一个"。
    restoreFileSelection(qLFiles);

    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction);
}

/** 按给定文件列表恢复两个视图的选择状态（拖拽过程中用）。 */
void FormPageFiles::restoreFileSelection(const QStringList &qLFiles)
{
    QAbstractItemView *view = nullptr;
    if (m_filesList != nullptr && m_filesList->isVisible())
        view = m_filesList;
    else
        view = m_filesTree;
    if (view == nullptr || view->selectionModel() == nullptr)
        return;

    QItemSelection selection;
    for (const QString &sFile : qLFiles) {
        const QModelIndex index = m_modelFiles->index(sFile);
        if (index.isValid())
            selection.select(index, index);
    }
    if (selection.isEmpty())
        return;
    view->selectionModel()->select(selection,
                                   QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

/**
 * 把从标签库拖过来的标签写到文件上。
 * 拖到某个文件上 → 只作用于这个文件；拖到空白处/文件夹上 → 作用于当前选中的文件。
 */
void FormPageFiles::applyDroppedLabels(const QStringList &qLLabels, const QModelIndex &dropIndex)
{
    if (qLLabels.isEmpty())
        return;

    QStringList qLTargets;
    if (dropIndex.isValid() && !m_modelFiles->isDir(dropIndex)) {
        // 拖到某个文件上时：如果这一行本来就在多选里，就作用于整个选择
        // （资源管理器同款行为）；否则只作用于这一行。
        const QString sDropped = QDir::cleanPath(m_modelFiles->filePath(dropIndex));
        const QStringList qLSelFiles = getSelFilePath();
        if (qLSelFiles.size() > 1 && qLSelFiles.contains(sDropped))
            qLTargets = qLSelFiles;
        else
            qLTargets << sDropped;
    } else {
        qLTargets = getSelFilePath();
    }
    qLTargets.removeAll(QString());

    if (qLTargets.isEmpty()) {
        emit sendHintMessage(tr("请先把要打标签的文件选中，或者把标签直接拖到某个文件上。"));
        return;
    }

    applyLabelsToFiles(qLLabels, qLTargets);
}

/**
 * 把指定标签写到指定文件上（拖拽的两种方向都走这里）：
 *  - 标签 → 文件：applyDroppedLabels() 先算出目标文件再调用本函数；
 *  - 文件 → 标签：FormLabels 那边把文件列表传过来，调用本函数。
 * 这里统一处理撤销点、缓存失效、界面刷新和失败提示。
 */
void FormPageFiles::applyLabelsToFiles(const QStringList &qLLabels, const QStringList &qLFiles)
{
    // 统一成 QFileSystemModel 的路径形式（正斜杠），否则 clearTagCacheForFile() 清不掉，
    // 标签列的 chip 会停留在旧内容。
    QStringList qLTargets;
    for (const QString &sFile : qLFiles) {
        if (sFile.trimmed().isEmpty())
            continue;
        const QString sClean = QDir::cleanPath(sFile);
        if (!qLTargets.contains(sClean))
            qLTargets << sClean;
    }
    if (qLLabels.isEmpty() || qLTargets.isEmpty())
        return;

    emit sendBeforeTagsChanged(qLTargets);   // 支持 Ctrl+Z 撤销

    int nFailed = 0;
    QStringList qLFailedFiles;
    for (const QString &sFilePath : qLTargets) {
        if (!ADSOperation::appendTags(sFilePath, qLLabels)) {
            ++nFailed;
            qLFailedFiles << QFileInfo(sFilePath).fileName();
        }
    }

    emit sendTagsChanged(qLTargets);
    refreshAfterTagChange(qLTargets);

    if (nFailed > 0) {
        QMessageBox::warning(this, tr("添加标签失败"),
                             tr("以下 %1 个文件没能写入标签（可能被占用或只读）：\n%2")
                                 .arg(nFailed)
                                 .arg(qLFailedFiles.mid(0, 5).join(QStringLiteral("\n"))));
    } else {
        emit sendHintMessage(tr("已给 %1 个文件添加标签：%2")
                                 .arg(qLTargets.size())
                                 .arg(qLLabels.join(QStringLiteral("、"))));
    }

    // 注意：不要用 sendLabels 去"同步面板"——那个信号的含义是
    // "请把这份标签文本写到这个文件上"，会再做一次写盘并把撤销点覆盖成写入后的状态。
}


/** 由 FormLabels 转发过来：用户把文件拖到了某个标签上（可以同时拖到多选标签上）。 */
void FormPageFiles::onFilesDroppedOnLabel(const QStringList &qLFiles, const QStringList &qLLabels)
{
    QStringList qLCleanLabels;
    for (const QString &sLabel : qLLabels) {
        const QString sTrimmed = sLabel.trimmed();
        if (!sTrimmed.isEmpty() && !qLCleanLabels.contains(sTrimmed))
            qLCleanLabels << sTrimmed;
    }
    if (qLCleanLabels.isEmpty())
        return;
    applyLabelsToFiles(qLCleanLabels, qLFiles);
}

QModelIndex FormPageFiles::currentIndexInActiveView() const
{
    if (m_filesList != nullptr && m_filesList->isVisible())
        return m_filesList->currentIndex();
    return m_filesTree->currentIndex();
}

