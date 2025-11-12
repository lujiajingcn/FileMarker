/****************************************************************************
**
** 文件列表类
**
** 用来显示文件列表，包括详情显示和缩略图显示
**
****************************************************************************/

#ifndef FORMPAGEFILES_H
#define FORMPAGEFILES_H

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
class FormPageFiles;
}

enum FILEPAGE
{
    DETAIL,
    THUMBNAIL
};

class FormPageFiles : public QWidget
{
    Q_OBJECT

public:
    explicit FormPageFiles(QWidget *parent = nullptr);
    ~FormPageFiles();

    // 初始化详情显示的文件列表
    void initDetailFileList();

    // 初始化缩略图显示的文件列表
    void initThumbnailFileList();

    void curDirChanged(QString sCurDir);

    QStringList getSelFilePath();

    QStringList getSelDirs();

    bool event(QEvent *event);
    void updateToolButtons();

    void setAction(QAction *actionDeleteLabels);

public slots:

    // 双击文件列表中的文件夹，进入该文件夹
    void on_tvFiles_doubleClicked(const QModelIndex &index);

    // 文件列表的右键菜单
    void on_tvFiles_customContextMenuRequested(const QPoint &pos);

    void on_btnRefresh_clicked();

    void recvGotoFile(QString sFilePath);

    void onTvNavigationClicked(const QModelIndex &index);

private slots:

    /** https://stackoverflow.com/questions/24716271/qt-qfilesystemmodel-how-to-get-files-in-folder-noob */
    void onDirectoryLoaded(const QString &sDir);

    void on_backButton_clicked();

    void on_forwardButton_clicked();

    void on_toParentButton_clicked();

    void onDeleteLabels();
    void on_tbDetail_clicked();

    void on_tbThumbnail_clicked();

private:

    QTreeView               *m_tvFiles;             // 详情显示文件列表，由于QListView不能显示表头，故用QTreeView实现列表功能。
    QListView               *m_lstFiles;            // 缩略图显示文件列表
    MyQFileSystemModel      *m_modelFiles;
    QAction                 *m_actionDeleteLabels;
    QStack<QString>         m_skBackward;           // 访问过的路径，用于后退功能
    QStack<QString>         m_skForward;            // 访问过的路径，用于前进功能
    QString                 m_sCurPath;             // 当前正在访问的路径

private:
    Ui::FormPageFiles *ui;
};

#endif // FORMPAGEFILES_H
