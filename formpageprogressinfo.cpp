#include "formpageprogressinfo.h"
#include "ui_formpageprogressinfo.h"

#include <QShortcut>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QToolButton>
#include <QTextCursor>
#include <QTreeView>

namespace {

const int kDirProgressColumns = 5;   // 目录 / 状态 / 已扫 / 命中 / 操作
const int kMaxDisplayDepth = 3;      // 只显示相对搜索根的 3 层

QString stateText(DirNodeState state)
{
    switch (state) {
    case DirNodeState::Waiting:    return QObject::tr("等待");
    case DirNodeState::Searching:  return QObject::tr("● 搜索中");
    case DirNodeState::Complete:   return QObject::tr("✓ 完成");
    case DirNodeState::Stopped:    return QObject::tr("■ 已停止");
    case DirNodeState::Skipped:    return QObject::tr("⊘ 已跳过");
    case DirNodeState::Incomplete: return QObject::tr("! 不完整");
    }
    return QString();
}

QColor stateColor(DirNodeState state)
{
    switch (state) {
    case DirNodeState::Waiting:    return QColor(0x88, 0x88, 0x88);
    case DirNodeState::Searching:  return QColor(0x1a, 0x73, 0xe8);
    case DirNodeState::Complete:   return QColor(0x1e, 0x8e, 0x3e);
    case DirNodeState::Stopped:    return QColor(0xc5, 0x22, 0x1f);
    case DirNodeState::Skipped:    return QColor(0x5f, 0x63, 0x68);
    case DirNodeState::Incomplete: return QColor(0xb0, 0x60, 0x00);
    }
    return QColor(Qt::black);
}

/** 这个状态下，操作列提供什么按钮 */
QString actionText(DirNodeState state)
{
    switch (state) {
    case DirNodeState::Searching:
        return QObject::tr("停止");     // 正在扫：只停这一棵子树
    case DirNodeState::Waiting:
    case DirNodeState::Incomplete:
        return QObject::tr("跳过");     // 还没轮到 / 只扫了一部分：可以跳过剩下的
    case DirNodeState::Complete:
    case DirNodeState::Stopped:
    case DirNodeState::Skipped:
        // 已经扫完的目录再"跳过"没有任何意义（徒增误解），已经停/跳过的也没有操作可做
        return QString();
    }
    return QString();
}

/**
 * 目录节点键：cleanPath + 去掉结尾斜杠。
 * 必须和 ThreadSearch::dirNodeKey() 一致：QFileInfo("E:/bin").path() 在 Qt 里返回的是
 * "E:/"（带斜杠），不统一的话父节点会另建一个 "E:/"，界面上就出现两个 "E:\" 根节点。
 */
QString normalizedDirKey(const QString &sPath)
{
    QString sKey = QDir::cleanPath(sPath);
    while (sKey.size() > 1 && sKey.endsWith(QLatin1Char('/')))
        sKey.chop(1);
    return sKey;
}

const int kRolePath = Qt::UserRole;
const int kRoleState = Qt::UserRole + 1;
const int kRoleSubDirs = Qt::UserRole + 2;   // 已聚合的深层子目录数（避免每次刷新都改 tooltip）

}   // namespace

FormPageProgressInfo::FormPageProgressInfo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormPageProgressInfo)
{
    ui->setupUi(this);

    ui->plainTextEdit->setMaximumBlockCount(100);
    // 原来那一路滚动的文件路径不再需要：进度用目录树表达，"当前文件"放在顶部一行
    ui->plainTextEdit->hide();

    // 显式连接停止按钮（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(ui->btnStop, &QPushButton::clicked, this, &FormPageProgressInfo::onBtnStopClicked);

    // Esc = 停止全部：只在任务进行中启用。
    // 用 WindowShortcut 而不是 WidgetWithChildren —— 搜索开始后键盘焦点可能还留在
    // 文件列表/工具栏（进度页只是被切换显示），WidgetWithChildren 会收不到 Esc。
    // 因为只在 running 时 enable，不会干扰其它页面的 Esc。
    m_stopShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_stopShortcut->setContext(Qt::WindowShortcut);
    connect(m_stopShortcut, &QShortcut::activated, this, &FormPageProgressInfo::onBtnStopClicked);
    setRunning(false);

    // 【← 返回目录】：只在任务被中断、停在总结页时显示
    m_btnBack = new QToolButton(this);
    m_btnBack->setObjectName(QStringLiteral("btnBackToDir"));
    m_btnBack->setText(tr("← 返回目录"));
    m_btnBack->setAutoRaise(true);
    m_btnBack->setCursor(Qt::PointingHandCursor);
    m_btnBack->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_btnBack->setFocusPolicy(Qt::NoFocus);
    m_btnBack->hide();
    connect(m_btnBack, &QToolButton::clicked, this, &FormPageProgressInfo::sendBackRequested);
    // 和【停止全部】放在同一个格子（左上）：任务结束后停止按钮隐藏，
    // 【← 返回目录】就出现在左侧，位置与搜索结果的"返回目录"一致。
    ui->gridLayout_4->addWidget(m_btnBack, 1, 0, Qt::AlignLeft);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statusLabel->setTextFormat(Qt::PlainText);
    // 关键：横向尺寸策略设成 Ignored。
    // 否则标签会按"最长的那条路径"撑出最小宽度，搜索过程中路径一变长变短，
    // 整个页面（乃至窗口）就跟着来回变大小 —— 就是用户看到的"窗口快速变化大小"。
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_statusLabel->setMinimumWidth(0);
    ui->gridLayout->addWidget(m_statusLabel, 1, 0);

    initTree();
}

FormPageProgressInfo::~FormPageProgressInfo()
{
    delete ui;
}

void FormPageProgressInfo::initTree()
{
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels(QStringList() << tr("目录") << tr("状态")
                                                     << tr("已扫") << tr("命中") << tr("操作"));

    m_tree = new QTreeView(this);
    m_tree->setModel(m_model);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);      // 行高一致，几万行也不会慢
    m_tree->setAlternatingRowColors(true);
    // 允许选中一行：选中后按回车等同于点该行的"停止/跳过"（键盘可达，也方便验证）
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_tree->header()->resizeSection(4, 70);

    connect(m_tree, &QTreeView::clicked, this, &FormPageProgressInfo::onTreeClicked);
    // 选中一行按回车 = 点该行的"停止/跳过"。
    // 注意不能再用 m_model->item(row) 去找行：那是顶层行号，子目录会取错行
    // （实测误把根节点停掉）。路径直接取自 index 的自定义角色即可。
    connect(m_tree, &QTreeView::activated, this, [this](const QModelIndex &index) {
        if (!index.isValid())
            return;
        onActionClicked(index.data(kRolePath).toString());
    });

    // 顶部是停止按钮/状态行（第 0 行），状态行第 1 行，进度树第 2 行
    ui->gridLayout->addWidget(m_tree, 2, 0);
}

void FormPageProgressInfo::beginAIPrepare()
{
    clear();                 // 上次搜索/遍历残留的树和计数一并清掉
    m_bAiMode = true;
    m_bAiPrepare = true;
    m_sAiStage = tr("检查环境…");
    m_nAiDone = 0;
    ui->plainTextEdit->clear();
    ui->plainTextEdit->setVisible(true);   // AI 期间用日志区显示安装/下载过程
    if (m_tree != nullptr)
        m_tree->setVisible(false);
    setRunning(true);
    updateStatusLabel();
}

void FormPageProgressInfo::setAIPrepareStage(const QString &sStage)
{
    m_sAiStage = sStage;
    updateStatusLabel();
}

void FormPageProgressInfo::appendAILog(const QString &sLine)
{
    if (sLine.isEmpty())
        return;
    ui->plainTextEdit->setVisible(true);
    ui->plainTextEdit->appendPlainText(sLine);
    ui->plainTextEdit->moveCursor(QTextCursor::End);
}

void FormPageProgressInfo::beginAITask()
{
    clear();                 // 上次搜索/遍历残留的树和计数一并清掉
    m_bAiMode = true;
    m_bAiPrepare = false;
    m_nAiDone = 0;
    ui->plainTextEdit->setVisible(true);   // 打标过程也把每行结果留在日志区
    if (m_tree != nullptr)
        m_tree->setVisible(false);
    setRunning(true);        // 让【停止全部】和 Esc 在 AI 打标期间可用
    updateStatusLabel();
}

void FormPageProgressInfo::setAIProgress(int nDone)
{
    if (!m_bAiMode)
        return;
    m_nAiDone = nDone;
    updateStatusLabel();
}

void FormPageProgressInfo::resetProgress(const QStringList &qLSelDirs)
{
    m_bAiMode = false;
    m_nAiDone = 0;
    m_bFinished = false;
    m_sFinishSummary.clear();
    if (m_btnBack != nullptr)
        m_btnBack->hide();
    setRunning(true);   // 开始跑：显示【停止全部】、启用 Esc
    if (m_tree != nullptr)
        m_tree->setFocus();   // 键盘可达：进页就能用 Esc/回车
    m_items.clear();
    m_stoppedByUser.clear();
    m_skippedByUser.clear();
    m_model->removeRows(0, m_model->rowCount());
    m_sCurrentFile.clear();

    m_nScanned = 0;
    m_nMatched = 0;

    // 和 ThreadSearch 用同一套节点键（去尾斜杠）：否则这里建的是 "E:/"，
    // 线程那边报上来的是 "E:"，界面上会出现两个 "E:\" 根节点。
    for (const QString &sDir : qLSelDirs) {
        QString sKey = QDir::cleanPath(sDir);
        while (sKey.size() > 1 && sKey.endsWith(QLatin1Char('/')))
            sKey.chop(1);

        DirProgressNode node;
        node.path = sKey;
        node.level = 0;
        node.state = DirNodeState::Waiting;
        applyNode(node);
    }

    // 默认保持收起：只把根节点展开一层，让用户看到要扫的顶层目录；
    // 具体展开到"当前搜索的文件夹"由 focusOnDirectory() 负责。
    m_tree->collapseAll();
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const QModelIndex rootIndex = m_model->index(i, 0);
        if (rootIndex.isValid())
            m_tree->expand(rootIndex);
    }
    updateStatusLabel();
}

QStandardItem *FormPageProgressInfo::ensureItem(const QString &sPath, int nLevel)
{
    if (nLevel < 0 || nLevel > kMaxDisplayDepth)
        return nullptr;

    const QString sKey = normalizedDirKey(sPath);
    const auto existed = m_items.constFind(sKey);
    if (existed != m_items.constEnd())
        return existed.value();

    QList<QStandardItem *> items;
    for (int i = 0; i < kDirProgressColumns; ++i)
        items << new QStandardItem();

    // 显示名：搜索根显示完整路径，其它层只显示目录名（和资源管理器一致）
    // 搜索根显示完整路径；盘符根本身是 "E:"，补个斜杠显示成 "E:\"
    QString sName;
    if (nLevel == 0) {
        sName = QDir::toNativeSeparators(sKey);
        if (sName.endsWith(QLatin1Char(':')))
            sName += QLatin1Char('\\');
    } else {
        sName = QFileInfo(sKey).fileName();
    }
    items[0]->setText(sName);
    items[0]->setData(sKey, kRolePath);
    items[0]->setToolTip(QDir::toNativeSeparators(sKey));
    for (int i = 1; i < kDirProgressColumns; ++i)
        items[i]->setData(sKey, kRolePath);

    QStandardItem *parentItem = nullptr;
    if (nLevel > 0) {
        // 快照里的节点顺序不保证父在前，所以父节点不存在就现建（递归向上补链），
        // 否则子节点会被挂到根上，层级就乱了。
        parentItem = ensureItem(normalizedDirKey(QFileInfo(sKey).path()), nLevel - 1);
    }

    if (parentItem != nullptr)
        parentItem->appendRow(items);
    else
        m_model->invisibleRootItem()->appendRow(items);

    m_items.insert(sKey, items[0]);

    // 注意：这里**不自动展开**父节点。整盘搜索时树会一直长，
    // 全展开既乱又卡；展开/定位统一由 focusOnDirectory() 按"当前搜索目录"处理。
    return items[0];
}

void FormPageProgressInfo::applyNode(const DirProgressNode &node)
{
    QStandardItem *nameItem = ensureItem(node.path, node.level);
    if (nameItem == nullptr)
        return;

    // 用户点过【停止】/【跳过】的目录（或其子树）：即使线程后续报"完成/等待"，
    // 也保持用户点出来的状态，避免看起来像没生效。
    // 停止优先（同时点过时显示"已停止"，语义更明确）。
    DirNodeState effectiveState = node.state;
    if (isSkippedByUser(node.path))
        effectiveState = DirNodeState::Skipped;
    if (isStoppedByUser(node.path))
        effectiveState = DirNodeState::Stopped;

    QStandardItem *parentItem = nameItem->parent();
    if (parentItem == nullptr)
        parentItem = m_model->invisibleRootItem();
    const int nRow = nameItem->row();

    QStandardItem *stateItem = parentItem->child(nRow, 1);
    QStandardItem *filesItem = parentItem->child(nRow, 2);
    QStandardItem *matchItem = parentItem->child(nRow, 3);
    QStandardItem *actionItem = parentItem->child(nRow, 4);
    if (stateItem == nullptr || filesItem == nullptr || matchItem == nullptr || actionItem == nullptr)
        return;

    // 每 150ms 会把整棵树的快照刷一遍（最多几百个节点），这里逐项比较再赋值：
    // 值没变就不 setText/setData，避免无谓的 dataChanged → 视图重绘。
    const QString sStateText = stateText(effectiveState);
    if (stateItem->text() != sStateText) {
        stateItem->setText(sStateText);
        stateItem->setForeground(stateColor(effectiveState));
        QFont font = stateItem->font();
        font.setBold(effectiveState == DirNodeState::Searching);
        stateItem->setFont(font);
    }

    const QString sFiles = node.files > 0 ? QString::number(node.files) : QString();
    if (filesItem->text() != sFiles)
        filesItem->setText(sFiles);

    const QString sMatches = node.matches > 0 ? QString::number(node.matches) : QString();
    if (matchItem->text() != sMatches)
        matchItem->setText(sMatches);

    // 第 3 层节点下面还有更深目录时，提示被聚合的数量
    if (node.subDirs > 0 && nameItem->data(kRoleSubDirs).toInt() != node.subDirs) {
        nameItem->setData(node.subDirs, kRoleSubDirs);
        nameItem->setToolTip(tr("%1\n（含 %2 个子目录，已聚合计入本行）")
                                 .arg(QDir::toNativeSeparators(node.path))
                                 .arg(node.subDirs));
    }

    actionItem->setText(QString());   // 文字交给按钮显示，避免格子里"停止"出现两次
    actionItem->setData(int(effectiveState), kRoleState);

    // 操作列放真正的按钮（不是靠点单元格文字）：更直观，也方便自动化点它
    updateActionButton(actionItem, effectiveState, node.path);
}

/** 按状态给操作列装/更新一个按钮（没有可做的操作就撤掉按钮）。 */
void FormPageProgressInfo::updateActionButton(QStandardItem *actionItem, DirNodeState state,
                                              const QString &sPath)
{
    if (actionItem == nullptr)
        return;

    const QModelIndex index = actionItem->index();
    if (!index.isValid())
        return;

    const QString sText = actionText(state);
    auto *button = qobject_cast<QToolButton *>(m_tree->indexWidget(index));

    if (sText.isEmpty()) {
        if (button != nullptr)
            m_tree->setIndexWidget(index, nullptr);   // 已停止/已跳过：不再给按钮
        return;
    }

    if (button != nullptr && button->text() == sText && button->property("fmPath").toString() == sPath)
        return;   // 状态和路径都没变：不用重建连接（每 150ms 重建一次太浪费）

    if (button == nullptr) {
        button = new QToolButton(m_tree);
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("fmPath", sPath);
        m_tree->setIndexWidget(index, button);
    }
    button->setProperty("fmPath", sPath);
    button->setText(sText);
    button->setToolTip(state == DirNodeState::Searching
                           ? tr("停止搜索这个文件夹（它下面的子目录也一起停）")
                           : tr("跳过这个文件夹（整棵子树不搜索）"));
    // 每次重建连接，保证路径和当前状态是最新的
    disconnect(button, &QToolButton::clicked, nullptr, nullptr);
    connect(button, &QToolButton::clicked, this, [this, sPath]() { onActionClicked(sPath); });
}

/** 点了某一行的操作按钮：停止 / 跳过。 */
/** 该路径（或它的祖先）是否被用户点过【停止】。 */
bool FormPageProgressInfo::isStoppedByUser(const QString &sPath) const
{
    const QString sKey = normalizedDirKey(sPath);
    for (const QString &sStopped : m_stoppedByUser) {
        const QString sStoppedKey = normalizedDirKey(sStopped);
        if (sKey.compare(sStoppedKey, Qt::CaseInsensitive) == 0)
            return true;
        if (sKey.startsWith(sStoppedKey + QLatin1Char('/'), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

/** 立刻把某行及其已建出来的子行标成"已停止"（不等快照回来）。 */
void FormPageProgressInfo::markSubtreeStopped(const QString &sPath)
{
    const QString sKey = normalizedDirKey(sPath);
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        const QString sItemKey = normalizedDirKey(it.key());
        if (sItemKey.compare(sKey, Qt::CaseInsensitive) != 0
                && !sItemKey.startsWith(sKey + QLatin1Char('/'), Qt::CaseInsensitive)) {
            continue;
        }

        QStandardItem *nameItem = it.value();
        if (nameItem == nullptr)
            continue;
        QStandardItem *parentItem = nameItem->parent();
        if (parentItem == nullptr)
            parentItem = m_model->invisibleRootItem();
        QStandardItem *stateItem = parentItem->child(nameItem->row(), 1);
        QStandardItem *actionItem = parentItem->child(nameItem->row(), 4);
        if (stateItem != nullptr) {
            stateItem->setText(stateText(DirNodeState::Stopped));
            stateItem->setForeground(stateColor(DirNodeState::Stopped));
            QFont font = stateItem->font();
            font.setBold(false);
            stateItem->setFont(font);
        }
        if (actionItem != nullptr) {
            actionItem->setData(int(DirNodeState::Stopped), kRoleState);
            actionItem->setText(QString());
            updateActionButton(actionItem, DirNodeState::Stopped, it.key());
        }
    }
}

/** 该路径（或它的祖先）是否被用户点过【跳过】。 */
bool FormPageProgressInfo::isSkippedByUser(const QString &sPath) const
{
    const QString sKey = normalizedDirKey(sPath);
    for (const QString &sSkipped : m_skippedByUser) {
        const QString sSkippedKey = normalizedDirKey(sSkipped);
        if (sKey.compare(sSkippedKey, Qt::CaseInsensitive) == 0)
            return true;
        if (sKey.startsWith(sSkippedKey + QLatin1Char('/'), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

/** 立刻把某行及其已建出来的子行标成"已跳过"（不等快照回来）。 */
void FormPageProgressInfo::markSubtreeSkipped(const QString &sPath)
{
    const QString sKey = normalizedDirKey(sPath);
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        const QString sItemKey = normalizedDirKey(it.key());
        if (sItemKey.compare(sKey, Qt::CaseInsensitive) != 0
                && !sItemKey.startsWith(sKey + QLatin1Char('/'), Qt::CaseInsensitive)) {
            continue;
        }
        // 已经点过"停止"的行保持"已停止"（语义更强，不降级成跳过）
        if (isStoppedByUser(it.key()))
            continue;

        QStandardItem *nameItem = it.value();
        if (nameItem == nullptr)
            continue;
        QStandardItem *parentItem = nameItem->parent();
        if (parentItem == nullptr)
            parentItem = m_model->invisibleRootItem();
        QStandardItem *stateItem = parentItem->child(nameItem->row(), 1);
        QStandardItem *actionItem = parentItem->child(nameItem->row(), 4);
        if (stateItem != nullptr) {
            stateItem->setText(stateText(DirNodeState::Skipped));
            stateItem->setForeground(stateColor(DirNodeState::Skipped));
            QFont font = stateItem->font();
            font.setBold(false);
            stateItem->setFont(font);
        }
        if (actionItem != nullptr) {
            actionItem->setData(int(DirNodeState::Skipped), kRoleState);
            actionItem->setText(QString());
            updateActionButton(actionItem, DirNodeState::Skipped, it.key());
        }
    }
}

void FormPageProgressInfo::onActionClicked(const QString &sPath)
{
    if (sPath.isEmpty())
        return;

    // 状态从模型里取（按钮文字可能滞后于快照）
    const auto it = m_items.constFind(sPath);
    if (it == m_items.constEnd())
        return;

    QStandardItem *nameItem = it.value();
    QStandardItem *parentItem = nameItem->parent();
    if (parentItem == nullptr)
        parentItem = m_model->invisibleRootItem();
    QStandardItem *actionItem = parentItem->child(nameItem->row(), 4);
    if (actionItem == nullptr)
        return;

    const DirNodeState state = DirNodeState(actionItem->data(kRoleState).toInt());

    if (state == DirNodeState::Searching) {
        if (!isStoppedByUser(sPath))
            m_stoppedByUser << sPath;
        emit sendStopDir(sPath);
        // 立刻把这一行（以及已经建出来的子行）标成"已停止"，
        // 不等线程下一次快照 —— 否则点了按钮看起来没任何变化。
        markSubtreeStopped(sPath);
        actionItem->setText(tr("已停止"));
        actionItem->setData(int(DirNodeState::Stopped), kRoleState);
        updateActionButton(actionItem, DirNodeState::Stopped, sPath);
    } else if (state == DirNodeState::Waiting || state == DirNodeState::Incomplete) {
        if (!isSkippedByUser(sPath))
            m_skippedByUser << sPath;
        emit sendSkipDir(sPath);
        // 和"停止"一样：立刻把这一行（以及已建出来的子行）标成"已跳过"。
        // 否则 150ms 后快照会把状态写回"等待"，用户看到的就是"点了没反应"。
        markSubtreeSkipped(sPath);
        actionItem->setText(tr("已跳过"));
        actionItem->setData(int(DirNodeState::Skipped), kRoleState);
        updateActionButton(actionItem, DirNodeState::Skipped, sPath);
    }
}

void FormPageProgressInfo::onRecvDirSnapshot(const DirProgressSnapshot &snapshot)
{
    for (const DirProgressNode &node : snapshot)
        applyNode(node);

    // 计数取所有根节点之和（根节点的计数本来就包含了整棵子树）
    int nScanned = 0;
    int nMatched = 0;
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        QStandardItem *item = it.value();
        if (item->parent() != nullptr)
            continue;
        QStandardItem *parentItem = m_model->invisibleRootItem();
        QStandardItem *filesItem = parentItem->child(item->row(), 2);
        QStandardItem *matchItem = parentItem->child(item->row(), 3);
        if (filesItem != nullptr)
            nScanned += filesItem->text().toInt();
        if (matchItem != nullptr)
            nMatched += matchItem->text().toInt();
    }
    m_nScanned = nScanned;
    m_nMatched = nMatched;
    updateStatusLabel();
}

void FormPageProgressInfo::onRecvProgress(const QString &sProcessedFilePath)
{
    // 只保留"当前正在处理的文件"一行，不再往文本框里追加几十万行。
    // 注意：这里**不再**自动展开/选中目录 —— 树保持用户自己的展开状态，
    // 需要看哪个分支由用户手动展开。
    m_sCurrentFile = sProcessedFilePath;
    if (m_bAiMode)
        appendAILog(sProcessedFilePath);
    updateStatusLabel();
}

void FormPageProgressInfo::updateStatusLabel()
{
    if (m_statusLabel == nullptr)
        return;

    if (m_bAiMode) {
        if (m_bAiPrepare) {
            const QString sText = tr("AI 环境准备：%1").arg(m_sAiStage);
            m_statusLabel->setText(sText);
            m_statusLabel->setToolTip(sText);
            return;
        }

        QString sText = tr("AI 打标：已处理 %1 个文件").arg(m_nAiDone);
        if (!m_sCurrentFile.isEmpty()) {
            const QFontMetrics metrics(m_statusLabel->font());
            const QString sPrefix = tr("    当前：");
            int nPathWidth = m_statusLabel->width();
            if (nPathWidth > 40)
                nPathWidth = nPathWidth - metrics.boundingRect(sPrefix).width()
                             - metrics.boundingRect(sText).width() - 16;
            nPathWidth = qBound(120, nPathWidth, 600);
            sText += sPrefix
                     + metrics.elidedText(QDir::toNativeSeparators(m_sCurrentFile),
                                          Qt::ElideMiddle, nPathWidth);
        }
        m_statusLabel->setText(sText);
        m_statusLabel->setToolTip(sText);
        return;
    }

    if (m_bFinished) {
        // 结束/中断后停在总结页：状态行只显示总结，不再显示计数和当前文件
        m_statusLabel->setText(m_sFinishSummary);
        m_statusLabel->setToolTip(m_sFinishSummary);
        return;
    }

    const QString sCounter = tr("已扫描 %1 个文件 · 命中 %2 个").arg(m_nScanned).arg(m_nMatched);
    QString sText = sCounter;
    QString sFullText = sCounter;

    if (!m_sCurrentFile.isEmpty()) {
        const QString sPath = QDir::toNativeSeparators(m_sCurrentFile);
        const QString sPrefix = tr("    当前：");
        const QFontMetrics metrics(m_statusLabel->font());

        // 路径最多占 600px（约 50~60 个字符），再长就从中间省略：
        // 保留盘符和文件名，隐藏中间的目录（和资源管理器一致）。
        int nPathWidth = m_statusLabel->width();
        if (nPathWidth > 40)
            nPathWidth = nPathWidth - metrics.boundingRect(sPrefix).width() - metrics.boundingRect(sCounter).width() - 16;
        nPathWidth = qBound(120, nPathWidth, 600);

        sText = sCounter + sPrefix + metrics.elidedText(sPath, Qt::ElideMiddle, nPathWidth);
        sFullText = sCounter + sPrefix + sPath;
    }

    m_statusLabel->setText(sText);
    // 省略掉的完整内容放 tooltip，鼠标悬停能看到
    m_statusLabel->setToolTip(sFullText);
}

void FormPageProgressInfo::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateStatusLabel();   // 宽度变了要重新算省略位置
}

void FormPageProgressInfo::onTreeClicked(const QModelIndex &index)
{
    if (!index.isValid() || index.column() != 4)
        return;

    QStandardItem *actionItem = m_model->itemFromIndex(index);
    if (actionItem == nullptr)
        return;

    onActionClicked(actionItem->data(kRolePath).toString());
}

void FormPageProgressInfo::onBtnStopClicked()
{
    emit sendStop();
}

void FormPageProgressInfo::clear()
{
    m_bAiMode = false;
    m_bAiPrepare = false;
    m_nAiDone = 0;
    m_sAiStage.clear();
    m_bFinished = false;
    m_sFinishSummary.clear();
    if (m_btnBack != nullptr)
        m_btnBack->hide();
    ui->plainTextEdit->setVisible(false);
    if (m_tree != nullptr)
        m_tree->setVisible(true);
    setRunning(false);  // 结束/离开：隐藏【停止全部】，避免"点了没反应"
    ui->plainTextEdit->clear();
    m_items.clear();
    m_stoppedByUser.clear();
    m_skippedByUser.clear();
    if (m_model != nullptr)
        m_model->removeRows(0, m_model->rowCount());
    m_sCurrentFile.clear();
    m_nScanned = 0;
    m_nMatched = 0;
    updateStatusLabel();
}

/** 任务是否在跑：控制【停止全部】按钮与 Esc 的可用性。 */
void FormPageProgressInfo::setRunning(bool bRunning)
{
    m_bRunning = bRunning;
    if (ui->btnStop != nullptr)
        ui->btnStop->setVisible(bRunning);
    if (ui->lblStopHint != nullptr)
        ui->lblStopHint->setVisible(bRunning);
    if (m_stopShortcut != nullptr)
        m_stopShortcut->setEnabled(bRunning);
    updateStatusLabel();
}


/**
 * 任务被中断/结束后停在进度页：隐藏【停止全部】、禁用 Esc，
 * 状态行改成一句总结，并显示【← 返回目录】。
 */
void FormPageProgressInfo::showTraversalStoppedSummary(int nWrittenRoots)
{
    // 按进度树里**看得见的行**统计，避免出现"树上有 5 行完成、总结写 0 个"的错觉
    int nComplete = 0;
    int nSkipped = 0;
    int nOther = 0;
    for (auto it = m_items.constBegin(); it != m_items.constEnd(); ++it) {
        QStandardItem *nameItem = it.value();
        if (nameItem == nullptr)
            continue;
        QStandardItem *parentItem = nameItem->parent();
        if (parentItem == nullptr)
            parentItem = m_model->invisibleRootItem();
        QStandardItem *actionItem = parentItem->child(nameItem->row(), 4);
        if (actionItem == nullptr)
            continue;
        const int nState = actionItem->data(kRoleState).toInt();
        if (nState == int(DirNodeState::Complete))
            ++nComplete;
        else if (nState == int(DirNodeState::Skipped))
            ++nSkipped;
        else
            ++nOther;
    }

    m_bFinished = true;
    m_sFinishSummary = tr("遍历已停止：完成 %1 个目录，跳过 %2 个，%3 个未完成"
                          "（已写入缓存 %4 个根目录）")
                           .arg(nComplete).arg(nSkipped).arg(nOther).arg(nWrittenRoots);
    m_sCurrentFile.clear();
    setRunning(false);          // 隐藏停止按钮、禁用 Esc
    if (m_btnBack != nullptr)
        m_btnBack->setVisible(true);
    updateStatusLabel();
}
