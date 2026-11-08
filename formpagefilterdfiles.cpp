#include "formpagefilterdfiles.h"
#include "ui_formpagefilterdfiles.h"
#include <QMenu>

FormPageFilterdFiles::FormPageFilterdFiles(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormPageFilterdFiles)
{
    ui->setupUi(this);

    m_tvFilteredFiles = ui->tvFilteredFiles;
    m_modelFilteredFiles = new QStandardItemModel(this);
    m_tvFilteredFiles->setModel(m_modelFilteredFiles);

    m_gotoFilePath = new QAction("打开文件所在位置", this);
    // 显式连接：原先使用 SIGNAL/SLOT 字符串形式的 connect()
    connect(m_gotoFilePath, &QAction::triggered, this, &FormPageFilterdFiles::onGotoFilePath);

    // 显式连接右键菜单信号（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(m_tvFilteredFiles, &QTreeView::customContextMenuRequested,
            this, &FormPageFilterdFiles::onTvFilteredFilesCustomContextMenuRequested);
}

void FormPageFilterdFiles::onShowFilteredFiles(QStringList qLFilteredFiles)
{
    m_modelFilteredFiles->removeRows(0, m_modelFilteredFiles->rowCount());
    foreach(QString sFiltededFile, qLFilteredFiles)
    {
        QStandardItem* item = new QStandardItem(sFiltededFile);
        m_modelFilteredFiles->appendRow(item);
    }
}

void FormPageFilterdFiles::onGotoFilePath()
{
    QModelIndex curIdx = ui->tvFilteredFiles->currentIndex();
    if (!curIdx.isValid())
        return;

    // P3-31：用 itemFromIndex 取第 0 列对应项，避免非 0 列索引或行号越界等问题。
    QStandardItem *item = m_modelFilteredFiles->itemFromIndex(curIdx.siblingAtColumn(0));
    if (item == nullptr)
        return;

    QString sFilePath = item->text();
    if (sFilePath.isEmpty())
        return;

    emit sendGotoFile(sFilePath);
}

void FormPageFilterdFiles::onTvFilteredFilesCustomContextMenuRequested(const QPoint &pos)
{
    QMenu menu(ui->tvFilteredFiles);
    menu.addAction(m_gotoFilePath);
    menu.exec(QCursor::pos());
}

FormPageFilterdFiles::~FormPageFilterdFiles()
{
    delete ui;
}
