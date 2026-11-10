/**
** 【文件导航】
**
**
**/
#ifndef FORMFILENAVIGATION_H
#define FORMFILENAVIGATION_H

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>

namespace Ui {
class FormFileNavigation;
}

class FormFileNavigation : public QWidget
{
    Q_OBJECT

public:
    explicit FormFileNavigation(QWidget *parent = nullptr);
    ~FormFileNavigation();

signals:
    void sendNavigationClicked(const QModelIndex &index);

private:
    // 目录树点击处理：构造函数中显式 connect，
    // 不再使用 on_<对象名>_<信号名> 的自动连接（connectSlotsByName）。
    void onTvNavigationClicked(const QModelIndex &index);

private:
    Ui::FormFileNavigation *ui;

    QTreeView               *m_navigationTree;        // 左侧导航栏
    QFileSystemModel        *m_modelNavigation;

};

#endif // FORMFILENAVIGATION_H
