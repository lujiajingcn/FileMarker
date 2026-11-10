// FormPageFiles 的导航实现：前进/后退/上级、地址栏历史、双击打开。

#include "formpagefiles.h"
#include "ui_formpagefiles.h"
#include "formpagefilesutil.h"

#include <QKeySequence>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QToolButton>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QProcess>
#include <QMessageBox>
#include <QPushButton>
#include <QLineEdit>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QSettings>
#include <QElapsedTimer>

#include "adsoperation.h"
#include "tagchipdelegate.h"
#include "thumbnailiconprovider.h"
#include "editablefiledelegate.h"
#include "tageditdialog.h"
#include "utility.h"

void FormPageFiles::restoreLastDir()
{
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    const QString sLastDir = settings.value(QStringLiteral("view/lastDir")).toString();
    if (sLastDir.isEmpty() || FmPageUtil::isMyComputerName(sLastDir))
        return;
    if (QDir(sLastDir).exists())
        navigateToDir(sLastDir);
}

QString FormPageFiles::getCurDir()
{
    return m_sCurDir;
}

void FormPageFiles::curDirChanged(QString sCurDir)
{
    m_skBackward.push_back(m_sCurDir);
    m_skForward.clear();
    m_sCurDir = sCurDir;
    // 同步地址栏并把路径记入下拉历史
    rememberPath(sCurDir);
    // 导航栈变化时直接更新按钮状态，不再依赖 event() 拦截 Paint。
    updateToolButtons();

    // 进入目录后立即更新【当前目录标签】面板：先清空旧内容，再在文件模型加载完成后扫描。
    updateDirLabelsPanel(sCurDir);
}

// 把地址栏文本恢复成当前目录（用户输入非法路径、或导航失败时使用）
void FormPageFiles::refreshAddressBar()
{
    ui->lookInCombo->setCurrentIndex(-1);
    ui->lookInCombo->setEditText(m_sCurDir);
    if (ui->lookInCombo->lineEdit())
        ui->lookInCombo->lineEdit()->setCursorPosition(0);
}

void FormPageFiles::rememberPath(const QString &sPath)
{
    if (sPath.isEmpty())
        return;

    // 最近访问的 10 个目录：去重、最近的在最前面、写回 settings
    const QString sClean = QDir::cleanPath(sPath);
    m_qLRecentDirs.removeAll(sClean);
    m_qLRecentDirs.prepend(sClean);
    while (m_qLRecentDirs.size() > FmPageUtil::RECENT_DIR_LIMIT)
        m_qLRecentDirs.removeLast();
    saveRecentDirs();

    // 用历史列表重建下拉项；地址栏文本单独设置，避免把"当前项"高亮成列表里的一项
    ui->lookInCombo->blockSignals(true);
    ui->lookInCombo->clear();
    ui->lookInCombo->addItems(m_qLRecentDirs);
    ui->lookInCombo->setCurrentIndex(-1);
    ui->lookInCombo->setEditText(sClean);
    ui->lookInCombo->blockSignals(false);
}

void FormPageFiles::loadRecentDirs()
{
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif

    const QStringList qLStored = settings.value(QStringLiteral("view/recentDirs")).toStringList();
    m_qLRecentDirs.clear();
    for (const QString &sDir : qLStored) {
        if (m_qLRecentDirs.size() >= FmPageUtil::RECENT_DIR_LIMIT)
            break;
        const QString sClean = QDir::cleanPath(sDir);
        if (!sClean.isEmpty() && !m_qLRecentDirs.contains(sClean))
            m_qLRecentDirs << sClean;
    }

    ui->lookInCombo->blockSignals(true);
    ui->lookInCombo->clear();
    ui->lookInCombo->addItems(m_qLRecentDirs);
    ui->lookInCombo->setCurrentIndex(-1);
    ui->lookInCombo->blockSignals(false);
}

void FormPageFiles::saveRecentDirs() const
{
    QSettings settings(Utility::settingsFilePath(), QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    settings.setValue(QStringLiteral("view/recentDirs"), m_qLRecentDirs);
}

void FormPageFiles::navigateToDir(const QString &sPath)
{
    QString sTarget = sPath.trimmed();
    // 支持直接粘贴资源管理器的带引号路径
    if (sTarget.size() >= 2 && sTarget.startsWith(QLatin1Char('"')) && sTarget.endsWith(QLatin1Char('"')))
        sTarget = sTarget.mid(1, sTarget.size() - 2).trimmed();
    if (sTarget.isEmpty()) {
        refreshAddressBar();
        return;
    }
    sTarget.replace(QLatin1Char('/'), QLatin1Char('\\'));

    // “我的电脑”是虚拟根节点，不是真实目录，不能走 QDir::exists 校验。
    // 注意 QFileSystemModel::myComputer() 返回的是英文 "My Computer"，所以两种名字都接受。
    if (FmPageUtil::isMyComputerName(sTarget)) {
        bothSetRootIndex(m_modelFiles->index(QString()));
        curDirChanged(FmPageUtil::myComputerName());
        return;
    }

    // 输入的是文件而不是目录：进入其所在目录并选中该文件
    const QFileInfo targetInfo(sTarget);
    if (targetInfo.exists() && targetInfo.isFile()) {
        const QString sParentDir = targetInfo.absolutePath();
        const QModelIndex parentIndex = m_modelFiles->index(sParentDir);
        if (!parentIndex.isValid()) {
            QMessageBox::warning(this, tr("无法打开路径"),
                                 tr("无法定位到该目录：\n%1").arg(sParentDir));
            refreshAddressBar();
            return;
        }
        bothSetRootIndex(parentIndex);
        curDirChanged(QDir::cleanPath(sParentDir));
        const QModelIndex fileIndex = m_modelFiles->index(targetInfo.absoluteFilePath());
        if (fileIndex.isValid()) {
            m_tvFiles->setCurrentIndex(fileIndex);
            m_lstFiles->setCurrentIndex(fileIndex);
            m_tvFiles->scrollTo(fileIndex, QAbstractItemView::PositionAtCenter);
            m_lstFiles->scrollTo(fileIndex, QAbstractItemView::PositionAtCenter);
        }
        return;
    }

    const QDir dir(sTarget);
    if (!dir.exists()) {
        QMessageBox::warning(this, tr("无法打开路径"),
                             tr("路径不存在或无法访问：\n%1").arg(sPath));
        refreshAddressBar();
        return;
    }

    const QString sAbs = QDir::cleanPath(dir.absolutePath());
    const QModelIndex targetIndex = m_modelFiles->index(sAbs);
    if (!targetIndex.isValid()) {
        QMessageBox::warning(this, tr("无法打开路径"),
                             tr("无法定位到该目录：\n%1").arg(sAbs));
        refreshAddressBar();
        return;
    }

    bothSetRootIndex(targetIndex);
    curDirChanged(sAbs);
}

void FormPageFiles::onLookInComboReturnPressed()
{
    navigateToDir(ui->lookInCombo->currentText());
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

void FormPageFiles::onTvFilesDoubleClicked(const QModelIndex &index)
{
    onDoubleClicked(index);
}

void FormPageFiles::onBackButtonClicked()
{
    if (m_skBackward.isEmpty())
        return;
    m_skForward.push_back(m_sCurDir);
    QString sDir = m_skBackward.pop();
    if (sDir.isEmpty())
        sDir = FmPageUtil::myComputerName();
    bothSetRootIndex(m_modelFiles->index(sDir));
    m_sCurDir = sDir;
    refreshAddressBar();
    updateToolButtons();  // 导航栈变化后直接更新按钮
    updateDirLabelsPanel(sDir);
}

void FormPageFiles::onForwardButtonClicked()
{
    if (m_skForward.isEmpty())
        return;
    m_skBackward.push_back(m_sCurDir);
    QString sDir = m_skForward.pop();
    if (sDir.isEmpty())
        sDir = FmPageUtil::myComputerName();
    bothSetRootIndex(m_modelFiles->index(sDir));
    m_sCurDir = sDir;
    refreshAddressBar();
    updateToolButtons();  // 导航栈变化后直接更新按钮
    updateDirLabelsPanel(sDir);
}

void FormPageFiles::onToParentButtonClicked()
{
    QModelIndex rootIndex = m_tvFiles->rootIndex();
    QString sFilePath = m_modelFiles->filePath(rootIndex);
    QDir dir(sFilePath);
    QString sNewDirectory;
    if (sFilePath.isEmpty() || dir.isRoot())
    {
        // 已经到盘符根：再上一级是虚拟根“我的电脑”，用空路径表示
        sNewDirectory = FmPageUtil::myComputerName();
        bothSetRootIndex(m_modelFiles->index(QString()));
    }
    else
    {
        dir.cdUp();
        sNewDirectory = dir.absolutePath();
        bothSetRootIndex(m_modelFiles->index(sNewDirectory));
    }
    // 根节点没有“父路径”，不要把空字符串压进后退栈
    if (!sFilePath.isEmpty())
        m_skBackward.push_back(sFilePath);
    m_sCurDir = sNewDirectory;
    // 地址栏同步：之前这里漏了更新，导致点上移后地址栏还停留在旧路径。
    rememberPath(sNewDirectory);
    updateToolButtons();  // 导航栈变化后直接更新按钮
    updateDirLabelsPanel(sNewDirectory);
}

void FormPageFiles::onTvNavigationClicked(const QModelIndex &index)
{
    QString path = index.data(QFileSystemModel::FilePathRole).toString();
    // 详情视图和缩略图视图共用模型，根节点必须一起切，否则切到缩略图还是旧目录。
    bothSetRootIndex(m_modelFiles->index(path));
    ui->stackedWidget->setCurrentIndex(FILEPAGE::DETAIL);
    curDirChanged(path);
}

void FormPageFiles::recvGotoFile(QString sFilePath)
{
    // 用 QFileInfo 取父目录，正确处理盘符根（如 C:/file.txt 下 lastIndexOf('/') 会失效）。
    QFileInfo fi(sFilePath);
    const QString sFileDir = QDir::cleanPath(fi.absolutePath());
    const QModelIndex dirIndex = m_modelFiles->index(sFileDir);
    // 使用 bothSetRootIndex 同时更新详细视图和缩略图视图，
    // 否则缩略图模式下跳转后仍停留在旧目录。
    bothSetRootIndex(dirIndex);

    // "当前目录"的状态有 5 份副本（两个视图的 rootIndex、m_sCurDir、地址栏、
    // 后退/前进栈、【当前目录中的标签】面板）。原来这里只改了 rootIndex，于是：
    // 地址栏和标签面板还显示旧目录，点【刷新】又按 m_sCurDir 把列表弹回旧目录。
    // 现在统一走 curDirChanged()，只有"本来就在这个目录"时才只同步地址栏，避免重复压后退栈。
    if (!FmPageUtil::isSameDirPath(sFileDir, m_sCurDir)) {
        curDirChanged(sFileDir);
    } else {
        refreshAddressBar();
        updateToolButtons();
    }

    const QModelIndex fileIndex = m_modelFiles->index(fi.absoluteFilePath());
    if (fileIndex.isValid()) {
        m_tvFiles->setCurrentIndex(fileIndex);
        m_lstFiles->setCurrentIndex(fileIndex);
    }
}

void FormPageFiles::onDoubleClicked(const QModelIndex &index)
{
    // 第 5 列（标签列）的双击语义是"编辑标签"，不能同时打开文件/进入目录。
    // 改为弹出"增删式"标签对话框（不再就地编辑整行文本）。
    // 说明：0aead52 之前该列的 FilePathRole 会误返回标签字符串，openUrl 打开失败，
    // 看起来像“没有打开文件”；修正 FilePathRole 后必须在这里显式排除标签列。
    if (index.column() == 4) {
        onEditTag(index);
        return;
    }

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
    // 换目录时必须清掉选择集：两个视图共用同一个 QItemSelectionModel，
    // 而 setRootIndex() 不会清理它。不清的话，切到别的目录后工具栏【删除标签】/【添加标签】
    // 仍会作用到旧目录里"已经看不见"的文件（实测：不可见文件的标签被真实删除）。
    if (m_tvFiles->selectionModel() != nullptr)
        m_tvFiles->selectionModel()->clearSelection();
}

void FormPageFiles::recvDblClick(const QModelIndex &index)
{
    onDoubleClicked(index);
}
