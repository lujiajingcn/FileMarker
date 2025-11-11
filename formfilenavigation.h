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

private slots:
    void on_tvNavigation_clicked(const QModelIndex &index);

private:
    Ui::FormFileNavigation *ui;

    QTreeView               *m_tvNavigation;        // 左侧导航栏
    QFileSystemModel        *m_modelNavigation;

};

#endif // FORMFILENAVIGATION_H
