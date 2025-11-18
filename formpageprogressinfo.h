/**
** 【查询过程】
**
**/
#ifndef FORMPAGEPROGRESSINFO_H
#define FORMPAGEPROGRESSINFO_H

#include <QWidget>

namespace Ui {
class FormPageProgressInfo;
}

class FormPageProgressInfo : public QWidget
{
    Q_OBJECT

public:
    explicit FormPageProgressInfo(QWidget *parent = nullptr);
    ~FormPageProgressInfo();

signals:
    void sendStop();

public slots:
    void onRecvProgress(QString sProcessedFilePath);

private slots:
    void on_btnStop_clicked();

private:
    Ui::FormPageProgressInfo *ui;
};

#endif // FORMPAGEPROGRESSINFO_H
