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
    void traverseWriteLabel(QStandardItem *item, const QList<LabelInfo> &lstLabelInfo);
    void traverseReadLabel(QStandardItem *item, QList<LabelInfo> &lstLabelInfo);
    void saveLabelToXmlFile();

    void addItem(QStandardItem *parentItem = nullptr);

private slots:
    void on_actionAdd_triggered();

    void on_actionAddChild_triggered();

    void on_actionDelete_triggered();

    void on_actionMoveUp_triggered();

    void on_actionMoveDown_triggered();

    void on_actionMoveLeft_triggered();

    void on_actionMoveRight_triggered();

    void onItemChanged(QStandardItem *item); // 用于检查新标签是否已经存在

    void on_treeView_clicked(const QModelIndex &index);

private:
    Ui::FormLabels *ui;

    QTreeView               *m_tvAllLabels;
    QStandardItemModel      *m_modelAllLabels;

    QSet<QString>           m_setLabels;
};

#endif // FORMALLLABELS_H
