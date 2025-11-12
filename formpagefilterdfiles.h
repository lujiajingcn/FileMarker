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
    void onShowFiltedFiles(QStringList qLFilteredFiles);
    void onGotoFilePath();

private slots:
    // 右键菜单
    void on_tvFilteredFiles_customContextMenuRequested(const QPoint &pos);

private:
    Ui::FormPageFilterdFiles *ui;

    QTreeView               *m_tvFilteredFiles;
    QStandardItemModel      *m_modelFilteredFiles;
    QAction                 *m_gotoFilePath;
};

#endif // FORMPAGEFILTERDFILES_H
