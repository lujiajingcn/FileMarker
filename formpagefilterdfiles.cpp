#include "formpagefilterdfiles.h"
#include "ui_formpagefilterdfiles.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenu>
#include <QTimer>
#include <QElapsedTimer>
#include <QUrl>

#include "adsoperation.h"

FormPageFilterdFiles::FormPageFilterdFiles(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormPageFilterdFiles)
{
    ui->setupUi(this);

    m_tvFilteredFiles = ui->tvFilteredFiles;
    m_modelFilteredFiles = new QStandardItemModel(this);
    m_modelFilteredFiles->setHorizontalHeaderLabels(QStringList() << tr("名称") << tr("路径") << tr("标签"));
    m_tvFilteredFiles->setModel(m_modelFilteredFiles);
    m_tvFilteredFiles->setRootIsDecorated(false);
    m_tvFilteredFiles->setAlternatingRowColors(true);
    m_tvFilteredFiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tvFilteredFiles->setTextElideMode(Qt::ElideMiddle);
    m_tvFilteredFiles->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tvFilteredFiles->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tvFilteredFiles->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    m_gotoFilePath = new QAction(tr("打开文件所在位置"), this);
    m_openFile = new QAction(tr("打开"), this);
    m_copyFilePath = new QAction(tr("复制路径"), this);

    // 标签列分片读取：搜索结果可能上千条，一次性枚举 ADS 会卡住界面
    m_pTagScanTimer = new QTimer(this);
    m_pTagScanTimer->setInterval(0);
    m_nTagScanRow = 0;
    connect(m_pTagScanTimer, &QTimer::timeout, this, &FormPageFilterdFiles::continueFillTags);

    connect(m_gotoFilePath, &QAction::triggered, this, &FormPageFilterdFiles::onGotoFilePath);
    connect(m_openFile, &QAction::triggered, this, [this]() {
        const QString sFilePath = currentFilePath();
        if (!sFilePath.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(sFilePath));
    });
    connect(m_copyFilePath, &QAction::triggered, this, &FormPageFilterdFiles::onCopyFilePath);

    // “返回目录”：由外层 FormFileBrowser 切回文件列表页面
    connect(ui->btnBack, &QToolButton::clicked, this, &FormPageFilterdFiles::sendBackRequested);
    connect(m_tvFilteredFiles, &QTreeView::doubleClicked,
            this, &FormPageFilterdFiles::onTvFilteredFilesDoubleClicked);

    // 显式连接右键菜单信号（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(m_tvFilteredFiles, &QTreeView::customContextMenuRequested,
            this, &FormPageFilterdFiles::onTvFilteredFilesCustomContextMenuRequested);
}

FormPageFilterdFiles::~FormPageFilterdFiles()
{
    delete ui;
}

void FormPageFilterdFiles::onShowFilteredFiles(QStringList qLFilteredFiles)
{
    if (m_pTagScanTimer != nullptr)
        m_pTagScanTimer->stop();

    m_modelFilteredFiles->removeRows(0, m_modelFilteredFiles->rowCount());

    // 先只填名称/路径（很快），标签列交给定时器分片补齐
    m_qLTagScanPaths = qLFilteredFiles;
    m_nTagScanRow = 0;
    for (const QString &sFilePath : qLFilteredFiles) {
        const QFileInfo fileInfo(sFilePath);
        QList<QStandardItem *> items;
        items << new QStandardItem(fileInfo.fileName())
              << new QStandardItem(QDir::toNativeSeparators(sFilePath))
              << new QStandardItem(QString());
        m_modelFilteredFiles->appendRow(items);
    }

    // 路径列显示完整路径，避免同名文件难区分
    ui->lblResultCount->setText(tr("搜索结果 %1 项").arg(qLFilteredFiles.size()));

    if (m_pTagScanTimer != nullptr && !m_qLTagScanPaths.isEmpty())
        m_pTagScanTimer->start();
}

void FormPageFilterdFiles::continueFillTags()
{
    if (m_nTagScanRow >= m_qLTagScanPaths.size()) {
        if (m_pTagScanTimer != nullptr)
            m_pTagScanTimer->stop();
        return;
    }

    QElapsedTimer budget;
    budget.start();
    while (m_nTagScanRow < m_qLTagScanPaths.size() && budget.elapsed() < 15) {
        QStandardItem *tagItem = m_modelFilteredFiles->item(m_nTagScanRow, 2);
        if (tagItem != nullptr) {
            const QStringList qLTags = ADSOperation::listADSTagsInOrder(m_qLTagScanPaths.at(m_nTagScanRow));
            tagItem->setText(qLTags.join(","));
        }
        ++m_nTagScanRow;
    }

    if (m_nTagScanRow >= m_qLTagScanPaths.size() && m_pTagScanTimer != nullptr)
        m_pTagScanTimer->stop();
}

QString FormPageFilterdFiles::currentFilePath() const
{
    const QModelIndex curIdx = m_tvFilteredFiles->currentIndex();
    if (!curIdx.isValid())
        return QString();
    QStandardItem *item = m_modelFilteredFiles->item(curIdx.row(), 1);
    return item != nullptr ? item->text() : QString();
}

void FormPageFilterdFiles::onTvFilteredFilesDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    QStandardItem *item = m_modelFilteredFiles->item(index.row(), 1);
    if (item == nullptr || item->text().isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(item->text()));
}

void FormPageFilterdFiles::onGotoFilePath()
{
    const QString sFilePath = currentFilePath();
    if (sFilePath.isEmpty())
        return;
    emit sendGotoFile(sFilePath);
}

void FormPageFilterdFiles::onCopyFilePath()
{
    const QString sFilePath = currentFilePath();
    if (sFilePath.isEmpty())
        return;
    QApplication::clipboard()->setText(sFilePath);
}

void FormPageFilterdFiles::onTvFilteredFilesCustomContextMenuRequested(const QPoint &pos)
{
    // 右键先定位到鼠标所在行，否则菜单会作用到上一次选中的文件（QTreeView 右键不会自动改 currentIndex）
    const QModelIndex clickedIndex = m_tvFilteredFiles->indexAt(pos);
    m_tvFilteredFiles->setCurrentIndex(clickedIndex);
    if (!clickedIndex.isValid())
        m_tvFilteredFiles->clearSelection();

    const bool hasSelection = clickedIndex.isValid();

    QMenu menu(ui->tvFilteredFiles);
    menu.addAction(m_openFile);
    menu.addAction(m_gotoFilePath);
    menu.addAction(m_copyFilePath);
    m_openFile->setEnabled(hasSelection);
    m_gotoFilePath->setEnabled(hasSelection);
    m_copyFilePath->setEnabled(hasSelection);
    menu.exec(m_tvFilteredFiles->viewport()->mapToGlobal(pos));
}
