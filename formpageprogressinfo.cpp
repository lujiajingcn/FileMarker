#include "formpageprogressinfo.h"
#include "ui_formpageprogressinfo.h"

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

void FormPageProgressInfo::resetProgress(const QStringList &qLSelDirs)
{
    m_items.clear();
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
    const QString sStateText = stateText(node.state);
    if (stateItem->text() != sStateText) {
        stateItem->setText(sStateText);
        stateItem->setForeground(stateColor(node.state));
        QFont font = stateItem->font();
        font.setBold(node.state == DirNodeState::Searching);
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
    actionItem->setData(int(node.state), kRoleState);

    // 操作列放真正的按钮（不是靠点单元格文字）：更直观，也方便自动化点它
    updateActionButton(actionItem, node.state, node.path);
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
        emit sendStopDir(sPath);
        actionItem->setText(tr("已停止"));
        actionItem->setData(int(DirNodeState::Stopped), kRoleState);
        updateActionButton(actionItem, DirNodeState::Stopped, sPath);
    } else if (state == DirNodeState::Waiting || state == DirNodeState::Incomplete) {
        emit sendSkipDir(sPath);
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
    updateStatusLabel();
}

void FormPageProgressInfo::updateStatusLabel()
{
    if (m_statusLabel == nullptr)
        return;

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
    ui->plainTextEdit->clear();
    m_items.clear();
    if (m_model != nullptr)
        m_model->removeRows(0, m_model->rowCount());
    m_sCurrentFile.clear();
    m_nScanned = 0;
    m_nMatched = 0;
    updateStatusLabel();
}
