#include "formpagefilterdfiles.h"
#include "ui_formpagefilterdfiles.h"

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <QUrl>

#include "adsoperation.h"
#include "searchmatch.h"
#include "utility.h"

FormPageFilterdFiles::FormPageFilterdFiles(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormPageFilterdFiles)
{
    ui->setupUi(this);

    m_filteredFilesTree = ui->tvFilteredFiles;
    m_modelFilteredFiles = new QStandardItemModel(this);
    m_modelFilteredFiles->setHorizontalHeaderLabels(QStringList() << tr("名称") << tr("路径") << tr("标签"));
    m_filteredFilesTree->setModel(m_modelFilteredFiles);
    m_filteredFilesTree->setRootIsDecorated(false);
    m_filteredFilesTree->setAlternatingRowColors(true);
    m_filteredFilesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // 结果摘要可能有多行（计数/来源、标签、目录）：允许换行，"重新扫描"贴住第一行
    ui->lblResultCount->setWordWrap(true);
    // wordWrap 的 QLabel 默认 sizeHint 很窄，会被旁边的 spacer 挤成两三个字一行；
    // 显式给 Expanding，让每行占满可用宽度、只在真的超宽时才折行。
    ui->lblResultCount->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->hBoxResultLine->setAlignment(ui->btnRescan, Qt::AlignTop);

    m_filteredFilesTree->setTextElideMode(Qt::ElideMiddle);
    m_filteredFilesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_filteredFilesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    // 标签列异步逐行 setText，QHeaderView 的 ResizeToContents 不会随 dataChanged 重算，
    // 容易一直按“空标签”时的窄宽度显示；这里改用固定初始宽度的 Interactive。
    m_filteredFilesTree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_filteredFilesTree->header()->resizeSection(2, 180);
    m_filteredFilesTree->header()->setMinimumSectionSize(60);

    // 两个"定位文件"拆成两个名字互相区分的行为：
    //   - 在资源管理器中显示：打开系统资源管理器并选中文件（与文件列表右键一致）
    //   - 在文件列表中打开所在目录：把程序自己的文件列表切到该目录
    m_gotoFilePath = new QAction(tr("在文件列表中打开所在目录"), this);
    m_revealInExplorer = new QAction(tr("在资源管理器中显示"), this);
    m_openFile = new QAction(tr("打开"), this);
    m_copyFilePath = new QAction(tr("复制路径"), this);
    m_editTags = new QAction(tr("编辑标签…"), this);
    m_editTags->setShortcut(QKeySequence(Qt::Key_F4));
    m_clearTags = new QAction(tr("清除标签"), this);
    m_renameFile = new QAction(tr("重命名…"), this);
    m_renameFile->setShortcut(QKeySequence(Qt::Key_F2));

    // 标签列分片读取：搜索结果可能上千条，一次性枚举 ADS 会卡住界面
    // 结果行分片填充定时器（和标签扫描定时器各管一件事）
    m_rowFillTimer = new QTimer(this);
    m_rowFillTimer->setInterval(0);
    connect(m_rowFillTimer, &QTimer::timeout, this, &FormPageFilterdFiles::continueAddRows);

    m_tagScanTimer = new QTimer(this);
    m_tagScanTimer->setInterval(0);
    m_tagScanRow = 0;
    connect(m_tagScanTimer, &QTimer::timeout, this, &FormPageFilterdFiles::continueFillTags);

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
    connect(m_editTags, &QAction::triggered, this, [this]() {
        const QString sFilePath = currentFilePath();
        if (!sFilePath.isEmpty())
            emit sendEditTagsRequested(QStringList() << sFilePath);
    });
    connect(m_clearTags, &QAction::triggered, this, [this]() {
        emit sendClearTagsRequested(selectedFilePaths());
    });
    connect(m_renameFile, &QAction::triggered, this, [this]() { renameCurrentFileInteractive(); });
    // 结果页也支持 F2/F4：动作挂到结果树上（WidgetWithChildrenShortcut 才能生效）
    addAction(m_renameFile);
    addAction(m_editTags);
    m_renameFile->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_editTags->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    // “返回目录”：由外层 FormFileBrowser 切回文件列表页面
    connect(ui->btnBack, &QToolButton::clicked, this, &FormPageFilterdFiles::sendBackRequested);
    // 【重新扫描】：让外层忽略缓存、重新走一遍磁盘搜索
    connect(ui->btnRescan, &QToolButton::clicked, this, &FormPageFilterdFiles::sendRescanRequested);
    connect(m_filteredFilesTree, &QTreeView::doubleClicked,
            this, &FormPageFilterdFiles::onTvFilteredFilesDoubleClicked);

    // 显式连接右键菜单信号（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(m_filteredFilesTree, &QTreeView::customContextMenuRequested,
            this, &FormPageFilterdFiles::onTvFilteredFilesCustomContextMenuRequested);
}

FormPageFilterdFiles::~FormPageFilterdFiles()
{
    delete ui;
}

void FormPageFilterdFiles::onShowFilteredFiles(const QStringList &qLFilteredFiles)
{
    if (m_tagScanTimer != nullptr)
        m_tagScanTimer->stop();
    if (m_rowFillTimer != nullptr)
        m_rowFillTimer->stop();

    m_modelFilteredFiles->removeRows(0, m_modelFilteredFiles->rowCount());

    // 结果行分片填充：整盘查找一次可能上万条，原来一次性 appendRow 会卡住界面
    // （而且名称列是 ResizeToContents，每插一行都要重算列宽，越填越慢）。
    m_rowSourcePaths = qLFilteredFiles;
    m_rowSourceRow = 0;
    m_tagScanPaths = qLFilteredFiles;
    m_tagScanRow = 0;
    m_staleCount = 0;

    // 摘要前几行：①计数+来源 ②中断提示（如有）；标签/目录由 rebuildSummaryText()
    // 按当前宽度排版（目录只在条目之间换行，不会把路径从中间劈开）。
    QString sCountLine = tr("搜索结果 %1 项").arg(qLFilteredFiles.size());
    if (!m_searchSource.isEmpty())
        sCountLine += QStringLiteral("  ") + m_searchSource;

    QStringList qLHeadLines;
    qLHeadLines << sCountLine;
    if (m_bInterrupted) {
        // 用户中途按了停止：结果只是"扫到的部分"，必须让人知道，
        // 否则很容易误判成"这些文件没有该标签"。
        qLHeadLines << tr("⚠ 搜索已中断：结果不完整，可点【重新扫描】重搜");
    }
    m_summaryHeadText = qLHeadLines.join(QLatin1Char('\n'));

    if (qLFilteredFiles.isEmpty()) {
        rebuildSummaryText();
        return;
    }

    // 填充期间先关掉"按内容自适应列宽"，否则每插入一行都要重算一次列宽
    m_filteredFilesTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    updateResultCountLabel(tr("正在准备结果…（共 %1 项）").arg(qLFilteredFiles.size()));

    if (m_rowFillTimer != nullptr)
        m_rowFillTimer->start();
    else
        continueAddRows();   // 定时器没建好时（理论上不会）退化为一次性填充
}

/** 分片把结果行插进模型：每片约 15ms，界面全程可操作。 */
void FormPageFilterdFiles::continueAddRows()
{
    QElapsedTimer budget;
    budget.start();

    while (m_rowSourceRow < m_rowSourcePaths.size() && budget.elapsed() < 15) {
        const QString &sFilePath = m_rowSourcePaths.at(m_rowSourceRow);
        // QFileInfo 只做字符串解析，不会 stat 磁盘
        const QFileInfo fileInfo(sFilePath);
        QList<QStandardItem *> items;
        items << new QStandardItem(fileInfo.fileName())
              << new QStandardItem(QDir::toNativeSeparators(sFilePath))
              << new QStandardItem(QString());
        m_modelFilteredFiles->appendRow(items);
        ++m_rowSourceRow;
    }

    if (m_rowSourceRow < m_rowSourcePaths.size())
        return;

    if (m_rowFillTimer != nullptr)
        m_rowFillTimer->stop();

    // 填完了：恢复"按内容自适应列宽"（只算一次），再补上计数文案
    m_filteredFilesTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    rebuildSummaryText();

    if (m_tagScanTimer != nullptr && !m_tagScanPaths.isEmpty())
        m_tagScanTimer->start();
}

/** 更新结果摘要文字：多行显示 + tooltip 放完整内容（防止被省略）。 */
void FormPageFilterdFiles::updateResultCountLabel(const QString &sText)
{
    if (ui->lblResultCount == nullptr)
        return;
    ui->lblResultCount->setText(sText);
    ui->lblResultCount->setToolTip(sText);
}

void FormPageFilterdFiles::setSearchCondition(const QStringList &qLConditionTexts,
                                              const QStringList &qLSearchDirs)
{
    m_searchConditionTexts = qLConditionTexts;
    m_searchDirs = qLSearchDirs;
    rebuildSummaryText();
}

/** 按分隔符把条目排成多行：只在条目之间换行，不把 "E:\\路径" 从中间劈开。 */
QString FormPageFilterdFiles::wrapItemsBySeparator(const QString &sPrefix,
                                                   const QStringList &qLItems,
                                                   const QString &sSeparator) const
{
    if (qLItems.isEmpty() || ui->lblResultCount == nullptr)
        return QString();

    const QFontMetrics fm(ui->lblResultCount->font());
    // 留一点右侧余量，避免正好顶到滚动条/按钮
    const int nAvailable = qMax(160, ui->lblResultCount->width() - 12);
    // 换行后的缩进：和第一行"目录："同宽，看起来像续行
    const int nSpaceWidth = qMax(1, fm.horizontalAdvance(QLatin1Char(' ')));
    const QString sIndent(nSpaceWidth > 0 ? fm.horizontalAdvance(sPrefix) / nSpaceWidth : 0,
                          QLatin1Char(' '));

    QStringList qLLines;
    QString sCurrent = sPrefix;
    bool bLineHasItem = false;
    for (const QString &sItem : qLItems) {
        const QString sPiece = (bLineHasItem ? sSeparator : QString()) + sItem;
        if (bLineHasItem && fm.horizontalAdvance(sCurrent + sPiece) > nAvailable) {
            qLLines << sCurrent;          // 在条目之间换行
            sCurrent = sIndent + sItem;
            bLineHasItem = true;
        } else {
            sCurrent += sPiece;
            bLineHasItem = true;
        }
    }
    if (bLineHasItem)
        qLLines << sCurrent;
    return qLLines.join(QLatin1Char('\n'));
}

/** 按当前宽度重排摘要：计数/来源、标签、目录（目录只在"；"处换行）。 */
void FormPageFilterdFiles::rebuildSummaryText()
{
    if (ui->lblResultCount == nullptr)
        return;

    QStringList qLLines;
    if (!m_summaryHeadText.isEmpty())
        qLLines << m_summaryHeadText;

    if (!m_searchConditionTexts.isEmpty()) {
        const QString sLabelLine = wrapItemsBySeparator(tr("标签："), m_searchConditionTexts,
                                                        QStringLiteral("、"));
        if (!sLabelLine.isEmpty())
            qLLines << sLabelLine;
    }
    if (!m_searchDirs.isEmpty()) {
        const QString sDirLine = wrapItemsBySeparator(tr("目录："), m_searchDirs,
                                                      QStringLiteral("；"));
        if (!sDirLine.isEmpty())
            qLLines << sDirLine;
    }

    if (m_staleCount > 0) {
        qLLines << tr("（其中 %1 项已不匹配当前条件，可点【重新扫描】刷新）")
                       .arg(m_staleCount);
    }

    if (qLLines.isEmpty())
        return;
    updateResultCountLabel(qLLines.join(QLatin1Char('\n')));
}

/** 窗口宽度变了要重排，否则换行位置会停留在旧宽度上。 */
void FormPageFilterdFiles::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    rebuildSummaryText();
}

void FormPageFilterdFiles::setSearchSource(const QString &sSource)
{
    m_searchSource = sSource;
}

void FormPageFilterdFiles::hideEvent(QHideEvent *event)
{
    // 返回文件页后不要再枚举隐藏页的 ADS
    if (m_tagScanTimer != nullptr)
        m_tagScanTimer->stop();
    QWidget::hideEvent(event);
}

void FormPageFilterdFiles::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 标签还没填完就切回来时继续填
    if (m_tagScanTimer != nullptr && m_tagScanRow < m_tagScanPaths.size()
            && !m_tagScanTimer->isActive()) {
        m_tagScanTimer->start();
    }
}

void FormPageFilterdFiles::continueFillTags()
{
    if (m_tagScanRow >= m_tagScanPaths.size()) {
        if (m_tagScanTimer != nullptr)
            m_tagScanTimer->stop();
        return;
    }

    QElapsedTimer budget;
    budget.start();
    while (m_tagScanRow < m_tagScanPaths.size() && budget.elapsed() < 15) {
        QStandardItem *tagItem = m_modelFilteredFiles->item(m_tagScanRow, 2);
        if (tagItem != nullptr) {
            const QStringList qLTags = ADSOperation::listADSTagsInOrder(m_tagScanPaths.at(m_tagScanRow));
            tagItem->setText(qLTags.join(","));
        }
        ++m_tagScanRow;
    }

    if (m_tagScanRow >= m_tagScanPaths.size() && m_tagScanTimer != nullptr)
        m_tagScanTimer->stop();
}

QString FormPageFilterdFiles::currentFilePath() const
{
    const QModelIndex curIdx = m_filteredFilesTree->currentIndex();
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
    const QModelIndex clickedIndex = m_filteredFilesTree->indexAt(pos);
    m_filteredFilesTree->setCurrentIndex(clickedIndex);
    if (!clickedIndex.isValid())
        m_filteredFilesTree->clearSelection();

    const bool hasSelection = clickedIndex.isValid();

    QMenu menu(ui->tvFilteredFiles);
    menu.addAction(m_openFile);
    menu.addAction(m_revealInExplorer);
    menu.addAction(m_gotoFilePath);
    menu.addAction(m_copyFilePath);
    menu.addSeparator();
    menu.addAction(m_editTags);
    menu.addAction(m_clearTags);
    menu.addAction(m_renameFile);
    m_openFile->setEnabled(hasSelection);
    m_revealInExplorer->setEnabled(hasSelection);
    m_gotoFilePath->setEnabled(hasSelection);
    m_copyFilePath->setEnabled(hasSelection);
    m_editTags->setEnabled(hasSelection);
    m_clearTags->setEnabled(hasSelection);
    m_renameFile->setEnabled(hasSelection);
    menu.exec(m_filteredFilesTree->viewport()->mapToGlobal(pos));
}


void FormPageFilterdFiles::setSearchMatcher(const QList<QSet<QString>> &qLLabelGroups, labelLogic logic)
{
    m_labelGroups = qLLabelGroups;
    m_searchLogic = logic;
    m_hasMatcher = !qLLabelGroups.isEmpty();
}

/**
 * 标签变化后就地刷新结果行：
 *  - 重新读一次 ADS，更新"标签"列（原来只有搜索结果那一刻的旧值）；
 *  - 用当前条件重新判断是否还匹配，不匹配的标灰并加后缀，
 *    而不是让用户对着一行"其实已经不匹配"的结果继续操作。
 */
void FormPageFilterdFiles::onFileTagsChanged(const QStringList &qLFilePaths)
{
    if (m_modelFilteredFiles == nullptr || qLFilePaths.isEmpty())
        return;

    QStringList qLTargets;
    for (const QString &sPath : qLFilePaths) {
        const QString sClean = QDir::cleanPath(sPath);
        if (sClean.isEmpty() || qLTargets.contains(sClean))
            continue;
        qLTargets << sClean;
        refreshRowForPath(sClean);
    }
    updateStaleCountText();
}

void FormPageFilterdFiles::refreshRowForPath(const QString &sFilePath)
{
    const QString sClean = QDir::cleanPath(sFilePath);
    const int nRows = m_modelFilteredFiles->rowCount();
    for (int nRow = 0; nRow < nRows; ++nRow) {
        QStandardItem *pathItem = m_modelFilteredFiles->item(nRow, 1);
        QStandardItem *tagItem = m_modelFilteredFiles->item(nRow, 2);
        if (pathItem == nullptr || tagItem == nullptr)
            continue;
        const QString sRowPath = QDir::cleanPath(QDir::fromNativeSeparators(pathItem->text()));
        if (sRowPath.compare(sClean, Qt::CaseInsensitive) != 0)
            continue;

        const QStringList qLTags = ADSOperation::listADSTagsInOrder(sClean);
        bool bMatched = true;
        if (m_hasMatcher) {
            bMatched = SearchMatch::matchesFileLabels(Utility::toSet(qLTags), m_labelGroups,
                                                      static_cast<int>(m_searchLogic));
        }

        QString sText = qLTags.join(QStringLiteral(", "));
        if (!bMatched) {
            if (!sText.isEmpty())
                sText += QStringLiteral(" ");
            sText += tr("（已不匹配当前条件）");
        }
        tagItem->setText(sText);
        tagItem->setData(!bMatched, Qt::UserRole + 1);
        tagItem->setForeground(bMatched ? QBrush() : QBrush(Qt::gray));
    }
}

void FormPageFilterdFiles::updateStaleCountText()
{
    if (ui == nullptr || ui->lblResultCount == nullptr)
        return;
    int nStale = 0;
    const int nRows = m_modelFilteredFiles->rowCount();
    for (int nRow = 0; nRow < nRows; ++nRow) {
        QStandardItem *tagItem = m_modelFilteredFiles->item(nRow, 2);
        if (tagItem != nullptr && tagItem->data(Qt::UserRole + 1).toBool())
            ++nStale;
    }
    m_staleCount = nStale;
    rebuildSummaryText();
}

void FormPageFilterdFiles::setSearchInterrupted(bool bInterrupted)
{
    m_bInterrupted = bInterrupted;
}

/** 当前选中的全部结果路径（去重、保持顺序）。 */
QStringList FormPageFilterdFiles::selectedFilePaths() const
{
    QStringList qLPaths;
    if (m_filteredFilesTree == nullptr || m_filteredFilesTree->selectionModel() == nullptr)
        return qLPaths;
    const QModelIndexList qLRows = m_filteredFilesTree->selectionModel()->selectedRows(1);
    for (const QModelIndex &index : qLRows) {
        const QString sPath = index.data().toString();
        if (!sPath.isEmpty() && !qLPaths.contains(sPath))
            qLPaths << sPath;
    }
    return qLPaths;
}

/** 结果页里按 F2 重命名：改名成功后把新名字回写到结果行，并通知外层同步缓存。 */
void FormPageFilterdFiles::renameCurrentFileInteractive()
{
    if (m_filteredFilesTree == nullptr)
        return;
    const QModelIndex current = m_filteredFilesTree->currentIndex();
    if (!current.isValid())
        return;

    const QModelIndex pathIndex =
        m_modelFilteredFiles->index(current.row(), 1);
    QStandardItem *pathItem = m_modelFilteredFiles->itemFromIndex(pathIndex);
    QStandardItem *nameItem = m_modelFilteredFiles->item(current.row(), 0);
    if (pathItem == nullptr || nameItem == nullptr)
        return;

    const QString sOldPath = pathItem->text();
    const QFileInfo fileInfo(sOldPath);
    if (!fileInfo.exists())
        return;

    bool bOk = false;
    const QString sNewName = QInputDialog::getText(this, tr("重命名"),
                                                   tr("新名称（含扩展名）："),
                                                   QLineEdit::Normal, fileInfo.fileName(), &bOk)
                                 .trimmed();
    if (!bOk || sNewName.isEmpty() || sNewName == fileInfo.fileName())
        return;

    const QString sNewPath = fileInfo.absolutePath() + QLatin1Char('/') + sNewName;
    if (QFileInfo::exists(sNewPath)) {
        QMessageBox::warning(this, tr("重命名失败"),
                             tr("【%1】已经存在，请换一个名字。").arg(sNewName));
        return;
    }
    if (!QFile::rename(sOldPath, sNewPath)) {
        QMessageBox::warning(this, tr("重命名失败"),
                             tr("无法把【%1】改成【%2】。\n可能是名称不合法或文件正被占用。")
                                 .arg(fileInfo.fileName(), sNewName));
        return;
    }

    nameItem->setText(sNewName);
    pathItem->setText(sNewPath);
    emit sendRenameRequested(sOldPath, sNewName);
}
