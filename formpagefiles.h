/****************************************************************************
**
** 【文件列表】
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
#include <QEvent>

#include "myqfilesystemmodel.h"
#include "sqliteoperation.h"

class EditableDelegate;
class ThumbnailIconProvider;

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

    void updateToolButtons();

    void setAction(QAction *actionDeleteLabels);

    QString getCurDir();

signals:

    void sendLabels(QString sFilePath, QString sLabels);

public:
    // 刷新当前目录：既响应【刷新】按钮，也作为 FormFileBrowser::sendRefresh 信号的接收者
    void onBtnRefreshClicked();

protected:
    // 双击文件夹则进入该文件夹，双击文件则打开该文件。
    void onDoubleClicked(const QModelIndex &index);

    // 每次当前目录变化时，详情显示的窗口和列表显示的窗口都要重新设置跟索引
    void bothSetRootIndex(const QModelIndex &index);

    void onCustomContextMenuRequested(const QPoint &pos);

    // 鼠标离开缩略图视图时清除悬停高亮
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:

    void recvGotoFile(QString sFilePath);

    void onTvNavigationClicked(const QModelIndex &index);

private:
    // 以下处理函数全部在构造函数中显式 connect，
    // 不再使用 on_<对象名>_<信号名> 的自动连接（connectSlotsByName）。
    void onTvFilesCustomContextMenuRequested(const QPoint &pos);

    void onBackButtonClicked();

    void onForwardButtonClicked();

    void onToParentButtonClicked();

    void onDeleteLabels();

    void onTbDetailClicked();

    void onTbThumbnailClicked();

    void onTvFilesDoubleClicked(const QModelIndex &index);

    void onListViewCustomContextMenuRequested(const QPoint &pos);

    void onRecvLabels(QString sFilePath, QString sLabels);

    void recvDblClick(const QModelIndex &index);

    // 缩略图模式下双击标签区域时弹出输入框编辑该文件的标签（P2-2）。
    void onEditTag(const QModelIndex &index);

    // 切换缩略图尺寸档位（小/中/大）
    void setThumbnailSize(int iconSize);

    // 缩略图列表的悬停项变化
    void onThumbnailHoverChanged(const QModelIndex &index);

    // 应用排序（列：0 名称 / 1 大小 / 2 类型 / 3 修改日期）
    void applySort(int column, Qt::SortOrder order);

    // 当前可见视图中的当前项（缩略图模式取 listView，否则取 tvFiles）
    QModelIndex currentIndexInActiveView() const;

private:

    QTreeView               *m_tvFiles;             // 详情显示文件列表，由于QListView不能显示表头，故用QTreeView实现列表功能。
    QListView               *m_lstFiles;            // 缩略图显示文件列表
    MyQFileSystemModel      *m_modelFiles;
    EditableDelegate        *m_delegate;            // 缩略图模式的自绘代理
    ThumbnailIconProvider   *m_iconProvider;        // 缩略图生成器
    QAction                 *m_actionDeleteLabels;
    QStack<QString>         m_skBackward;           // 访问过的路径，用于后退功能
    QStack<QString>         m_skForward;            // 访问过的路径，用于前进功能
    QString                 m_sCurDir;             // 当前正在访问的路径
    int                     m_nIconSize;            // 当前缩略图边长（逻辑像素）
    int                     m_nSortColumn;          // 当前排序列
    Qt::SortOrder           m_sortOrder;            // 当前排序方向

private:
    Ui::FormPageFiles *ui;
};

#endif // FORMPAGEFILES_H
