#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <QWidget>
#include <QTreeView>
#include <QListView>
#include <QFileSystemModel>
#include <QStack>
#include <QAction>
#include <QStandardItemModel>

#include "myqfilesystemmodel.h"
#include "sqliteoperation.h"

namespace Ui {
class FileBrowser;
}

class FileBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit FileBrowser(QWidget *parent = nullptr);
    ~FileBrowser();

    // 初始化文件列表
    void initFileList();

    // 初始化根据标签筛选出的文件列表
    void initFilteredFileList();

    // 设置样式，大部分样式已经在filebrowser.ui中设置完成，在此进行少量样式设置
    void setWindowStyleSheet();

    void curDirChanged(QString sCurDir);

    // 获取当前选中文件的路径
    // 如果返回值为空，表示未选中文件
    QString getCurPath();

    QStringList getSelFilePath();

    QStringList getSelDirs();

    void showProcessPage();

    bool event(QEvent *event);
    void updateToolButtons();

    void getFilesInCurDir(QVector<QString> &vtFilePath);

    void showFilesWidget();

    void setAction(QAction *actionDeleteLabels);

signals:
    void sendStop();
    void sendProgress(QString sProcessedFilePath);
    void sendShowFiltedFiles(QStringList qLFilteredFiles);

public slots:
    void showFilteredFile(QStringList qLFilteredFiles);

    void recvProcessInfo(QString sProcessedFilePath);

    void onGotoFilePath(QString sFilePath);

    // 点击左侧导航栏中节点，将该节点下的文件夹及文件显示在右侧文件列表
    void onTvNavigationClicked(const QModelIndex &index);

    // 双击文件列表中的文件夹，进入该文件夹
    void on_tvFiles_doubleClicked(const QModelIndex &index);

    // 文件列表的右键菜单
    void on_tvFiles_customContextMenuRequested(const QPoint &pos);


public slots:
    void on_btnRefresh_clicked();

private slots:

    /** https://stackoverflow.com/questions/24716271/qt-qfilesystemmodel-how-to-get-files-in-folder-noob */
    void onDirectoryLoaded(const QString &sDir);

    void on_btnStop_clicked();

    void on_backButton_clicked();

    void on_forwardButton_clicked();

    void on_toParentButton_clicked();

    void onDeleteLabels();
private:

    QTreeView               *m_tvFiles;             // 文件列表，由于QListView不能显示表头，故用QTreeView实现列表功能。
    MyQFileSystemModel      *m_modelFiles;

    QAction                 *m_actionDeleteLabels;

    QStack<QString>         m_skBackward;           // 访问过的路径，用于后退功能
    QStack<QString>         m_skForward;            // 访问过的路径，用于前进功能
    QString                 m_sCurPath;             // 当前正在访问的路径

    Ui::FileBrowser *ui;
};

#endif // FILEBROWSER_H
