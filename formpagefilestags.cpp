// FormPageFiles 的标签实现：编辑/删除标签、目录标签扫描、重命名。

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

void FormPageFiles::setAction(QAction *actionDeleteLabels)
{
    m_actionDeleteLabels = actionDeleteLabels;
    connect(m_actionDeleteLabels, &QAction::triggered, this, &FormPageFiles::onDeleteLabels);
}

void FormPageFiles::onDeleteLabels()
{
    QStringList qLSelFilePaths = getSelFilePath();
    if (qLSelFilePaths.isEmpty()) {
        // 没选文件时原来直接 return，点了按钮像没反应，这里给一句提示
        emit sendHintMessage(tr("请先在文件列表里选中要清空标签的文件。"));
        return;
    }

    // 清空标签是破坏性操作：先让用户确认，避免误点工具栏就清空全部标签。
    // 删除前的标签会被记录，可再用 Ctrl+Z 撤销（撤销入口见【撤销上一次标签操作】）。
    QMessageBox box(this);
    box.setWindowTitle(tr("删除标签"));
    box.setIcon(QMessageBox::Warning);
    // 确认框里列出具体文件（最多 5 个），避免"选中的文件已经不在当前视图里"时
    // 用户不知道到底要删哪些文件的标签。
    QString sFileList = qLSelFilePaths.mid(0, 5).join(QStringLiteral("\n"));
    if (qLSelFilePaths.size() > 5)
        sFileList += tr("\n…另有 %1 个文件").arg(qLSelFilePaths.size() - 5);
    box.setText(tr("将删除以下 %1 个文件的全部标签（删除后可用 Ctrl+Z 撤销）：\n\n%2\n\n是否继续？")
                    .arg(qLSelFilePaths.size()).arg(sFileList));
    QPushButton *btnDelete = box.addButton(tr("删除"), QMessageBox::DestructiveRole);
    QPushButton *btnCancel = box.addButton(tr("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(btnCancel);
    box.exec();
    if (box.clickedButton() != btnDelete)
        return;

    // 删除前广播一次：上层会把"删除之前的标签"记下来，供 Ctrl+Z 撤销
    emit sendBeforeTagsChanged(qLSelFilePaths);

    bool bAllOk = true;
    QStringList qLFailedFiles;          // 把失败的文件名带回给用户
    for (const QString &sSelFilePath : qLSelFilePaths) {
        if (!ADSOperation::deleteADSFiles(sSelFilePath)) {
            bAllOk = false;
            qLFailedFiles << QFileInfo(sSelFilePath).fileName();
        }
    }
    emit sendTagsChanged(qLSelFilePaths);
    refreshAfterTagChange(qLSelFilePaths);

    if (!bAllOk) {
        const QString sList = qLFailedFiles.mid(0, 5).join(QStringLiteral("\n"));
        const QString sMore = qLFailedFiles.size() > 5
                                  ? tr("\n…另有 %1 个文件").arg(qLFailedFiles.size() - 5)
                                  : QString();
        QMessageBox::warning(this, tr("删除标签失败"),
                             tr("以下 %1 个文件的标签未能删除（可能是文件被占用、只读或权限不足）：\n\n%2%3")
                                 .arg(qLFailedFiles.size()).arg(sList).arg(sMore));
    }
}

void FormPageFiles::updateDirLabelsPanel(const QString &sDir)
{
    emit sendDirLabels(sDir, QSet<QString>());
    // 这里必须强制重扫：发送空集已经把面板清空了，如果再用"非强制"方式调用，
    // 对"本会话已扫过一遍的同一目录"会被 m_scanCompletedDir 守卫直接跳过，
    // 结果面板清空后永远不再填充（实测：对同一路径再按一次回车即可复现）。
    QTimer::singleShot(150, this, [this, sDir]() { scanDirLabels(sDir, true); });
}

void FormPageFiles::scanDirLabels(const QString &sDir, bool bForce)
{
    if (sDir.isEmpty() || !FmPageUtil::isSameDirPath(sDir, m_curDir))
        return;
    // 已经完整扫过且不是强制刷新（例如 directoryLoaded 的重复通知）就不再扫
    if (!bForce && m_scanCompletedDir == sDir)
        return;

    const QModelIndex rootIndex = m_modelFiles->index(sDir);
    if (!rootIndex.isValid())
        return;

    const int nRows = m_modelFiles->rowCount(rootIndex);
    if (nRows <= 0)
        return;   // 目录还没加载出内容，等 directoryLoaded 或下一次刷新

    m_scanDir = sDir;
    m_scanRootIndex = rootIndex;
    m_scanRow = 0;
    m_scanTotal = qMin(nRows, 2000);
    m_setScanLabels.clear();
    m_scanCompletedDir.clear();

    if (m_scanTimer != nullptr)
        m_scanTimer->start();
}

void FormPageFiles::continueDirLabelScan()
{
    if (!FmPageUtil::isSameDirPath(m_scanDir, m_curDir)) {
        if (m_scanTimer != nullptr)
            m_scanTimer->stop();
        return;
    }

    // 每片最多约 15ms（实测约 0.37ms/文件，即每片约 40 个文件），保证界面仍可响应
    QElapsedTimer budget;
    budget.start();
    while (m_scanRow < m_scanTotal && budget.elapsed() < 15) {
        const QModelIndex idx = m_modelFiles->index(m_scanRow, 0, m_scanRootIndex);
        ++m_scanRow;
        if (!idx.isValid() || m_modelFiles->isDir(idx))
            continue;
        const QStringList qLTags = ADSOperation::listADSTagsInOrder(m_modelFiles->filePath(idx));
        for (const QString &sTag : qLTags)
            m_setScanLabels.insert(sTag);
        // 预热模型缓存：标签列绘制时直接命中，避免首次 paint 同步枚举 ADS
        m_modelFiles->cacheTagsForFile(m_modelFiles->filePath(idx), qLTags);
    }

    if (m_scanRow < m_scanTotal)
        return;

    if (m_scanTimer != nullptr)
        m_scanTimer->stop();

    if (!FmPageUtil::isSameDirPath(m_scanDir, m_curDir))
        return;

    emit sendDirLabels(m_scanDir, m_setScanLabels);
    m_scanCompletedDir = m_scanDir;

    // 目录文件数超过上限时明确提示，避免用户误以为面板内容完整
    if (m_modelFiles->rowCount(m_scanRootIndex) > m_scanTotal) {
        emit sendHintMessage(tr("目录文件过多，仅统计了前 %1 个文件的标签").arg(m_scanTotal));
    }
}

void FormPageFiles::onRecvLabels(const QString &sFilePath, const QString &sLabels)
{
    emit sendLabels(sFilePath, sLabels);
}

void FormPageFiles::onEditTag(const QModelIndex &index)
{
    QString sFilePath = m_modelFiles->filePath(index);
    // 只对文件弹编辑框，目录无需标签
    if (sFilePath.isEmpty() || m_modelFiles->isDir(index))
        return;

    // 按录入顺序显示当前标签（NTFS 枚举流是按流名排序的，不能直接用它）
    const QStringList adsNames = ADSOperation::listADSTagsInOrder(sFilePath);

    // 改用"列表 + 添加/移除"的编辑器：
    // 以前是把全部标签当成一行文本编辑，用户全选后输入新名字就等于静默替换掉所有旧标签；
    // 现在每个标签都是列表里的一项，增删都要显式操作，且非法名字在添加时就被拒绝。
    TagEditDialog dlg(adsNames, QFileInfo(sFilePath).fileName(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QStringList qLLabels = dlg.tags();
    if (qLLabels == adsNames)   // 没有改动就不要重写
        return;

    emit sendBeforeTagsChanged(QStringList() << sFilePath);   // 供"撤销上一次标签操作"
    const bool bOk = ADSOperation::replaceTags(sFilePath, qLLabels);
    emit sendTagsChanged(QStringList() << sFilePath);
    refreshAfterTagChange(QStringList() << sFilePath);

    if (!bOk) {
        QMessageBox::warning(this, tr("保存标签失败"),
                             tr("标签未能完整写入：%1\n"
                                "已尝试恢复操作前的标签，请检查文件是否被占用、只读或磁盘空间不足。")
                                 .arg(sFilePath));
        return;
    }

    emit sendLabels(sFilePath, qLLabels.join(","));
}

void FormPageFiles::onRemoveTagFromRow(const QModelIndex &index, const QString &sTag)
{
    const QString sFilePath = m_modelFiles->filePath(index);
    if (sFilePath.isEmpty() || m_modelFiles->isDir(index) || sTag.isEmpty())
        return;

    const QStringList qLTags = ADSOperation::listADSTagsInOrder(sFilePath);
    if (!qLTags.contains(sTag))
        return;

    emit sendBeforeTagsChanged(QStringList() << sFilePath);
    const bool bOk = ADSOperation::removeTag(sFilePath, sTag);
    emit sendTagsChanged(QStringList() << sFilePath);
    refreshAfterTagChange(QStringList() << sFilePath);

    if (!bOk) {
        QMessageBox::warning(this, tr("删除标签失败"),
                             tr("未能从文件上删除标签「%1」：%2\n"
                                "请检查文件是否被占用、只读。")
                                 .arg(sTag, sFilePath));
        return;
    }

    // 让右侧面板和结果页同步：发最新标签串（空串表示已无标签）
    const QStringList qLRemain = ADSOperation::listADSTagsInOrder(sFilePath);
    emit sendLabels(sFilePath, qLRemain.join(","));
}

void FormPageFiles::renameFile(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    const QModelIndex nameIndex = index.sibling(index.row(), 0);
    const QString sOldName = m_modelFiles->fileName(nameIndex);
    if (sOldName.isEmpty())
        return;

    QInputDialog dlg(this);
    dlg.setWindowTitle(tr("重命名"));
    dlg.setLabelText(tr("新名称（含扩展名）："));
    dlg.setInputMode(QInputDialog::TextInput);
    dlg.setTextValue(sOldName);
    dlg.setOkButtonText(tr("重命名"));
    dlg.setCancelButtonText(tr("取消"));
    dlg.setMinimumWidth(520);
    dlg.resize(520, dlg.sizeHint().height());
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString sNewName = dlg.textValue().trimmed();
    if (sNewName.isEmpty() || sNewName == sOldName)
        return;

    if (!m_modelFiles->setData(nameIndex, sNewName, Qt::EditRole)) {
        QMessageBox::warning(this, tr("重命名失败"),
                             tr("无法把【%1】改成【%2】。\n"
                                "可能是名称不合法、已经存在同名文件，或文件正被占用。")
                                 .arg(sOldName, sNewName));
    }
}
