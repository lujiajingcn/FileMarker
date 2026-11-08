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

namespace Ui {
class FormLabels;
}

class FormLabels : public QWidget
{
    Q_OBJECT

public:
    explicit FormLabels(QWidget *parent = nullptr);
    ~FormLabels();

    void addToolButtonsToMenu(QMenu *menuLabelManage);

    void setToolButtonDefaultAction();

    /** 从文件初始化标签树 */
    void initLabels();

    QStringList getSelLabels();

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

    // 询问是否把“从文件编辑产生的新标签”加入标签库（是否加入由 QSettings 记住）
    void promptAddNewLabels(const QStringList &qLNewLabels);

private:
    Ui::FormLabels *ui;

    QTreeView               *m_tvAllLabels;
    QStandardItemModel      *m_modelAllLabels;

    QSet<QString>           m_setLabels;
    // 每个节点最近一次“提交成功”的文本，按持久索引记录；用于重命名去重与回退原名。
    QMap<QPersistentModelIndex, QString> m_committedText;
    bool m_bPromptShowing;

    enum { LabelUuidRole = Qt::UserRole + 1 };
};

#endif // FORMALLLABELS_H
