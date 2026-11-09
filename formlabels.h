/**
** 【标签管理】
**
** 标签可以按照按照分类形成树形结构，也可以简单的以列表形式显示。
**
**/

#ifndef FORMALLLABELS_H
#define FORMALLLABELS_H

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QMap>
#include <QPersistentModelIndex>

#include "xmloperation.h"
#include <QSet>

namespace Ui {
class FormLabels;
}

class SqliteOperation;

class FormLabels : public QWidget
{
    Q_OBJECT

public:
    explicit FormLabels(QWidget *parent = nullptr);
    ~FormLabels();

    // 方案A：标签库命令只在面板内（dock 工具条 + 右键菜单），不再挂到主窗口菜单栏。
    void addContextMenuAction(QAction *action);

    void setToolButtonDefaultAction();

    /**
     * 注入缓存数据库。标签库的改名/删除只改库、不碰文件，
     * 但这个差别必须让用户看见 —— 有了它就能显示"有 N 个文件在用"并让用户选择是否同步到文件。
     */
    void setSqlOperation(SqliteOperation *op) { m_sqlOperation = op; }

    /** 从文件初始化标签树 */
    void initLabels();

    QStringList getSelLabels();

    /**
     * 把当前选中的标签展开成"组"（每组 = 选中的标签 + 它的全部子标签）。
     * 搜索用这个：组内 OR（搜父标签能搜到打了子标签的文件），组间仍按设置的 AND/OR 组合。
     * 注意与 getSelLabels() 的区别：那个是"直接选中的标签"，用于给文件打标签。
     */
    QList<QSet<QString>> getSelLabelGroups() const;

    // 刷新全部标签列表
    void updateAllLabelList(QVector<QString> vtAllLabels);

    /** 返回标签库中所有标签名（跳过 AI 分组节点本身，包含其子标签） */
    QStringList allLabels() const;

    /** 从标签库删除指定名字的标签（大小写不敏感），返回是否删除了内容 */
    bool removeLabels(const QStringList &qLLabels);

signals:
    void sendSelLabels(QStringList qLSelLabels);

protected:
    /** 创建带 UUID 的标签节点 */
    QStandardItem* createLabelItem(const QString &name, const QString &uuid = QString());
    /** 将节点（含子树）登记到 m_setLabels 与 m_committedText，供去重/回退使用 */
    void registerItem(QStandardItem *item);
    /** 依据当前模型重建去重集合（结构性变更后调用） */
    void resyncSets();
    /** 统计整棵树中名为 name 的节点数量（用于重复检测） */
    /** 生成一个新的标签名：新标签 / 新标签1 / 新标签2 …（取第一个没被占用的，忽略大小写） */
    QString nextNewLabelName() const;

    int countLabelOccurrences(const QString &name) const;

    void traverseWriteLabel(QStandardItem *item, const QList<LabelInfo> &lstLabelInfo);
    void traverseReadLabel(QStandardItem *item, QList<LabelInfo> &lstLabelInfo);
    void saveLabelToXmlFile();

    void addItem(QStandardItem *parentItem = nullptr);

public slots:
    void onRecvLabels(QString sLabels);

    void onRecvLabelsGeneratedByAI(QString sLabels);

private:
    // 标签管理动作与标签树点击的处理函数：全部在构造函数中显式 connect，
    // 不再使用 on_<对象名>_<信号名> 的自动连接（connectSlotsByName）。
    void onActionAddTriggered();

    void onActionAddChildTriggered();

    void onActionDeleteTriggered();

    void onActionMoveUpTriggered();

    void onActionMoveDownTriggered();

    void onActionMoveLeftTriggered();

    void onActionMoveRightTriggered();

    void onItemChanged(QStandardItem *item); // 检查新标签是否已存在，并重命名时保持集合同步

    void onTreeViewClicked(const QModelIndex &index);

    /** 标签树的右键菜单：新建/子标签/删除/移动 + 外部追加的命令 */
    void onTreeContextMenuRequested(const QPoint &pos);

    // 询问是否把“从文件编辑产生的新标签”加入标签库（是否加入由 QSettings 记住）
    void promptAddNewLabels(const QStringList &qLNewLabels);

    /** 改名后询问"要不要同时改文件上的标签"（排队执行，避免在 itemChanged 里 exec 模态框） */
    void promptSyncRenameToFiles(const QString &sOldTag, const QString &sNewTag);

    /** 把改名/删除同步到文件；返回成功处理的文件数，失败数写进 nFailed */
    int syncTagToFiles(bool bRename, const QString &sOldTag, const QString &sNewTag, int &nFailed);

private:
    Ui::FormLabels *ui;

    QTreeView               *m_tvAllLabels;
    QStandardItemModel      *m_modelAllLabels;

    QSet<QString>           m_setLabels;
    // 每个节点最近一次“提交成功”的文本，按持久索引记录；用于重命名去重与回退原名。
    QMap<QPersistentModelIndex, QString> m_committedText;
    bool m_bPromptShowing;
    // 程序化改回名字时不要再次弹"是否同步到文件"
    bool m_bSuppressSyncPrompt = false;
    // 新建标签后正在输入名字：这一次改名只是给新标签起名，不该弹"要不要同步到文件"
    bool m_bNamingNewLabel = false;
    // 右键菜单里追加的外部命令（如"清理未使用标签"）
    QList<QAction *>        m_extraContextActions;
    SqliteOperation        *m_sqlOperation = nullptr;

    enum { LabelUuidRole = Qt::UserRole + 1 };
};

#endif // FORMALLLABELS_H
