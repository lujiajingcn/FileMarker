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

private slots:
    void on_treeView_clicked(const QModelIndex &index);

private:
    Ui::FormCurDirLabels *ui;

    QStandardItemModel      *m_modelLabels;

};

#endif // FORMCURDIRLABELS_H
