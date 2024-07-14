#ifndef DLGLABEL_H
#define DLGLABEL_H

#include <QDialog>
#include <QListView>
#include <QStandardItemModel>
#include "sqliteoperation.h"

namespace Ui {
class DlgLabel;
}

class DlgLabel : public QDialog
{
    Q_OBJECT

public:
    explicit DlgLabel(QWidget *parent = nullptr);
    ~DlgLabel();

    void setIsDir(bool bIsDir);
    void setAllLabels(QVector<QString> vtAllLabels);
    void setSelLabels(QStringList labels);

    void setLabel(QString sLabel);
    QStringList getLabels();

    QStringList getAllLabels();

    // 设置当前文件夹和文件夹中的全部文件，还是只是设置当前文件夹
    bool isSetAllFiles();

    void setSqlOperation(SqliteOperation *sqlOperation);

private slots:
    void on_btnOk_clicked();

    void on_btnCancel_clicked();

    void on_lvAllLabels_clicked(const QModelIndex &index);

    void on_btnMoveRight_clicked();

    void on_btnMoveLeft_clicked();

    void on_lvAllLabels_doubleClicked(const QModelIndex &index);

    void on_lvSelLabels_doubleClicked(const QModelIndex &index);

    void on_btnAddLable_clicked();

    void on_btnDelLabel_clicked();

    void on_btnModLabel_clicked();

private:
    bool isItemExist(QStandardItemModel *model, QString sText);

private:
    QStringList             m_sAllLabels;
    QStringList             m_sLabels;

    QListView               *m_lvAllLabels;         // 所有标签列表
    QStandardItemModel      *m_modelAllLabels;

    QListView               *m_lvSelLabels;         // 选中标签列表
    QStandardItemModel      *m_modelSelLabels;

    SqliteOperation         *m_sqlOperation;         // 操作sqlite数据库

    Ui::DlgLabel            *ui;
};

#endif // DLGLABEL_H
