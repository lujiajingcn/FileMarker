/**
** 【查询结果】
**
**/

#ifndef FORMPAGEFILTERDFILES_H
#define FORMPAGEFILTERDFILES_H

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QAction>

namespace Ui {
class FormPageFilterdFiles;
}

class FormPageFilterdFiles : public QWidget
{
    Q_OBJECT

public:
    explicit FormPageFilterdFiles(QWidget *parent = nullptr);
    ~FormPageFilterdFiles();

signals:
    void sendGotoFile(QString sFilePath);

public slots:
    void onShowFilteredFiles(QStringList qLFilteredFiles);
    void onGotoFilePath();

private:
    // 右键菜单处理函数：构造函数中显式 connect，
    // 不再使用 on_<对象名>_<信号名> 的自动连接（connectSlotsByName）。
    void onTvFilteredFilesCustomContextMenuRequested(const QPoint &pos);

private:
    Ui::FormPageFilterdFiles *ui;

    QTreeView               *m_tvFilteredFiles;
    QStandardItemModel      *m_modelFilteredFiles;
    QAction                 *m_gotoFilePath;
};

#endif // FORMPAGEFILTERDFILES_H
