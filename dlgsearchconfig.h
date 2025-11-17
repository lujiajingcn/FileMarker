/**
** 查找条件类
**
** 用来设置查找操作时的条件，并将设置保存为配置文件
**/
#ifndef DLGSEARCHCONFIG_H
#define DLGSEARCHCONFIG_H

#include <QDialog>

namespace Ui {
class DlgSearchConfig;
}

class DlgSearchConfig : public QDialog
{
    Q_OBJECT

public:
    explicit DlgSearchConfig(QWidget *parent = nullptr, QString sConfigFilePath = "");
    ~DlgSearchConfig();

protected:
    void readSearchConfig();

    void writeSearchConfig();

private slots:
    void on_btnOk_clicked();

    void on_btnCancel_clicked();

private:
    QString         m_sConfigFilePath;

    Ui::DlgSearchConfig *ui;
};

#endif // DLGSEARCHCONFIG_H
