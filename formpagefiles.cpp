#include "formpagefiles.h"
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

// 详情视图第 5 列（标签列）专用代理：
// 默认代理的编辑器在标签列上只按文字宽度显示，输入长标签时几乎不可用；
// 这里让编辑器铺满整个单元格，其余行为与默认代理一致（写回模型第 5 列）。
class TagColumnDelegate : public QStyledItemDelegate
{
public:
    explicit TagColumnDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(option);
        QLineEdit *editor = new QLineEdit(parent);
        editor->setPlaceholderText(index.data(Qt::DisplayRole).toString());
        return editor;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit)
            lineEdit->setText(index.data(Qt::EditRole).toString());
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
        if (lineEdit)
            model->setData(index, lineEdit->text(), Qt::EditRole);
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(index);
        editor->setGeometry(option.rect.adjusted(1, 1, -1, -1));
    }
};

// 详情视图第 0~3 列（名称/大小/类型/修改日期）禁止就地编辑。
// 原因：为了让第 5 列（标签）可编辑，代码里调用了 m_modelFiles->setReadOnly(false)，
// 而 QFileSystemModel::flags() 会因此把第 0 列也标成 ItemIsEditable —— 于是双击文件名或按 F2
// 会真的重命名磁盘文件（无确认、无撤销，实测把 audio.wav 改了名）。
// 这里用一个"拒绝创建编辑器"的代理把这些列锁死；标签列仍用 TagColumnDelegate。
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

    m_delegate = nullptr;
    m_iconProvider = nullptr;
    m_nIconSize = ICON_SIZE;
    m_nSortColumn = 0;
    m_sortOrder = Qt::AscendingOrder;
    m_pScanTimer = nullptr;
    m_nScanRow = 0;
    m_nScanTotal = 0;

    m_modelFiles = new MyQFileSystemModel(this);
    // 设置根路径（例如当前目录）
    m_modelFiles->setRootPath("");

    // 当前目录标签的分片扫描定时器：每片最多 15ms，避免一次枚举上千个文件卡住界面
    m_pScanTimer = new QTimer(this);
    m_pScanTimer->setInterval(0);
    connect(m_pScanTimer, &QTimer::timeout, this, &FormPageFiles::continueDirLabelScan);

    // 目录内容异步加载完成后再扫一次标签（导航时立刻扫描时 rowCount 可能还是 0）
    connect(m_modelFiles, &QFileSystemModel::directoryLoaded, this, [this](const QString &sPath) {
        if (FmPageUtil::isSameDirPath(sPath, m_sCurDir))
            scanDirLabels(m_sCurDir);
    });
    // 扫描过程中排序/目录内容变化会改变行顺序，已扫描的行号会错位；只有正在扫描时才重启。
    auto restartScanIfActive = [this]() {
        if (m_pScanTimer != nullptr && m_pScanTimer->isActive() && !m_sCurDir.isEmpty())
            scanDirLabels(m_sCurDir, true);
    };
    connect(m_modelFiles, &QAbstractItemModel::layoutChanged, this, [restartScanIfActive]() {
        restartScanIfActive();
    });
    connect(m_modelFiles, &QAbstractItemModel::rowsInserted, this,
            [restartScanIfActive](const QModelIndex &, int, int) { restartScanIfActive(); });
    connect(m_modelFiles, &QAbstractItemModel::rowsRemoved, this,
            [restartScanIfActive](const QModelIndex &, int, int) { restartScanIfActive(); });
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
                && m_tvFiles != nullptr && m_tvFiles->header() != nullptr) {
            m_tvFiles->header()->restoreState(headerState);
            m_bHeaderRestored = true;
            // 旧版本保存的状态可能把列模式/表头可点击性也恢复成旧值，这里重申当前策略：
            // 所有列都可拖动调宽（与 Windows 资源管理器一致），最后一列不再自动撑满。
            QHeaderView *header = m_tvFiles->header();
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
    connect(ui->toParentButton, &QToolButton::clicked, this, &FormPageFiles::onToParentButtonClicked);
    connect(ui->btnRefresh, &QToolButton::clicked, this, &FormPageFiles::onBtnRefreshClicked);
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
    auto addToFileViews = [this](QAction *pAction) {
        if (m_tvFiles != nullptr)
            m_tvFiles->addAction(pAction);
        if (m_lstFiles != nullptr)
            m_lstFiles->addAction(pAction);
    };

    // Ctrl+A：全选当前目录里的文件。焦点在地址栏时仍然让输入框自己全选文本。
    m_actionSelectAll = new QAction(tr("全选"), this);
    m_actionSelectAll->setShortcut(QKeySequence(QKeySequence::SelectAll));
    m_actionSelectAll->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(m_actionSelectAll, &QAction::triggered, this, [this]() {
        if (auto *pEdit = qobject_cast<QLineEdit *>(QApplication::focusWidget())) {
            pEdit->selectAll();
            return;
        }
        if (m_lstFiles != nullptr && m_lstFiles->isVisible())
            m_lstFiles->selectAll();
        else if (m_tvFiles != nullptr)
            m_tvFiles->selectAll();
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

    // 地址栏下拉：展示最近 10 次打开过的目录（持久化，重启后仍在）。
    // 不用 QComboBox 的自动插入（InsertAtBottom），历史由 rememberPath() 统一维护。
    ui->lookInCombo->setInsertPolicy(QComboBox::NoInsert);
    ui->lookInCombo->setMaxVisibleItems(10);
    loadRecentDirs();
    ui->lookInCombo->setEditText(m_sCurDir);

    // 地址栏是可编辑的（.ui 中 editable=true）：回车或选择历史项时按路径导航。
    connect(ui->lookInCombo->lineEdit(), &QLineEdit::returnPressed,
            this, &FormPageFiles::onLookInComboReturnPressed);
    connect(ui->lookInCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index >= 0)
            navigateToDir(ui->lookInCombo->itemText(index));
    });

    connect(m_modelFiles, &MyQFileSystemModel::sendLabels, this, &FormPageFiles::onRecvLabels);
}

FormPageFiles::~FormPageFiles()
{
    // 保存列宽与上次目录，下次启动恢复
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    if (m_tvFiles != nullptr && m_tvFiles->header() != nullptr)
        settings.setValue(QStringLiteral("view/headerState"), m_tvFiles->header()->saveState());
    settings.setValue(QStringLiteral("settings/version"), 2);
    settings.setValue(QStringLiteral("view/sortColumn"), m_nSortColumn);
    settings.setValue(QStringLiteral("view/sortOrder"), static_cast<int>(m_sortOrder));
    if (!m_sCurDir.isEmpty())
        settings.setValue(QStringLiteral("view/lastDir"), m_sCurDir);

    delete m_iconProvider;   // 非 QObject，需要手动释放
    delete ui;
}





void FormPageFiles::initDetailFileList()
{
    m_tvFiles = ui->tvFiles;
     // 设置可以选中多行
    m_tvFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // 接受从【标签库】拖过来的标签（见 eventFilter / applyDroppedLabels）
    m_tvFiles->setAcceptDrops(true);
    m_tvFiles->viewport()->setAcceptDrops(true);
    m_tvFiles->viewport()->installEventFilter(this);
    m_tvFiles->setModel(m_modelFiles);
    // 第 0~3 列装"拒绝编辑"代理，禁止 F2/双击把文件名改到磁盘上。
    for (int nCol = 0; nCol <= 3; ++nCol)
        m_tvFiles->setItemDelegateForColumn(nCol, new NoEditDelegate(m_tvFiles));
    // 标签列也不再就地编辑：双击标签单元格改为弹出"增删式"标签对话框
    // （在 onDoubleClicked 里处理），与缩略图的双击语义统一。
    // 标签列改成 chip 样式：每个标签一个带 ✕ 的小药丸，点 ✕ 单独删该标签
    m_tagChipDelegate = new TagChipDelegate(m_tvFiles);
    m_tvFiles->setItemDelegateForColumn(4, m_tagChipDelegate);
    connect(m_tagChipDelegate, &TagChipDelegate::removeTagRequested,
            this, &FormPageFiles::onRemoveTagFromRow);
    m_tvFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_tvFiles->setRootIndex(m_modelFiles->index(""));
    // 排序统一由 applySort 控制：表头可点击排序，但“标签”列（第 5 列）不参与，
    // 否则 QFileSystemModel 会按名称兜底排序，与表头指示不一致。
    m_tvFiles->setSortingEnabled(false);
    // setSortingEnabled(false) 不会把表头设为可点击；Qt 的 sectionClicked 只在
    // sectionsClickable 为 true 时发出，必须显式打开，否则表头点击排序失效。
    m_tvFiles->header()->setSectionsClickable(true);
    m_tvFiles->header()->setSortIndicatorShown(true);
    m_tvFiles->header()->setSortIndicator(m_nSortColumn, m_sortOrder);
    m_modelFiles->sort(m_nSortColumn, m_sortOrder);
    connect(m_tvFiles->header(), &QHeaderView::sectionClicked, this, [this](int section) {
        // 标签列不支持排序：给一句提示，而不是"点了完全没反应"
        if (section == 4) {
            emit sendHintMessage(tr("标签列暂不支持排序；双击标签列可以编辑该文件的标签。"));
            return;
        }
        if (section < 0 || section > 3)
            return;
        const Qt::SortOrder order = (section == m_nSortColumn && m_sortOrder == Qt::AscendingOrder)
                                        ? Qt::DescendingOrder
                                        : Qt::AscendingOrder;
        applySort(section, order);
    });

    // 设置不能展开节点，以使得QTreeView表现的像QListView一样。
    m_tvFiles->setItemsExpandable(false);

    // 设置表头
    // 拷贝自Qt源码的Src\qtbase\src\widgets\dialogs\qfiledialg.cpp
    // 根据实际数据稍作修改，增加了列的宽度
    QHeaderView *treeHeader = m_tvFiles->header();
    QFontMetrics fm(m_tvFiles->font());
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
        if (m_bHeaderRestored || m_tvFiles == nullptr || m_tvFiles->header() == nullptr)
            return;
        QHeaderView *h = m_tvFiles->header();
        int nUsed = 0;
        for (int i = 1; i < h->count(); ++i)
            nUsed += h->sectionSize(i);
        const int nNameWidth = m_tvFiles->viewport()->width() - nUsed;
        if (nNameWidth > 160)
            h->resizeSection(0, nNameWidth);
    });

    m_sCurDir = FmPageUtil::myComputerName();
}

void FormPageFiles::initThumbnailFileList()
{
    m_lstFiles = ui->listView;
    // 同样接受从【标签库】拖过来的标签
    m_lstFiles->setAcceptDrops(true);
    m_lstFiles->viewport()->setAcceptDrops(true);
    // eventFilter 在下面（悬停高亮那里）已经装过，这里不重复装

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

    // 缩略图不再用"双击文件名/F2 就地重命名"：双击统一为"打开/进入"，
    // 标签编辑走双击标签区（弹对话框），重命名走右键菜单。
    m_lstFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_delegate = new EditableDelegate(this);
    m_delegate->setIconSize(m_nIconSize);
    m_delegate->setThumbnailProvider(m_iconProvider);
    m_lstFiles->setItemDelegate(m_delegate);

    connect(m_delegate, &EditableDelegate::sendDblClick, this, &FormPageFiles::recvDblClick);
    connect(m_delegate, &EditableDelegate::sendEditTag, this, &FormPageFiles::onEditTag);
    // 缩略图里点标签 chip 上的 ✕：删除该标签（与详情视图同一套逻辑）
    connect(m_delegate, &EditableDelegate::sendRemoveTag, this, &FormPageFiles::onRemoveTagFromRow);

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







/** 读取地址栏下拉历史（最多 10 条），并填充到 QComboBox。 */

/** 把地址栏下拉历史写回 settings。 */



// 不再需要拦截 Paint 事件来更新工具按钮。
// 原 event() 重写中每次重绘都调用 updateToolButtons()，降低 UI 性能。
// 按钮状态仅在导航栈变化时（back/forward/parent 点击、双击目录、onTvNavigationClicked）
// 需要更新，这些调用点均已直接调用 updateToolButtons()。


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









/** F3 / Ctrl+Shift+F：把"查找条件"菜单弹在搜索按钮下方（键盘也能改条件）。 */


/** 标签写盘后只刷新受影响的文件（不再整目录重读 ADS）。 */
void FormPageFiles::refreshAfterTagChange(const QStringList &qLFilePaths)
{
    for (const QString &sFilePath : qLFilePaths)
        m_modelFiles->clearTagCacheForFile(sFilePath);

    if (m_tvFiles != nullptr)
        m_tvFiles->viewport()->update();
    if (m_lstFiles != nullptr)
        m_lstFiles->viewport()->update();

    // 【当前目录标签】面板还是要重算（它展示整个目录的标签集合）
    emit sendDirLabels(m_sCurDir, QSet<QString>());
    QTimer::singleShot(100, this, [this]() { scanDirLabels(m_sCurDir, true); });
}

void FormPageFiles::onBtnRefreshClicked()
{
    QModelIndex curIndex = m_modelFiles->index(m_sCurDir);
    if (curIndex.isValid())
    {
        // 清空标签缓存，强制重新枚举 ADS（QFileSystemModel 本身通过文件监视器自动感知文件增删，
        // 这里只需保证标签列刷新；Qt5 的 QFileSystemModel 没有 refresh() 接口）。
        m_modelFiles->clearTagCache();
        m_tvFiles->setRootIndex(curIndex);
        m_lstFiles->setRootIndex(curIndex);
    }
    m_tvFiles->viewport()->update();
    m_lstFiles->viewport()->update();

    // 标签可能刚被编辑过：同步刷新【当前目录标签】面板（强制重扫）
    emit sendDirLabels(m_sCurDir, QSet<QString>());
    QTimer::singleShot(100, this, [this]() { scanDirLabels(m_sCurDir, true); });
}









/**
 * 点了标签 chip 上的 ✕：把该标签从文件上删掉。
 * 走和"编辑标签"同一套流程：先记撤销现场，再写盘，失败时提示并刷新。
 */


/** 统一的重命名入口（右键菜单）。 */



bool FormPageFiles::eventFilter(QObject *watched, QEvent *event)
{
    if (m_lstFiles != nullptr && watched == m_lstFiles->viewport() && event->type() == QEvent::Leave) {
        if (m_delegate)
            m_delegate->setHoveredIndex(QModelIndex());
        m_lstFiles->viewport()->update();
    }

    // 从【标签库】把标签拖到文件列表 = 给文件打标签
    const bool bIsFileView = (m_tvFiles != nullptr && watched == m_tvFiles->viewport())
                             || (m_lstFiles != nullptr && watched == m_lstFiles->viewport());
    if (bIsFileView && (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove)) {
        auto *pDragEvent = static_cast<QDragMoveEvent *>(event);
        if (pDragEvent->mimeData() != nullptr
            && pDragEvent->mimeData()->hasFormat(QString::fromLatin1(MIME_FILEMARKER_LABEL))) {
            pDragEvent->setDropAction(Qt::CopyAction);
            pDragEvent->accept();
            return true;
        }
    } else if (bIsFileView && event->type() == QEvent::Drop) {
        auto *pDropEvent = static_cast<QDropEvent *>(event);
        if (pDropEvent->mimeData() != nullptr
            && pDropEvent->mimeData()->hasFormat(QString::fromLatin1(MIME_FILEMARKER_LABEL))) {
            const QString sPayload = QString::fromUtf8(
                pDropEvent->mimeData()->data(QString::fromLatin1(MIME_FILEMARKER_LABEL)));
            QStringList qLLabels;
            for (const QString &sLabel : sPayload.split(QLatin1Char('\n'))) {
                const QString sTrimmed = sLabel.trimmed();
                if (!sTrimmed.isEmpty() && !qLLabels.contains(sTrimmed))
                    qLLabels << sTrimmed;
            }

            QAbstractItemView *pView = nullptr;
            if (m_lstFiles != nullptr && watched == m_lstFiles->viewport())
                pView = m_lstFiles;
            else
                pView = m_tvFiles;
            const QModelIndex dropIndex = pView != nullptr ? pView->indexAt(pDropEvent->pos()) : QModelIndex();
            pDropEvent->setDropAction(Qt::CopyAction);
            pDropEvent->accept();
            applyDroppedLabels(qLLabels, dropIndex);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
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
        qLTargets << m_modelFiles->filePath(dropIndex);
    } else {
        qLTargets = getSelFilePath();
    }
    qLTargets.removeAll(QString());

    if (qLTargets.isEmpty()) {
        emit sendHintMessage(tr("请先把要打标签的文件选中，或者把标签直接拖到某个文件上。"));
        return;
    }

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


QModelIndex FormPageFiles::currentIndexInActiveView() const
{
    if (m_lstFiles != nullptr && m_lstFiles->isVisible())
        return m_lstFiles->currentIndex();
    return m_tvFiles->currentIndex();
}


