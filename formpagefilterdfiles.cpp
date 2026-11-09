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
#include <QProcess>
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
    // 标签列异步逐行 setText，QHeaderView 的 ResizeToContents 不会随 dataChanged 重算，
    // 容易一直按“空标签”时的窄宽度显示；这里改用固定初始宽度的 Interactive。
    m_tvFilteredFiles->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tvFilteredFiles->header()->resizeSection(2, 180);
    m_tvFilteredFiles->header()->setMinimumSectionSize(60);

    // 【B8】两个"定位文件"拆成两个名字互相区分的行为：
    //   - 在资源管理器中显示：打开系统资源管理器并选中文件（与文件列表右键一致）
    //   - 在文件列表中打开所在目录：把程序自己的文件列表切到该目录
    m_gotoFilePath = new QAction(tr("在文件列表中打开所在目录"), this);
    m_revealInExplorer = new QAction(tr("在资源管理器中显示"), this);
    m_openFile = new QAction(tr("打开"), this);
    m_copyFilePath = new QAction(tr("复制路径"), this);

    // 标签列分片读取：搜索结果可能上千条，一次性枚举 ADS 会卡住界面
    m_pTagScanTimer = new QTimer(this);
    m_pTagScanTimer->setInterval(0);
    m_nTagScanRow = 0;
    connect(m_pTagScanTimer, &QTimer::timeout, this, &FormPageFilterdFiles::continueFillTags);

    connect(m_gotoFilePath, &QAction::triggered, this, &FormPageFilterdFiles::onGotoFilePath);
    connect(m_revealInExplorer, &QAction::triggered, this, [this]() {
        const QString sFilePath = currentFilePath();
        if (sFilePath.isEmpty())
            return;
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                QStringList() << (QStringLiteral("/select,") + QDir::toNativeSeparators(sFilePath)));
    });
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
    QString sCountText = tr("搜索结果 %1 项").arg(qLFilteredFiles.size());
    if (!m_sSearchCondition.isEmpty())
        sCountText += QStringLiteral("（") + m_sSearchCondition + QStringLiteral("）");
    // 【C12】把结果来源写出来：缓存可能过期，用户至少要能看到这次是从哪查的
    if (!m_sSearchSource.isEmpty())
        sCountText += QStringLiteral("  ") + m_sSearchSource;
    ui->lblResultCount->setText(sCountText);

    if (m_pTagScanTimer != nullptr && !m_qLTagScanPaths.isEmpty())
        m_pTagScanTimer->start();
}

void FormPageFilterdFiles::setSearchCondition(const QString &sCondition)
{
    m_sSearchCondition = sCondition;
}

void FormPageFilterdFiles::setSearchSource(const QString &sSource)
{
    m_sSearchSource = sSource;
}

void FormPageFilterdFiles::hideEvent(QHideEvent *event)
{
    // 返回文件页后不要再枚举隐藏页的 ADS
    if (m_pTagScanTimer != nullptr)
        m_pTagScanTimer->stop();
    QWidget::hideEvent(event);
}

void FormPageFilterdFiles::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 标签还没填完就切回来时继续填
    if (m_pTagScanTimer != nullptr && m_nTagScanRow < m_qLTagScanPaths.size()
            && !m_pTagScanTimer->isActive()) {
        m_pTagScanTimer->start();
    }
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
    menu.addAction(m_revealInExplorer);
    menu.addAction(m_gotoFilePath);
    menu.addAction(m_copyFilePath);
    m_openFile->setEnabled(hasSelection);
    m_revealInExplorer->setEnabled(hasSelection);
    m_gotoFilePath->setEnabled(hasSelection);
    m_copyFilePath->setEnabled(hasSelection);
    menu.exec(m_tvFilteredFiles->viewport()->mapToGlobal(pos));
}
