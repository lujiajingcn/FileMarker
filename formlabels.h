#ifndef FORMALLLABELS_H
#define FORMALLLABELS_H

#include <QWidget>

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

protected:
    void traverseWriteLabel(QStandardItem *item, const QList<LabelInfo> &lstLabelInfo);
    void traverseReadLabel(QStandardItem *item, QList<LabelInfo> &lstLabelInfo);
    void saveLabelToXmlFile();

private slots:
    void on_actionAdd_triggered();

    void on_actionAddChild_triggered();

    void on_actionDelete_triggered();

    void on_actionMoveUp_triggered();

    void on_actionMoveDown_triggered();

    void on_actionMoveLeft_triggered();

    void on_actionMoveRight_triggered();

private:
    QTreeView               *m_tvAllLabels;
    QStandardItemModel      *m_modelAllLabels;
private:
    Ui::FormLabels *ui;
};

#endif // FORMALLLABELS_H
