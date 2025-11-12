#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <QWidget>

namespace Ui {
class FileBrowser;
}

class FileBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit FileBrowser(QWidget *parent = nullptr);
    ~FileBrowser();

    // 设置样式，大部分样式已经在filebrowser.ui中设置完成，在此进行少量样式设置
    void setWindowStyleSheet();

    void setAction(QAction *action);

    QStringList getSelFilePath();

    QStringList getSelDirs();

signals:
    void sendStop();
    void sendProgress(QString sProcessedFilePath);
    void sendShowFiltedFiles(QStringList qLFilteredFiles);
    void sendRefresh();
    void sendGotoFile(QString sFilePath);
    void sendNavigationClicked(const QModelIndex &index);


public:
    void showProcessPage();
    void showFilesWidget();


public slots:
    void showFilteredFile(QStringList qLFilteredFiles);

    void recvProcessInfo(QString sProcessedFilePath);

    void onGotoFilePath(QString sFilePath);

    void onTvNavigationClicked(const QModelIndex &index);

    void onRecvRefresh();


private slots:
    void on_btnStop_clicked();

private:
    Ui::FileBrowser *ui;
};

#endif // FILEBROWSER_H
