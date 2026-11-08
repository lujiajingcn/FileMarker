/**
** 【当前目录标签】
**
** 用于显示当前目录中的所有文件所添加的标签总和，在文件列表窗口中，打开新的目录时更新。
**
**/

#ifndef FORMCURDIRLABELS_H
#define FORMCURDIRLABELS_H

#include <QWidget>
#include <QStandardItemModel>
#include <QSet>

namespace Ui {
class FormCurDirLabels;
}

class FormCurDirLabels : public QWidget
{
    Q_OBJECT

public:
    explicit FormCurDirLabels(QWidget *parent = nullptr);
    ~FormCurDirLabels();

signals:
    void sendSelLabels(QStringList qLSelLabels);

public slots:
    void onRecvDirAndLabels(QMap<QString, QSet<QString>> mapDirAndLabels);

    // 清空面板（切换目录时先清掉上一个目录的内容）
    void clearLabels();

private:
    // 标签树点击处理：构造函数中显式 connect，
    // 不再使用 on_<对象名>_<信号名> 的自动连接（connectSlotsByName）。
    void onTreeViewClicked(const QModelIndex &index);

    // 在最顶层找到指定内容的节点
    QStandardItem *getItem(QString text);

    Ui::FormCurDirLabels *ui;

    QStandardItemModel      *m_modelLabels;
};

#endif // FORMCURDIRLABELS_H
