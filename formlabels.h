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

private slots:
    void on_actionAdd_triggered();

    void on_actionAddChild_triggered();

    void on_actionDelete_triggered();

    void on_actionMoveUp_triggered();

    void on_actionMoveDown_triggered();

    void on_actionMoveLeft_triggered();

    void on_actionMoveRight_triggered();

    void onItemChanged(QStandardItem *item); // 检查新标签是否已存在，并重命名时保持集合同步

    void on_treeView_clicked(const QModelIndex &index);

private:
    Ui::FormLabels *ui;

    QTreeView               *m_tvAllLabels;
    QStandardItemModel      *m_modelAllLabels;

    QSet<QString>           m_setLabels;
    // 每个节点最近一次“提交成功”的文本，按持久索引记录；用于重命名去重与回退原名。
    QMap<QPersistentModelIndex, QString> m_committedText;

    enum { LabelUuidRole = Qt::UserRole + 1 };
};

#endif // FORMALLLABELS_H
