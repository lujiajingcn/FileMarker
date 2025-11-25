#include "formpagefiles.h"
#include "ui_formpagefiles.h"

#include <QMenu>
#include <QDesktopServices>
#include <QUrl>

#include "adsoperation.h"
#include "thumbnailiconprovider.h"
#include "editablefiledelegate.h"

FormPageFiles::FormPageFiles(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormPageFiles)
{
    ui->setupUi(this);

    m_modelFiles = new MyQFileSystemModel;
    // 设置根路径（例如当前目录）
    m_modelFiles->setRootPath("");

    initDetailFileList();
    initThumbnailFileList();

    // todo 后续将已经访问过的路径都添加上
    ui->lookInCombo->setEditText(m_sCurDir);

    connect(m_modelFiles, &QFileSystemModel::directoryLoaded, this, &FormPageFiles::onDirectoryLoaded);
    connect(m_modelFiles, &MyQFileSystemModel::sendLabels, this, &FormPageFiles::onRecvLabels);
}

FormPageFiles::~FormPageFiles()
{
    delete ui;
}

void FormPageFiles::setAction(QAction *actionDeleteLabels)
{
    m_actionDeleteLabels = actionDeleteLabels;
    connect(m_actionDeleteLabels, &QAction::triggered, this, &FormPageFiles::onDeleteLabels);
}

QString FormPageFiles::getCurDir()
{
    return m_sCurDir;
}

void FormPageFiles::onDeleteLabels()
{
    QStringList qLSelFilePaths = getSelFilePath();
    foreach(QString sSelFilePath, qLSelFilePaths)
    {
        ADSOperation::deleteADS(sSelFilePath);
    }
    on_btnRefresh_clicked();
}

void FormPageFiles::initDetailFileList()
{
    m_tvFiles = ui->tvFiles;
     // 设置可以选中多行
    m_tvFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tvFiles->setModel(m_modelFiles);

    m_tvFiles->setRootIndex(m_modelFiles->index(""));
    m_tvFiles->setSortingEnabled(true);
    m_tvFiles->header()->setSortIndicator(0, Qt::AscendingOrder);

    // 设置不能展开节点，以使得QTreeView表现的像QListView一样。
    m_tvFiles->setItemsExpandable(false);

    // 设置表头
    // 拷贝自Qt源码的Src\qtbase\src\widgets\dialogs\qfiledialg.cpp
    // 根据实际数据稍作修改，增加了列的宽度
    QHeaderView *treeHeader = m_tvFiles->header();
    QFontMetrics fm(m_tvFiles->font());
    treeHeader->resizeSection(0, fm.horizontalAdvance(QLatin1String("wwwwwwwwwwwwwwwwwwwwwwwwww")));
    treeHeader->resizeSection(1, fm.horizontalAdvance(QLatin1String("128.88 GB")) + 15);
    treeHeader->resizeSection(2, fm.horizontalAdvance(QLatin1String("mp3Folder")) + 35);
    treeHeader->resizeSection(3, fm.horizontalAdvance(QLatin1String("10/29/81 02:02PM")) + 5);
    treeHeader->resizeSection(4, fm.horizontalAdvance(QLatin1String("10/29/81 02:02PM")));
    treeHeader->setContextMenuPolicy(Qt::ActionsContextMenu);

    m_sCurDir = "我的电脑";
}

void FormPageFiles::initThumbnailFileList()
{
    m_lstFiles = ui->listView;

    // 创建并设置自定义图标提供器
    ThumbnailIconProvider *iconProvider = new ThumbnailIconProvider();
    m_modelFiles->setIconProvider(iconProvider);

    m_modelFiles->setReadOnly(false);

    // 创建列表视图
    m_lstFiles->setModel(m_modelFiles);

    // QTreeView和QListView共享同一个选择模型
    QItemSelectionModel *selectionModel = m_tvFiles->selectionModel();
    m_lstFiles->setSelectionModel(selectionModel);

    // 设置视图属性以显示缩略图
    m_lstFiles->setViewMode(QListView::IconMode);    // 图标模式
    m_lstFiles->setIconSize(QSize(ICON_SIZE, ICON_SIZE));        // 设置图标大小
    m_lstFiles->setGridSize(QSize(GRID_SIZE, GRID_SIZE));        // 设置网格大小，为图标留出空间
    m_lstFiles->setResizeMode(QListView::Adjust);    // 自动调整布局
    m_lstFiles->setMovement(QListView::Static);      // 禁止拖动
    m_lstFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_lstFiles->setContextMenuPolicy(Qt::CustomContextMenu);

    m_lstFiles->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    EditableDelegate *delegate = new EditableDelegate(this);
    m_lstFiles->setItemDelegate(delegate);

    connect(delegate, &EditableDelegate::sendDblClick, this, &FormPageFiles::recvDblClick);

    // 设置根索引为当前目录
    m_lstFiles->setRootIndex(m_modelFiles->index(""));
}

void FormPageFiles::curDirChanged(QString sCurDir)
{
    ui->lookInCombo->setEditText(sCurDir);
    m_skBackward.push_back(m_sCurDir);
    m_skForward.clear();
    m_sCurDir = sCurDir;
}

bool FormPageFiles::event(QEvent *event)
{
    if((event->type() == QEvent::Type::Paint))
    {
        updateToolButtons();
    }

    return QWidget::event(event);
}

void FormPageFiles::updateToolButtons()
{
    if(m_skBackward.count() == 0)
        ui->backButton->setEnabled(false);
    else
        ui->backButton->setEnabled(true);

    if(m_skForward.count() == 0)
        ui->forwardButton->setEnabled(false);
    else
        ui->forwardButton->setEnabled(true);

    QModelIndex rootIndex = m_tvFiles->rootIndex();
    QString sFilePath = m_modelFiles->filePath(rootIndex);
    if(sFilePath.isEmpty())
        ui->toParentButton->setEnabled(false);
    else
        ui->toParentButton->setEnabled(true);
}

QStringList FormPageFiles::getSelFilePath()
{
    QStringList selFilePath;
    QModelIndexList selected = m_tvFiles->selectionModel()->selectedIndexes();
    QString sSelect;
    for (QModelIndexList::const_iterator cit = selected.begin(); cit != selected.end(); ++cit)
    {
        selFilePath << m_modelFiles->filePath(*cit);
    }
    return selFilePath;
}

QStringList FormPageFiles::getSelDirs()
{
    QStringList qLSelDirs;
    QModelIndexList selected = m_tvFiles->selectionModel()->selectedRows();
    QString sSelect;
    for (QModelIndexList::const_iterator cit = selected.begin(); cit != selected.end(); ++cit)
    {
        QString sFilePath = m_modelFiles->filePath(*cit);
        QFileInfo fileInfo(sFilePath);
        if(fileInfo.isDir())
        {
            qLSelDirs << sFilePath;
        }
    }
    if(qLSelDirs.isEmpty())
    {
        qLSelDirs << m_sCurDir;
    }
    return qLSelDirs;
}

void FormPageFiles::on_tvFiles_doubleClicked(const QModelIndex &index)
{
    onDoubleClicked(index);
}

void FormPageFiles::onCustomContextMenuRequested(const QPoint &pos)
{
    QMenu menu(m_tvFiles);
    QAction *actionDetail = new QAction("详细信息");
    QAction *actionThumbnail = new QAction("缩略图");
    connect(actionDetail, &QAction::triggered, this, &FormPageFiles::on_tbDetail_clicked);
    connect(actionThumbnail, &QAction::triggered, this, &FormPageFiles::on_tbThumbnail_clicked);
    QMenu menuShowType("查看");
    menuShowType.addAction(actionDetail);
    menuShowType.addAction(actionThumbnail);
//    menu.addAction(m_actionDeleteLabels);
    menu.addMenu(&menuShowType);
    menu.exec(QCursor::pos());
}

void FormPageFiles::on_tvFiles_customContextMenuRequested(const QPoint &pos)
{
    onCustomContextMenuRequested(pos);
}

void FormPageFiles::onDirectoryLoaded(const QString &sDir)
{
    QModelIndex parentIndex = m_modelFiles->index(sDir);
    int numRows = m_modelFiles->rowCount(parentIndex);
    for (int row = 0; row < numRows; ++row) {
        QModelIndex childIndex = m_modelFiles->index(row, 0, parentIndex);
        QString sFileName = m_modelFiles->data(childIndex).toString();

        QString sFilePath = sDir + "/" + sFileName;
        sFilePath = sFilePath.replace("//","/");
    }
}

void FormPageFiles::on_backButton_clicked()
{
    m_skForward.push_back(m_sCurDir);
    QString sDir = m_skBackward.pop();
    bothSetRootIndex(m_modelFiles->index(sDir));
    ui->lookInCombo->setEditText(sDir);
    m_sCurDir = sDir;
}

void FormPageFiles::on_forwardButton_clicked()
{
    m_skBackward.push_back(m_sCurDir);
    QString sDir = m_skForward.pop();
    bothSetRootIndex(m_modelFiles->index(sDir));
    ui->lookInCombo->setEditText(sDir);
    m_sCurDir = sDir;
}

void FormPageFiles::on_toParentButton_clicked()
{
    QModelIndex rootIndex = m_tvFiles->rootIndex();
    QString sFilePath = m_modelFiles->filePath(rootIndex);
    QDir dir(sFilePath);
    QString sNewDirectory;
    if (dir.isRoot())
    {
        sNewDirectory = m_modelFiles->myComputer().toString();
    }
    else
    {
        dir.cdUp();
        sNewDirectory = dir.absolutePath();
    }
    bothSetRootIndex(m_modelFiles->index(sNewDirectory));
    m_skBackward.push_back(sFilePath);
    m_sCurDir = sNewDirectory;
}

void FormPageFiles::on_btnRefresh_clicked()
{
    QModelIndex curIndex = m_modelFiles->index(m_sCurDir);
    m_modelFiles->setRootPath("");
    m_tvFiles->setRootIndex(curIndex);
    m_lstFiles->viewport()->update();
}

void FormPageFiles::recvGotoFile(QString sFilePath)
{
    QString sFileDir = sFilePath.left(sFilePath.lastIndexOf("/"));
    m_tvFiles->setRootIndex(m_modelFiles->index(sFileDir));
    m_tvFiles->setCurrentIndex(m_modelFiles->index(sFilePath));
}

void FormPageFiles::onTvNavigationClicked(const QModelIndex &index)
{
    QString path = index.data(QFileSystemModel::FilePathRole).toString();
    m_tvFiles->setRootIndex(m_modelFiles->index(path));
    ui->stackedWidget->setCurrentIndex(FILEPAGE::DETAIL);
    curDirChanged(path);
}

void FormPageFiles::on_tbDetail_clicked()
{
    ui->stackedWidget->setCurrentIndex(FILEPAGE::DETAIL);
}

void FormPageFiles::on_tbThumbnail_clicked()
{
    ui->stackedWidget->setCurrentIndex(FILEPAGE::THUMBNAIL);
}

//void FormPageFiles::on_listView_doubleClicked(const QModelIndex &index)
//{
//    onDoubleClicked(index);
//}

void FormPageFiles::onDoubleClicked(const QModelIndex &index)
{
    QFileInfo fileInfo = m_modelFiles->fileInfo(index);
    QString path = index.data(QFileSystemModel::FilePathRole).toString();
    if(fileInfo.isDir())
    {
        bothSetRootIndex(m_modelFiles->index(path));
        curDirChanged(path);
    }
    else if(fileInfo.isFile())
    {
        QUrl url = QUrl::fromLocalFile(path);
        QDesktopServices::openUrl(url);
    }
}

void FormPageFiles::bothSetRootIndex(const QModelIndex &index)
{
    m_tvFiles->setRootIndex(index);
    m_lstFiles->setRootIndex(index);
}

void FormPageFiles::on_listView_customContextMenuRequested(const QPoint &pos)
{
    onCustomContextMenuRequested(pos);
}

void FormPageFiles::onRecvLabels(QString sLabels)
{
    emit sendLabels(sLabels);
}

void FormPageFiles::recvDblClick(const QModelIndex &index)
{
    onDoubleClicked(index);
}
