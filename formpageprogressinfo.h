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

public slots:
    void onRecvProgress(QString sProcessedFilePath);

private:
    Ui::FormPageProgressInfo *ui;
};

#endif // FORMPAGEPROGRESSINFO_H
