#include "filebrowser.h"
#include "ui_filebrowser.h"
#include <QMenu>
#include <QDebug>
#include <QTimer>
#include <QTime>

#include "adsoperation.h"

FileBrowser::FileBrowser(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FileBrowser)
{
    ui->setupUi(this);

    initFileList();
    initFilteredFileList();

    // todo 后续将已经访问过的路径都添加上
    ui->lookInCombo->setEditText(m_sCurPath);

    setWindowStyleSheet();

    connect(m_modelFiles, &QFileSystemModel::directoryLoaded, this, &FileBrowser::onDirectoryLoaded);
    connect(ui->wFileNavigation, &FormFileNavigation::sendNavigationClicked, this, &FileBrowser::onTvNavigationClicked);
    connect(this, &FileBrowser::sendProgress, ui->pageProgressInfo, &FormPageProgressInfo::onRecvProgress);
}

FileBrowser::~FileBrowser()
{
    delete ui;
}

void FileBrowser::setAction(QAction *actionDeleteLabels)
{
    m_actionDeleteLabels = actionDeleteLabels;
    connect(m_actionDeleteLabels, &QAction::triggered, this, &FileBrowser::onDeleteLabels);
}

void FileBrowser::onDeleteLabels()
{
    QStringList qLSelFilePaths = getSelFilePath();
    foreach(QString sSelFilePath, qLSelFilePaths)
    {
        ADSOperation::deleteADS(sSelFilePath);
    }
}

void FileBrowser::initFileList()
{
    m_tvFiles = ui->tvFiles;
    m_tvFiles->setSelectionMode(QAbstractItemView::ExtendedSelection); // 设置可以选中多行
    m_modelFiles = new MyQFileSystemModel;
    m_tvFiles->setModel(m_modelFiles);

    m_modelFiles->setRootPath("");
    m_tvFiles->setRootIndex(m_modelFiles->index(""));
    m_tvFiles->setSortingEnabled(true);
    m_tvFiles->header()->setSortIndicator(0, Qt::AscendingOrder);

    // 设置不能展开节点，以使得QTreeView表现的像QListView一样。
    m_tvFiles->setItemsExpandable(false);

    // 设置表头
    // 拷贝自Qt源码的Src\qtbase\src\widgets\dialogs\qfiledialg.cpp
    QHeaderView *treeHeader = m_tvFiles->header();
    QFontMetrics fm(m_tvFiles->font());
    treeHeader->resizeSection(0, fm.horizontalAdvance(QLatin1String("wwwwwwwwwwwwwwwwwwwwwwwwww")));
    treeHeader->resizeSection(1, fm.horizontalAdvance(QLatin1String("128.88 GB")));
    treeHeader->resizeSection(2, fm.horizontalAdvance(QLatin1String("mp3Folder")));
    treeHeader->resizeSection(3, fm.horizontalAdvance(QLatin1String("10/29/81 02:02PM")));
    treeHeader->resizeSection(4, fm.horizontalAdvance(QLatin1String("10/29/81 02:02PM")));
    treeHeader->setContextMenuPolicy(Qt::ActionsContextMenu);

    m_sCurPath = "我的电脑";
}

void FileBrowser::initFilteredFileList()
{
    m_tvFilteredFiles = ui->tvFilteredFiles;
    m_modelFilteredFiles = new QStandardItemModel;
    m_tvFilteredFiles->setModel(m_modelFilteredFiles);

    m_gotoFilePath = new QAction("打开文件所在位置", this);
    connect(m_gotoFilePath, SIGNAL(triggered(bool)), this, SLOT(onGotoFilePath()));
}

void FileBrowser::curDirChanged(QString sCurDir)
{
    ui->lookInCombo->setEditText(sCurDir);
    m_skBackward.push_back(m_sCurPath);
    m_skForward.clear();
    m_sCurPath = sCurDir;
}

void FileBrowser::setWindowStyleSheet()
{
    QList<int> sizes;
    // 在保持比例的情况下，绝对值要尽量大
    sizes << 10000 << 40000;
    ui->splitter->setSizes(sizes);

    // 设置初始窗口大小
    resize(1500,800);

    // 初始显示文件列表
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}

bool FileBrowser::event(QEvent *event)
{
    if((event->type() == QEvent::Type::Paint))
    {
        updateToolButtons();
    }

    return QWidget::event(event);
}

void FileBrowser::updateToolButtons()
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

void FileBrowser::getFilesInCurDir(QVector<QString> &vtFilePath)
{

}

void FileBrowser::onGotoFilePath()
{
    int nCurRow = ui->tvFilteredFiles->currentIndex().row();
    QString sFilePath = m_modelFilteredFiles->item(nCurRow)->text();
    QString sFileDir = sFilePath.left(sFilePath.lastIndexOf("/"));
    m_tvFiles->setRootIndex(m_modelFiles->index(sFileDir));
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
    m_tvFiles->setCurrentIndex(m_modelFiles->index(sFilePath));
}

void FileBrowser::onTvNavigationClicked(const QModelIndex &index)
{
    if(ui->stackedWidget->currentWidget() == ui->pageProgressInfo)
    {
        return;
    }
    QString path = index.data(QFileSystemModel::FilePathRole).toString();
    m_tvFiles->setRootIndex(m_modelFiles->index(path));
    curDirChanged(path);
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}

QString FileBrowser::getCurPath()
{
    QString sCurPath;
    if(ui->stackedWidget->currentWidget() == ui->pageFiles)
    {
        QModelIndex curIndex = m_tvFiles->currentIndex();
        if(!curIndex.isValid())
        {
            return "/";
        }
        sCurPath = m_modelFiles->filePath(curIndex);
    }
    else if (ui->stackedWidget->currentWidget() == ui->pageFilteredFiles)
    {

    }
    return sCurPath;
}

QStringList FileBrowser::getSelFilePath()
{
    QStringList selFilePath;
    QModelIndexList selected = m_tvFiles->selectionModel()->selectedRows();
    QString sSelect;
    for (QModelIndexList::const_iterator cit = selected.begin(); cit != selected.end(); ++cit)
    {
        selFilePath << m_modelFiles->filePath(*cit);
    }
    return selFilePath;
}

QStringList FileBrowser::getSelDirs()
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
    return qLSelDirs;
}

void FileBrowser::showProcessPage()
{
    ui->stackedWidget->setCurrentWidget(ui->pageProgressInfo);
}

void FileBrowser::showFilesWidget()
{
    ui->stackedWidget->setCurrentWidget(ui->pageFiles);
}

void FileBrowser::showFilteredFile(QStringList qLFilteredFiles)
{
    ui->stackedWidget->setCurrentWidget(ui->pageFilteredFiles);

    m_modelFilteredFiles->removeRows(0, m_modelFilteredFiles->rowCount());
    foreach(QString sFiltededFile, qLFilteredFiles)
    {
        QStandardItem* item = new QStandardItem(sFiltededFile);
        m_modelFilteredFiles->appendRow(item);
    }
}

void FileBrowser::recvProcessInfo(QString sProcessedFilePath)
{
    emit sendProgress(sProcessedFilePath);
}

void FileBrowser::on_tvFiles_doubleClicked(const QModelIndex &index)
{
    QFileInfo fileInfo = m_modelFiles->fileInfo(index);
    if(fileInfo.isDir())
    {
        // 如果双击的是文件夹，则将该文件夹内的列表显示。
        QString path = index.data(QFileSystemModel::FilePathRole).toString();
        m_tvFiles->setRootIndex(m_modelFiles->index(path));
        curDirChanged(path);
    }
    else if(fileInfo.isFile())
    {
        // todo
        // 如果双击的是文件,则直接打开文件。
    }
}

void FileBrowser::on_tvFiles_customContextMenuRequested(const QPoint &pos)
{
    QMenu menu(m_tvFiles);
    menu.addAction(m_actionDeleteLabels);
    menu.exec(QCursor::pos());
}

void FileBrowser::on_tvFilteredFiles_customContextMenuRequested(const QPoint &position)
{
    QMenu menu(ui->tvFilteredFiles);
    menu.addAction(m_gotoFilePath);
    menu.exec(QCursor::pos());
}

void FileBrowser::onDirectoryLoaded(const QString &sDir)
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

void FileBrowser::on_btnStop_clicked()
{
    emit sendStop();
}

void FileBrowser::on_backButton_clicked()
{
    m_skForward.push_back(m_sCurPath);
    QString sDir = m_skBackward.pop();
    m_tvFiles->setRootIndex(m_modelFiles->index(sDir));
    ui->lookInCombo->setEditText(sDir);
    m_sCurPath = sDir;
}

void FileBrowser::on_forwardButton_clicked()
{
    m_skBackward.push_back(m_sCurPath);
    QString sDir = m_skForward.pop();
    m_tvFiles->setRootIndex(m_modelFiles->index(sDir));
    ui->lookInCombo->setEditText(sDir);
    m_sCurPath = sDir;
}

void FileBrowser::on_toParentButton_clicked()
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
    m_tvFiles->setRootIndex(m_modelFiles->index(sNewDirectory));
    m_skBackward.push_back(sFilePath);
    m_sCurPath = sNewDirectory;
}

void FileBrowser::on_btnRefresh_clicked()
{
    QModelIndex curIndex = m_modelFiles->index(m_sCurPath);
    m_modelFiles->setRootPath("");
    m_tvFiles->setRootIndex(curIndex);
}
