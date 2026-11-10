#include "formlabels.h"
#include "ui_formlabels.h"

#include <QToolButton>
#include <QSpacerItem>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QDrag>
#include <QMimeData>
#include <QAction>
#include <QMenu>
#include <QCheckBox>
#include <QSettings>
#include <QTimer>
#include <functional>
#include <algorithm>
#include "adsoperation.h"
#include "sqliteoperation.h"

#include "common.h"
#include "utility.h"

FormLabels::FormLabels(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormLabels)
{
    ui->setupUi(this);

    m_bPromptShowing = false;
    m_tvAllLabels = ui->treeView;
    m_modelAllLabels = new QStandardItemModel(this);
    m_tvAllLabels->setModel(m_modelAllLabels);

    // 方案A：标签库的命令收在本面板里 —— dock 工具条 + 右键菜单
    m_tvAllLabels->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tvAllLabels, &QTreeView::customContextMenuRequested,
            this, &FormLabels::onTreeContextMenuRequested);

    setToolButtonDefaultAction();

    // 显式连接标签管理动作与标签树点击（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(ui->actionAdd, &QAction::triggered, this, &FormLabels::onActionAddTriggered);
    connect(ui->actionAddChild, &QAction::triggered, this, &FormLabels::onActionAddChildTriggered);
    connect(ui->actionDelete, &QAction::triggered, this, &FormLabels::onActionDeleteTriggered);
    connect(ui->actionMoveUp, &QAction::triggered, this, &FormLabels::onActionMoveUpTriggered);
    connect(ui->actionMoveDown, &QAction::triggered, this, &FormLabels::onActionMoveDownTriggered);
    connect(ui->actionMoveLeft, &QAction::triggered, this, &FormLabels::onActionMoveLeftTriggered);
    connect(ui->actionMoveRight, &QAction::triggered, this, &FormLabels::onActionMoveRightTriggered);
    connect(m_tvAllLabels, &QTreeView::clicked, this, &FormLabels::onTreeViewClicked);

    initLabels();

    connect(m_modelAllLabels, &QStandardItemModel::itemChanged, this, &FormLabels::onItemChanged);

    // 把标签从标签库拖到文件列表 = 给文件打标签。
    // 这里自己起 QDrag（不用视图内建的 startDrag），是因为要给拖拽数据打上
    // 我们自己的 MIME 类型，文件列表那边才好识别"这是一次打标签的拖拽"。
    m_tvAllLabels->viewport()->installEventFilter(this);
}




FormLabels::~FormLabels()
{
    // 退出时保存失败不能弹模态框（事件循环已结束），改为写备份文件并记日志
    m_bShuttingDown = true;
    saveLabelToXmlFile();
    delete ui;
}



QStringList FormLabels::allLabels() const
{
    QStringList qLResult;
    std::function<void(QStandardItem*)> collect = [&](QStandardItem *parent) {
        if (parent == nullptr)
            return;
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem *item = parent->child(i);
            if (item == nullptr)
                continue;
            // AI 分组只是界面节点，不参与“未使用标签”清理
            if (QString::compare(item->text(), LABEL_NAME_AI) != 0)
                qLResult << item->text();
            collect(item);
        }
    };
    collect(m_modelAllLabels->invisibleRootItem());
    return qLResult;
}

bool FormLabels::removeLabels(const QStringList &qLLabels)
{
    if (qLLabels.isEmpty())
        return false;

    std::function<bool(QStandardItem*)> removeFrom = [&](QStandardItem *parent) {
        bool bRemoved = false;
        for (int i = parent->rowCount() - 1; i >= 0; --i) {
            QStandardItem *item = parent->child(i);
            if (item == nullptr)
                continue;

            bool bMatch = false;
            for (const QString &sLabel : qLLabels) {
                if (QString::compare(item->text(), sLabel, Qt::CaseInsensitive) == 0) {
                    bMatch = true;
                    break;
                }
            }
            if (bMatch) {
                parent->removeRow(i);
                bRemoved = true;
            } else if (removeFrom(item)) {
                bRemoved = true;
            }
        }
        return bRemoved;
    };

    const bool bRemoved = removeFrom(m_modelAllLabels->invisibleRootItem());
    if (bRemoved) {
        resyncSets();
        saveLabelToXmlFile();
    }
    return bRemoved;
}


void FormLabels::initLabels()
{
    XmlOperation xmlOperation;
    QString sXmlFile = g_sAppDir + "/" + LABEL_FILE;
    QList<LabelInfo> lstLabelInfo = xmlOperation.readLabelFromXmlFile(sXmlFile.toStdString().c_str());
    for(QList<LabelInfo>::const_iterator cItr = lstLabelInfo.begin(); cItr != lstLabelInfo.end(); cItr++)
    {
        QString sName = (*cItr).sName;
        // 保留从文件读到的 UUID（为空则新生成），避免保存时 UUID 被清空。
        QString sUuid = (*cItr).sUuid.isEmpty() ? Utility::createUuid() : (*cItr).sUuid;
        QStandardItem *item = createLabelItem(sName, sUuid);
        if((*cItr).lstChild.count() > 0)
        {
            traverseWriteLabel(item, (*cItr).lstChild);
        }
        m_modelAllLabels->appendRow(item);
        registerItem(item);
    }
}

QStringList FormLabels::getSelLabels()
{
    QStringList qLLabels;
    QItemSelectionModel *selectionModel = m_tvAllLabels->selectionModel();
    // selectedIndexes() 会为同一选中行的每一列都返回一个索引，若不只取第 0 列，
    // 会导致同一标签被重复返回多次（搜索时会重复匹配）。
    // 这里按选中行的顺序逐个追加并去重，保证返回顺序与树上看到的顺序一致
    // （原先用 QSet::values() 会打乱顺序，导致批量添加标签时顺序随机）。
    QModelIndexList selectedIndexes = selectionModel->selectedRows(0);
    // Qt 不保证 selectedRows 的顺序；按界面可见位置排序，保证批量添加顺序稳定
    std::sort(selectedIndexes.begin(), selectedIndexes.end(),
              [this](const QModelIndex &a, const QModelIndex &b) {
                  return m_tvAllLabels->visualRect(a).top() < m_tvAllLabels->visualRect(b).top();
              });
    foreach (const QModelIndex &index, selectedIndexes)
    {
        QStandardItem *item = m_modelAllLabels->itemFromIndex(index);
        if (item == nullptr)
            continue;

        const QString sLabel = item->text();
        if (!qLLabels.contains(sLabel))
            qLLabels << sLabel;
    }
    return qLLabels;
}

void FormLabels::addItem(QStandardItem *parentItem)
{
    QStandardItem *newItem = createLabelItem(nextNewLabelName());
    if(nullptr == parentItem)
    {
        m_modelAllLabels->appendRow(newItem);
    }
    else
    {
        parentItem->appendRow(newItem);
    }
    registerItem(newItem);
    m_tvAllLabels->setCurrentIndex(newItem->index());
    // 新建的节点直接进入改名状态：默认名是"新标签"，让用户马上就改，
    // 而不是点完"新建标签"看到多了一行"新标签"还要再双击一次。
    m_bNamingNewLabel = true;          // 这次改名只是给新标签起名，不要弹同步询问
    m_tvAllLabels->edit(newItem->index());
    saveLabelToXmlFile();   // #11 新增标签后立即持久化
}

// 把选中的标签展开成"组"（组 = 标签 + 全部子标签），供搜索使用。
QList<QSet<QString>> FormLabels::getSelLabelGroups() const
{
    QList<QSet<QString>> qLGroups;
    QItemSelectionModel *selectionModel = m_tvAllLabels->selectionModel();
    if (selectionModel == nullptr)
        return qLGroups;

    QModelIndexList selectedIndexes = selectionModel->selectedRows(0);
    std::sort(selectedIndexes.begin(), selectedIndexes.end(),
              [this](const QModelIndex &a, const QModelIndex &b) {
                  return m_tvAllLabels->visualRect(a).top() < m_tvAllLabels->visualRect(b).top();
              });

    std::function<void(QStandardItem *, QSet<QString> &)> collect =
        [&collect](QStandardItem *pItem, QSet<QString> &setNames) {
            if (pItem == nullptr)
                return;
            setNames.insert(pItem->text());
            for (int i = 0; i < pItem->rowCount(); ++i)
                collect(pItem->child(i), setNames);
        };

    foreach (const QModelIndex &index, selectedIndexes) {
        QStandardItem *pItem = m_modelAllLabels->itemFromIndex(index);
        if (pItem == nullptr)
            continue;
        QSet<QString> setNames;
        collect(pItem, setNames);
        if (!setNames.isEmpty())
            qLGroups << setNames;
    }
    return qLGroups;
}












QStandardItem* FormLabels::createLabelItem(const QString &name, const QString &uuid)
{
    QStandardItem *item = new QStandardItem(name);
    // 每个标签节点携带稳定 UUID，保存时回写，避免退出保存后标识全部丢失。
    item->setData(uuid.isEmpty() ? Utility::createUuid() : uuid, LabelUuidRole);
    return item;
}

void FormLabels::registerItem(QStandardItem *item)
{
    if (item == nullptr)
        return;
    m_setLabels.insert(item->text());
    m_committedText[QPersistentModelIndex(item->index())] = item->text();
    // 递归登记子树
    for (int i = 0; i < item->rowCount(); ++i)
        registerItem(item->child(i));
}



// 依据当前模型重建去重集合与“已提交文本”映射（用于删除等结构性变更后保持一致）。
void FormLabels::resyncSets()
{
    m_setLabels.clear();
    m_committedText.clear();
    for (int i = 0; i < m_modelAllLabels->rowCount(); ++i)
        registerItem(m_modelAllLabels->item(i));
}

bool FormLabels::saveLabelToXmlFile()
{
    XmlOperation xOperation;
    QList<LabelInfo> lstLabelInfo;
    int nRowCount = m_modelAllLabels->rowCount();
    for (int i = 0; i < nRowCount; i++)
    {
        LabelInfo labelInfo;
        QStandardItem *item = m_modelAllLabels->item(i);
        if (item == nullptr)   // 模型里不该再出现 null 行，真出现也不能崩
            continue;
        labelInfo.sUuid = item->data(LabelUuidRole).toString();
        if (labelInfo.sUuid.isEmpty())
            labelInfo.sUuid = Utility::createUuid();
        traverseReadLabel(item, labelInfo.lstChild);
        labelInfo.sName = item->text();
        lstLabelInfo.push_back(labelInfo);
    }
    const QString sXmlFile = g_sAppDir + "/" + LABEL_FILE;
    if (xOperation.saveXml(sXmlFile.toStdString().c_str(), lstLabelInfo))
        return true;

    // 保存失败：目录只读、磁盘满、文件被占用等。以前这里直接忽略返回值，
    // 用户改了半天标签库、退出后全没了却毫无提示。
    qCritical() << "保存标签库失败:" << sXmlFile;

    // 兜底：另存一份到用户目录，至少不让这次的编辑彻底消失
    const QString sBackupDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString sBackupFile;
    if (!sBackupDir.isEmpty() && QDir().mkpath(sBackupDir)) {
        sBackupFile = sBackupDir + QStringLiteral("/labels-recovered-")
                      + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))
                      + QStringLiteral(".xml");
        if (!xOperation.saveXml(sBackupFile.toStdString().c_str(), lstLabelInfo))
            sBackupFile.clear();
        else
            qCritical() << "标签库已备份到:" << sBackupFile;
    }

    if (m_bShuttingDown) {
        // 退出阶段只是记日志，不再打扰用户
        return false;
    }

    if (!m_bSaveFailureWarned) {
        m_bSaveFailureWarned = true;
        QString sDetail = sBackupFile.isEmpty()
                              ? tr("请检查程序目录是否可写、磁盘是否已满。")
                              : tr("这次的编辑已临时另存到：\n%1").arg(sBackupFile);
        QMessageBox::warning(this, tr("标签库保存失败"),
                             tr("标签库没能写入：\n%1\n\n%2").arg(sXmlFile, sDetail));
    }
    return false;
}

void FormLabels::traverseWriteLabel(QStandardItem *item, const QList<LabelInfo> &lstLabelInfo)
{
    for (QList<LabelInfo>::const_iterator cItr = lstLabelInfo.begin(); cItr != lstLabelInfo.end(); cItr++)
    {
        // 写子节点时同样保留 UUID（为空则新生成）。
        QString sUuid = (*cItr).sUuid.isEmpty() ? Utility::createUuid() : (*cItr).sUuid;
        QStandardItem *itemChild = createLabelItem((*cItr).sName, sUuid);
        if((*cItr).lstChild.count() > 0)
        {
            traverseWriteLabel(itemChild, (*cItr).lstChild);
        }
        item->appendRow(itemChild);
    }
}

void FormLabels::traverseReadLabel(QStandardItem *item, QList<LabelInfo> &lstLabelInfo)
{
    if (item == nullptr)
        return;
    int nRowCount = item->rowCount();
    for(int i = 0; i < nRowCount; i++)
    {
        QStandardItem *itemChild = item->child(i);
        if (itemChild == nullptr)   // 跳过 null 子项
            continue;
        LabelInfo labelInfo;
        labelInfo.sUuid = itemChild->data(LabelUuidRole).toString();
        if (labelInfo.sUuid.isEmpty())
            labelInfo.sUuid = Utility::createUuid();
        labelInfo.sName = itemChild->text();
        traverseReadLabel(itemChild, labelInfo.lstChild);
        lstLabelInfo.push_back(labelInfo);
    }
}

void FormLabels::updateAllLabelList(QVector<QString> vtAllLabels)
{
    m_modelAllLabels->removeRows(0, m_modelAllLabels->rowCount());
    m_setLabels.clear();
    m_committedText.clear();
    for(QVector<QString>::iterator it = vtAllLabels.begin(); it != vtAllLabels.end(); it++)
    {
        QStandardItem *item = createLabelItem(*it);
        m_modelAllLabels->appendRow(item);
        registerItem(item);
    }
}




