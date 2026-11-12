#include "formpageprogressinfo.h"
#include "ui_formpageprogressinfo.h"

#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
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
        return QObject::tr("停止");
    case DirNodeState::Waiting:
    case DirNodeState::Complete:
    case DirNodeState::Incomplete:
        return QObject::tr("跳过");
    case DirNodeState::Stopped:
    case DirNodeState::Skipped:
        return QString();
    }
    return QString();
}

const int kRolePath = Qt::UserRole;
const int kRoleState = Qt::UserRole + 1;

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
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_tree->header()->resizeSection(4, 70);

    connect(m_tree, &QTreeView::clicked, this, &FormPageProgressInfo::onTreeClicked);

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

    for (const QString &sDir : qLSelDirs) {
        DirProgressNode node;
        node.path = sDir;
        node.level = 0;
        node.state = DirNodeState::Waiting;
        applyNode(node);
    }

    m_tree->expandAll();
    updateStatusLabel();
}

QStandardItem *FormPageProgressInfo::ensureItem(const QString &sPath, int nLevel)
{
    if (nLevel < 0 || nLevel > kMaxDisplayDepth)
        return nullptr;

    const auto existed = m_items.constFind(sPath);
    if (existed != m_items.constEnd())
        return existed.value();

    QList<QStandardItem *> items;
    for (int i = 0; i < kDirProgressColumns; ++i)
        items << new QStandardItem();

    // 显示名：搜索根显示完整路径，其它层只显示目录名（和资源管理器一致）
    const QString sName = (nLevel == 0) ? QDir::toNativeSeparators(sPath)
                                        : QFileInfo(sPath).fileName();
    items[0]->setText(sName);
    items[0]->setData(sPath, kRolePath);
    items[0]->setToolTip(QDir::toNativeSeparators(sPath));
    for (int i = 1; i < kDirProgressColumns; ++i)
        items[i]->setData(sPath, kRolePath);

    QStandardItem *parentItem = nullptr;
    if (nLevel > 0) {
        // 快照里的节点顺序不保证父在前，所以父节点不存在就现建（递归向上补链），
        // 否则子节点会被挂到根上，层级就乱了。
        parentItem = ensureItem(QFileInfo(sPath).path(), nLevel - 1);
    }

    if (parentItem != nullptr)
        parentItem->appendRow(items);
    else
        m_model->invisibleRootItem()->appendRow(items);

    m_items.insert(sPath, items[0]);
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

    stateItem->setText(stateText(node.state));
    stateItem->setForeground(stateColor(node.state));
    QFont font = stateItem->font();
    font.setBold(node.state == DirNodeState::Searching);
    stateItem->setFont(font);

    filesItem->setText(node.files > 0 ? QString::number(node.files) : QString());
    matchItem->setText(node.matches > 0 ? QString::number(node.matches) : QString());

    // 第 3 层节点下面还有更深目录时，提示被聚合的数量
    if (node.subDirs > 0) {
        nameItem->setToolTip(tr("%1\n（含 %2 个子目录，已聚合计入本行）")
                                 .arg(QDir::toNativeSeparators(node.path))
                                 .arg(node.subDirs));
    }

    actionItem->setText(actionText(node.state));
    actionItem->setData(int(node.state), kRoleState);
    actionItem->setForeground(QColor(0x1a, 0x73, 0xe8));
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
    // 只保留"当前正在处理的文件"一行，不再往文本框里追加几十万行
    m_sCurrentFile = sProcessedFilePath;
    updateStatusLabel();
}

void FormPageProgressInfo::updateStatusLabel()
{
    if (m_statusLabel == nullptr)
        return;

    QString sText = tr("已扫描 %1 个文件 · 命中 %2 个").arg(m_nScanned).arg(m_nMatched);
    if (!m_sCurrentFile.isEmpty())
        sText += tr("    当前：%1").arg(QDir::toNativeSeparators(m_sCurrentFile));
    m_statusLabel->setText(sText);
}

void FormPageProgressInfo::onTreeClicked(const QModelIndex &index)
{
    if (!index.isValid() || index.column() != 4)
        return;

    QStandardItem *actionItem = m_model->itemFromIndex(index);
    if (actionItem == nullptr)
        return;

    const QString sPath = actionItem->data(kRolePath).toString();
    const DirNodeState state = DirNodeState(actionItem->data(kRoleState).toInt());
    if (sPath.isEmpty())
        return;

    if (state == DirNodeState::Searching) {
        emit sendStopDir(sPath);
        actionItem->setText(tr("已停止"));
        actionItem->setData(int(DirNodeState::Stopped), kRoleState);
    } else if (state == DirNodeState::Waiting || state == DirNodeState::Complete
               || state == DirNodeState::Incomplete) {
        emit sendSkipDir(sPath);
        actionItem->setText(tr("已跳过"));
        actionItem->setData(int(DirNodeState::Skipped), kRoleState);
    }
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
