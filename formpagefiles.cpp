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
#include "thumbnailiconprovider.h"
#include "editablefiledelegate.h"
#include "tageditdialog.h"
#include "utility.h"

namespace {

// 地址栏下拉里保留的历史目录条数
const int RECENT_DIR_LIMIT = 10;

// 虚拟根节点（“我的电脑”）的显示名：QFileSystemModel::myComputer() 返回的是英文
// "My Computer"，和界面上的名称不同，这里统一使用界面名并兼容英文写法。
const QString MY_COMPUTER_NAME = QStringLiteral("我的电脑");

bool isMyComputerName(const QString &sName)
{
    const QString s = sName.trimmed();
    return s.isEmpty()
           || s.compare(MY_COMPUTER_NAME, Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("My Computer"), Qt::CaseInsensitive) == 0;
}

// Windows 下同一路径可能以不同分隔符/盘符大小写出现（模型 vs QDir），统一比较
bool isSameDirPath(const QString &sLeft, const QString &sRight)
{
    if (sLeft.isEmpty() || sRight.isEmpty())
        return sLeft == sRight;
    return QDir::cleanPath(sLeft).compare(QDir::cleanPath(sRight), Qt::CaseInsensitive) == 0;
}

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
        if (isSameDirPath(sPath, m_sCurDir))
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
            // 旧版本保存的状态可能把列模式/表头可点击性也恢复成旧值，这里重申当前策略
            QHeaderView *header = m_tvFiles->header();
            header->setSectionsClickable(true);
            header->setSectionResizeMode(0, QHeaderView::Stretch);
            header->setSectionResizeMode(1, QHeaderView::Interactive);
            header->setSectionResizeMode(2, QHeaderView::Interactive);
            header->setSectionResizeMode(3, QHeaderView::Interactive);
            header->setSectionResizeMode(4, QHeaderView::Interactive);
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

void FormPageFiles::restoreLastDir()
{
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    const QString sLastDir = settings.value(QStringLiteral("view/lastDir")).toString();
    if (sLastDir.isEmpty() || isMyComputerName(sLastDir))
        return;
    if (QDir(sLastDir).exists())
        navigateToDir(sLastDir);
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
    if (qLSelFilePaths.isEmpty())
        return;

    // 删除标签是不可撤销的破坏性操作：先让用户确认，避免误点工具栏就清空全部标签。
    QMessageBox box(this);
    box.setWindowTitle(tr("删除标签"));
    box.setIcon(QMessageBox::Warning);
    // 确认框里列出具体文件（最多 5 个），避免"选中的文件已经不在当前视图里"时
    // 用户不知道到底要删哪些文件的标签。
    QString sFileList = qLSelFilePaths.mid(0, 5).join(QStringLiteral("\n"));
    if (qLSelFilePaths.size() > 5)
        sFileList += tr("\n…另有 %1 个文件").arg(qLSelFilePaths.size() - 5);
    box.setText(tr("将删除以下 %1 个文件的全部标签，且无法撤销：\n\n%2\n\n是否继续？")
                    .arg(qLSelFilePaths.size()).arg(sFileList));
    QPushButton *pBtnDelete = box.addButton(tr("删除"), QMessageBox::DestructiveRole);
    QPushButton *pBtnCancel = box.addButton(tr("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(pBtnCancel);
    box.exec();
    if (box.clickedButton() != pBtnDelete)
        return;

    // 删除前广播一次，供上层记录"删除之前的标签"用于撤销
    emit sendBeforeTagsChanged(qLSelFilePaths);

    bool bAllOk = true;
    QStringList qLFailedFiles;          // 把失败的文件名带回给用户
    foreach(const QString &sSelFilePath, qLSelFilePaths)
    {
        if (!ADSOperation::deleteADSFiles(sSelFilePath)) {
            bAllOk = false;
            qLFailedFiles << QFileInfo(sSelFilePath).fileName();
        }
    }
    emit sendTagsChanged(qLSelFilePaths);
    onBtnRefreshClicked();

    if (!bAllOk) {
        const QString sList = qLFailedFiles.mid(0, 5).join(QStringLiteral("\n"));
        const QString sMore = qLFailedFiles.size() > 5
                                  ? tr("\n…另有 %1 个文件").arg(qLFailedFiles.size() - 5)
                                  : QString();
        QMessageBox::warning(this, tr("删除标签失败"),
                             tr("以下 %1 个文件的标签未能删除（可能是文件被占用、只读或权限不足）：\n\n%2%3")
                                 .arg(qLFailedFiles.size()).arg(sList).arg(sMore));
    }
}

void FormPageFiles::initDetailFileList()
{
    m_tvFiles = ui->tvFiles;
     // 设置可以选中多行
    m_tvFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tvFiles->setModel(m_modelFiles);
    // 第 0~3 列装"拒绝编辑"代理，禁止 F2/双击把文件名改到磁盘上。
    for (int nCol = 0; nCol <= 3; ++nCol)
        m_tvFiles->setItemDelegateForColumn(nCol, new NoEditDelegate(m_tvFiles));
    // 标签列也不再就地编辑：双击标签单元格改为弹出"增删式"标签对话框
    // （在 onDoubleClicked 里处理），与缩略图的双击语义统一。
    m_tvFiles->setItemDelegateForColumn(4, new NoEditDelegate(m_tvFiles));
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
    // 名称列吃掉剩余宽度，其余数据列固定宽度，标签列给一个可用宽度并可拖动。
    // 这样窗口较窄时优先压缩名称列，而不是把“标签”列挤出可视区（默认 994x663 时
    // 以前必须横向滚动才能看到标签）。
    treeHeader->setSectionResizeMode(0, QHeaderView::Stretch);
    // 其余列保持可拖动（之前设为 Fixed 会让用户无法再调整大小/类型/日期列宽）
    treeHeader->setSectionResizeMode(1, QHeaderView::Interactive);
    treeHeader->setSectionResizeMode(2, QHeaderView::Interactive);
    treeHeader->setSectionResizeMode(3, QHeaderView::Interactive);
    treeHeader->setSectionResizeMode(4, QHeaderView::Interactive);
    treeHeader->resizeSection(1, fm.horizontalAdvance(QLatin1String("128.88 GB")) + 15);
    treeHeader->resizeSection(2, fm.horizontalAdvance(QLatin1String("mp3Folder")) + 35);
    treeHeader->resizeSection(3, fm.horizontalAdvance(QLatin1String("10/29/1981 02:02PM")) + 5);
    treeHeader->resizeSection(4, 180);
    treeHeader->setContextMenuPolicy(Qt::ActionsContextMenu);

    m_sCurDir = MY_COMPUTER_NAME;
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

    // 缩略图不再用"双击文件名/F2 就地重命名"：双击统一为"打开/进入"，
    // 标签编辑走双击标签区（弹对话框），重命名走右键菜单。
    m_lstFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);

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
    m_skBackward.push_back(m_sCurDir);
    m_skForward.clear();
    m_sCurDir = sCurDir;
    // 同步地址栏并把路径记入下拉历史
    rememberPath(sCurDir);
    // 导航栈变化时直接更新按钮状态，不再依赖 event() 拦截 Paint。
    updateToolButtons();

    // 进入目录后立即更新【当前目录标签】面板：先清空旧内容，再在文件模型加载完成后扫描。
    updateDirLabelsPanel(sCurDir);
}

void FormPageFiles::updateDirLabelsPanel(const QString &sDir)
{
    emit sendDirLabels(sDir, QSet<QString>());
    // 这里必须强制重扫：发送空集已经把面板清空了，如果再用"非强制"方式调用，
    // 对"本会话已扫过一遍的同一目录"会被 m_sScanCompletedDir 守卫直接跳过，
    // 结果面板清空后永远不再填充（实测：对同一路径再按一次回车即可复现）。
    QTimer::singleShot(150, this, [this, sDir]() { scanDirLabels(sDir, true); });
}

void FormPageFiles::scanDirLabels(const QString &sDir, bool bForce)
{
    if (sDir.isEmpty() || !isSameDirPath(sDir, m_sCurDir))
        return;
    // 已经完整扫过且不是强制刷新（例如 directoryLoaded 的重复通知）就不再扫
    if (!bForce && m_sScanCompletedDir == sDir)
        return;

    const QModelIndex rootIndex = m_modelFiles->index(sDir);
    if (!rootIndex.isValid())
        return;

    const int nRows = m_modelFiles->rowCount(rootIndex);
    if (nRows <= 0)
        return;   // 目录还没加载出内容，等 directoryLoaded 或下一次刷新

    m_sScanDir = sDir;
    m_scanRootIndex = rootIndex;
    m_nScanRow = 0;
    m_nScanTotal = qMin(nRows, 2000);
    m_setScanLabels.clear();
    m_sScanCompletedDir.clear();

    if (m_pScanTimer != nullptr)
        m_pScanTimer->start();
}

void FormPageFiles::continueDirLabelScan()
{
    if (!isSameDirPath(m_sScanDir, m_sCurDir)) {
        if (m_pScanTimer != nullptr)
            m_pScanTimer->stop();
        return;
    }

    // 每片最多约 15ms（实测约 0.37ms/文件，即每片约 40 个文件），保证界面仍可响应
    QElapsedTimer budget;
    budget.start();
    while (m_nScanRow < m_nScanTotal && budget.elapsed() < 15) {
        const QModelIndex idx = m_modelFiles->index(m_nScanRow, 0, m_scanRootIndex);
        ++m_nScanRow;
        if (!idx.isValid() || m_modelFiles->isDir(idx))
            continue;
        const QStringList qLTags = ADSOperation::listADSTagsInOrder(m_modelFiles->filePath(idx));
        for (const QString &sTag : qLTags)
            m_setScanLabels.insert(sTag);
        // 预热模型缓存：标签列绘制时直接命中，避免首次 paint 同步枚举 ADS
        m_modelFiles->cacheTagsForFile(m_modelFiles->filePath(idx), qLTags);
    }

    if (m_nScanRow < m_nScanTotal)
        return;

    if (m_pScanTimer != nullptr)
        m_pScanTimer->stop();

    if (!isSameDirPath(m_sScanDir, m_sCurDir))
        return;

    emit sendDirLabels(m_sScanDir, m_setScanLabels);
    m_sScanCompletedDir = m_sScanDir;

    // 目录文件数超过上限时明确提示，避免用户误以为面板内容完整
    if (m_modelFiles->rowCount(m_scanRootIndex) > m_nScanTotal) {
        emit sendHintMessage(tr("目录文件过多，仅统计了前 %1 个文件的标签").arg(m_nScanTotal));
    }
}

// 把地址栏文本恢复成当前目录（用户输入非法路径、或导航失败时使用）
void FormPageFiles::refreshAddressBar()
{
    ui->lookInCombo->setCurrentIndex(-1);
    ui->lookInCombo->setEditText(m_sCurDir);
    if (ui->lookInCombo->lineEdit())
        ui->lookInCombo->lineEdit()->setCursorPosition(0);
}

void FormPageFiles::rememberPath(const QString &sPath)
{
    if (sPath.isEmpty())
        return;

    // 最近访问的 10 个目录：去重、最近的在最前面、写回 settings
    const QString sClean = QDir::cleanPath(sPath);
    m_qLRecentDirs.removeAll(sClean);
    m_qLRecentDirs.prepend(sClean);
    while (m_qLRecentDirs.size() > RECENT_DIR_LIMIT)
        m_qLRecentDirs.removeLast();
    saveRecentDirs();

    // 用历史列表重建下拉项；地址栏文本单独设置，避免把"当前项"高亮成列表里的一项
    ui->lookInCombo->blockSignals(true);
    ui->lookInCombo->clear();
    ui->lookInCombo->addItems(m_qLRecentDirs);
    ui->lookInCombo->setCurrentIndex(-1);
    ui->lookInCombo->setEditText(sClean);
    ui->lookInCombo->blockSignals(false);
}

/** 读取地址栏下拉历史（最多 10 条），并填充到 QComboBox。 */
void FormPageFiles::loadRecentDirs()
{
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif

    const QStringList qLStored = settings.value(QStringLiteral("view/recentDirs")).toStringList();
    m_qLRecentDirs.clear();
    for (const QString &sDir : qLStored) {
        if (m_qLRecentDirs.size() >= RECENT_DIR_LIMIT)
            break;
        const QString sClean = QDir::cleanPath(sDir);
        if (!sClean.isEmpty() && !m_qLRecentDirs.contains(sClean))
            m_qLRecentDirs << sClean;
    }

    ui->lookInCombo->blockSignals(true);
    ui->lookInCombo->clear();
    ui->lookInCombo->addItems(m_qLRecentDirs);
    ui->lookInCombo->setCurrentIndex(-1);
    ui->lookInCombo->blockSignals(false);
}

/** 把地址栏下拉历史写回 settings。 */
void FormPageFiles::saveRecentDirs() const
{
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    settings.setValue(QStringLiteral("view/recentDirs"), m_qLRecentDirs);
}

void FormPageFiles::navigateToDir(const QString &sPath)
{
    QString sTarget = sPath.trimmed();
    // 支持直接粘贴资源管理器的带引号路径
    if (sTarget.size() >= 2 && sTarget.startsWith(QLatin1Char('"')) && sTarget.endsWith(QLatin1Char('"')))
        sTarget = sTarget.mid(1, sTarget.size() - 2).trimmed();
    if (sTarget.isEmpty()) {
        refreshAddressBar();
        return;
    }
    sTarget.replace(QLatin1Char('/'), QLatin1Char('\\'));

    // “我的电脑”是虚拟根节点，不是真实目录，不能走 QDir::exists 校验。
    // 注意 QFileSystemModel::myComputer() 返回的是英文 "My Computer"，所以两种名字都接受。
    if (isMyComputerName(sTarget)) {
        bothSetRootIndex(m_modelFiles->index(QString()));
        curDirChanged(MY_COMPUTER_NAME);
        return;
    }

    // 输入的是文件而不是目录：进入其所在目录并选中该文件
    const QFileInfo targetInfo(sTarget);
    if (targetInfo.exists() && targetInfo.isFile()) {
        const QString sParentDir = targetInfo.absolutePath();
        const QModelIndex parentIndex = m_modelFiles->index(sParentDir);
        if (!parentIndex.isValid()) {
            QMessageBox::warning(this, tr("无法打开路径"),
                                 tr("无法定位到该目录：\n%1").arg(sParentDir));
            refreshAddressBar();
            return;
        }
        bothSetRootIndex(parentIndex);
        curDirChanged(QDir::cleanPath(sParentDir));
        const QModelIndex fileIndex = m_modelFiles->index(targetInfo.absoluteFilePath());
        if (fileIndex.isValid()) {
            m_tvFiles->setCurrentIndex(fileIndex);
            m_lstFiles->setCurrentIndex(fileIndex);
            m_tvFiles->scrollTo(fileIndex, QAbstractItemView::PositionAtCenter);
            m_lstFiles->scrollTo(fileIndex, QAbstractItemView::PositionAtCenter);
        }
        return;
    }

    const QDir dir(sTarget);
    if (!dir.exists()) {
        QMessageBox::warning(this, tr("无法打开路径"),
                             tr("路径不存在或无法访问：\n%1").arg(sPath));
        refreshAddressBar();
        return;
    }

    const QString sAbs = QDir::cleanPath(dir.absolutePath());
    const QModelIndex targetIndex = m_modelFiles->index(sAbs);
    if (!targetIndex.isValid()) {
        QMessageBox::warning(this, tr("无法打开路径"),
                             tr("无法定位到该目录：\n%1").arg(sAbs));
        refreshAddressBar();
        return;
    }

    bothSetRootIndex(targetIndex);
    curDirChanged(sAbs);
}

void FormPageFiles::onLookInComboReturnPressed()
{
    navigateToDir(ui->lookInCombo->currentText());
}

// 不再需要拦截 Paint 事件来更新工具按钮。
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

    connect(actionOpen, &QAction::triggered, this, [targetFile]() {
        if (!targetFile.isEmpty())
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

void FormPageFiles::onBackButtonClicked()
{
    if (m_skBackward.isEmpty())
        return;
    m_skForward.push_back(m_sCurDir);
    QString sDir = m_skBackward.pop();
    if (sDir.isEmpty())
        sDir = MY_COMPUTER_NAME;
    bothSetRootIndex(m_modelFiles->index(sDir));
    m_sCurDir = sDir;
    refreshAddressBar();
    updateToolButtons();  // 导航栈变化后直接更新按钮
    updateDirLabelsPanel(sDir);
}

void FormPageFiles::onForwardButtonClicked()
{
    if (m_skForward.isEmpty())
        return;
    m_skBackward.push_back(m_sCurDir);
    QString sDir = m_skForward.pop();
    if (sDir.isEmpty())
        sDir = MY_COMPUTER_NAME;
    bothSetRootIndex(m_modelFiles->index(sDir));
    m_sCurDir = sDir;
    refreshAddressBar();
    updateToolButtons();  // 导航栈变化后直接更新按钮
    updateDirLabelsPanel(sDir);
}

void FormPageFiles::onToParentButtonClicked()
{
    QModelIndex rootIndex = m_tvFiles->rootIndex();
    QString sFilePath = m_modelFiles->filePath(rootIndex);
    QDir dir(sFilePath);
    QString sNewDirectory;
    if (sFilePath.isEmpty() || dir.isRoot())
    {
        // 已经到盘符根：再上一级是虚拟根“我的电脑”，用空路径表示
        sNewDirectory = MY_COMPUTER_NAME;
        bothSetRootIndex(m_modelFiles->index(QString()));
    }
    else
    {
        dir.cdUp();
        sNewDirectory = dir.absolutePath();
        bothSetRootIndex(m_modelFiles->index(sNewDirectory));
    }
    // 根节点没有“父路径”，不要把空字符串压进后退栈
    if (!sFilePath.isEmpty())
        m_skBackward.push_back(sFilePath);
    m_sCurDir = sNewDirectory;
    // 地址栏同步：之前这里漏了更新，导致点上移后地址栏还停留在旧路径。
    rememberPath(sNewDirectory);
    updateToolButtons();  // 导航栈变化后直接更新按钮
    updateDirLabelsPanel(sNewDirectory);
}

void FormPageFiles::onTvNavigationClicked(const QModelIndex &index)
{
    QString path = index.data(QFileSystemModel::FilePathRole).toString();
    // 详情视图和缩略图视图共用模型，根节点必须一起切，否则切到缩略图还是旧目录。
    bothSetRootIndex(m_modelFiles->index(path));
    ui->stackedWidget->setCurrentIndex(FILEPAGE::DETAIL);
    curDirChanged(path);
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
        m_commandBar->addAction(pAction);
    }
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

void FormPageFiles::recvGotoFile(QString sFilePath)
{
    // 用 QFileInfo 取父目录，正确处理盘符根（如 C:/file.txt 下 lastIndexOf('/') 会失效）。
    QFileInfo fi(sFilePath);
    const QString sFileDir = QDir::cleanPath(fi.absolutePath());
    const QModelIndex dirIndex = m_modelFiles->index(sFileDir);
    // 使用 bothSetRootIndex 同时更新详细视图和缩略图视图，
    // 否则缩略图模式下跳转后仍停留在旧目录。
    bothSetRootIndex(dirIndex);

    // "当前目录"的状态有 5 份副本（两个视图的 rootIndex、m_sCurDir、地址栏、
    // 后退/前进栈、【当前目录中的标签】面板）。原来这里只改了 rootIndex，于是：
    // 地址栏和标签面板还显示旧目录，点【刷新】又按 m_sCurDir 把列表弹回旧目录。
    // 现在统一走 curDirChanged()，只有"本来就在这个目录"时才只同步地址栏，避免重复压后退栈。
    if (!isSameDirPath(sFileDir, m_sCurDir)) {
        curDirChanged(sFileDir);
    } else {
        refreshAddressBar();
        updateToolButtons();
    }

    const QModelIndex fileIndex = m_modelFiles->index(fi.absoluteFilePath());
    if (fileIndex.isValid()) {
        m_tvFiles->setCurrentIndex(fileIndex);
        m_lstFiles->setCurrentIndex(fileIndex);
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

void FormPageFiles::onDoubleClicked(const QModelIndex &index)
{
    // 第 5 列（标签列）的双击语义是"编辑标签"，不能同时打开文件/进入目录。
    // 改为弹出"增删式"标签对话框（不再就地编辑整行文本）。
    // 说明：0aead52 之前该列的 FilePathRole 会误返回标签字符串，openUrl 打开失败，
    // 看起来像“没有打开文件”；修正 FilePathRole 后必须在这里显式排除标签列。
    if (index.column() == 4) {
        onEditTag(index);
        return;
    }

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
    // 换目录时必须清掉选择集：两个视图共用同一个 QItemSelectionModel，
    // 而 setRootIndex() 不会清理它。不清的话，切到别的目录后工具栏【删除标签】/【添加标签】
    // 仍会作用到旧目录里"已经看不见"的文件（实测：不可见文件的标签被真实删除）。
    if (m_tvFiles->selectionModel() != nullptr)
        m_tvFiles->selectionModel()->clearSelection();
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
    const QStringList adsNames = ADSOperation::listADSTagsInOrder(sFilePath);

    // 改用"列表 + 添加/移除"的编辑器：
    // 以前是把全部标签当成一行文本编辑，用户全选后输入新名字就等于静默替换掉所有旧标签；
    // 现在每个标签都是列表里的一项，增删都要显式操作，且非法名字在添加时就被拒绝。
    TagEditDialog dlg(adsNames, QFileInfo(sFilePath).fileName(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QStringList qLLabels = dlg.tags();
    if (qLLabels == adsNames)   // 没有改动就不要重写
        return;

    emit sendBeforeTagsChanged(QStringList() << sFilePath);   // 供"撤销上一次标签操作"
    const bool bOk = ADSOperation::replaceTags(sFilePath, qLLabels);
    emit sendTagsChanged(QStringList() << sFilePath);
    onBtnRefreshClicked();

    if (!bOk) {
        QMessageBox::warning(this, tr("保存标签失败"),
                             tr("标签未能完整写入：%1\n"
                                "已尝试恢复操作前的标签，请检查文件是否被占用、只读或磁盘空间不足。")
                                 .arg(sFilePath));
        return;
    }

    emit sendLabels(sFilePath, qLLabels.join(","));
}

void FormPageFiles::recvDblClick(const QModelIndex &index)
{
    onDoubleClicked(index);
}

/** 统一的重命名入口（右键菜单）。 */
void FormPageFiles::renameFile(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    const QModelIndex nameIndex = index.sibling(index.row(), 0);
    const QString sOldName = m_modelFiles->fileName(nameIndex);
    if (sOldName.isEmpty())
        return;

    QInputDialog dlg(this);
    dlg.setWindowTitle(tr("重命名"));
    dlg.setLabelText(tr("新名称（含扩展名）："));
    dlg.setInputMode(QInputDialog::TextInput);
    dlg.setTextValue(sOldName);
    dlg.setOkButtonText(tr("重命名"));
    dlg.setCancelButtonText(tr("取消"));
    dlg.setMinimumWidth(520);
    dlg.resize(520, dlg.sizeHint().height());
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString sNewName = dlg.textValue().trimmed();
    if (sNewName.isEmpty() || sNewName == sOldName)
        return;

    if (!m_modelFiles->setData(nameIndex, sNewName, Qt::EditRole)) {
        QMessageBox::warning(this, tr("重命名失败"),
                             tr("无法把【%1】改成【%2】。\n"
                                "可能是名称不合法、已经存在同名文件，或文件正被占用。")
                                 .arg(sOldName, sNewName));
    }
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


