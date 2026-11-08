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

    void clear();
signals:
    void sendStop();

public slots:
    void onRecvProgress(QString sProcessedFilePath);

private:
    // 停止按钮处理函数：构造函数中显式 connect，
    // 不再使用 on_<对象名>_<信号名> 的自动连接（connectSlotsByName）。
    void onBtnStopClicked();

private:
    Ui::FormPageProgressInfo *ui;
};

#endif // FORMPAGEPROGRESSINFO_H
