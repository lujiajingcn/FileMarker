// FormFileBrowser 的标签备份实现：导出成 JSON、从 JSON 写回。

#include "formfilebrowser.h"
#include <QMenu>
#include <QToolButton>
#include <QAction>
#include <QThread>
#include <QTimer>
#include <QLabel>
#include <QStatusBar>
#include <QStackedWidget>
#include "ui_formfilebrowser.h"

#include <QActionGroup>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>

#include "adsoperation.h"
#include "configoperation.h"
#include "searchmatch.h"
#include "tagexport.h"
#include "utility.h"

/**
 * 导出标签：扫描选中目录（没有选中就用当前目录）下所有文件的标签，写成 JSON 备份。
 *
 * 存在的意义：标签是 NTFS 数据流，文件被拷到 U 盘（FAT32）、压成 zip、传网盘或聊天工具时
 * 标签会整体丢失；这份清单可以在文件回来后把标签原样写回。
 */
void FormFileBrowser::exportTagsToFile()
{
    QStringList qLDirs = getSelDirs();
    if (qLDirs.isEmpty()) {
        const QString sCurDir = ui->pageFiles->getCurDir();
        if (sCurDir.isEmpty() || sCurDir == tr("我的电脑")) {
            QMessageBox::information(this, tr("导出标签"),
                                     tr("请先选中要导出的文件夹（或在文件列表里进入该目录）。"));
            return;
        }
        qLDirs << sCurDir;
    }

    const QString sDefaultPath = QDir(qLDirs.first()).filePath(
        tr("标签备份-%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));
    const QString sTarget = QFileDialog::getSaveFileName(this, tr("导出标签"), sDefaultPath,
                                                         tr("标签备份 (*.json)"));
    if (sTarget.isEmpty())
        return;

    // 应用级模态 + 重入保护：导出期间 processEvents() 会继续处理别处的输入，
    // 只有把整个程序都置为模态，用户才点不到"导入/遍历/AI 打标"这些会同时写盘的操作。
    if (m_longOperationRunning) {
        QMessageBox::information(this, tr("导出标签"), tr("上一个导入/导出还没结束，请稍候。"));
        return;
    }
    m_longOperationRunning = true;

    QProgressDialog progress(tr("正在读取文件标签…"), tr("取消"), 0, 0, this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);

    int nScanned = 0;
    QString sError;
    const QList<TagExport::FileTags> qLRecords = TagExport::collectFromDirs(
        qLDirs,
        [&progress, &nScanned](const QString &sFilePath) {
            if ((++nScanned % 100) == 0) {
                progress.setLabelText(tr("正在读取文件标签…\n%1").arg(sFilePath));
                QCoreApplication::processEvents();
            }
            return !progress.wasCanceled();
        },
        &sError);
    progress.close();
    m_longOperationRunning = false;

    if (!sError.isEmpty())
        QMessageBox::warning(this, tr("导出标签"), sError);

    if (qLRecords.isEmpty()) {
        QMessageBox::information(this, tr("导出标签"), tr("选中的目录里没有带标签的文件。"));
        return;
    }

    if (!TagExport::writeJson(sTarget, qLRecords, qLDirs.first(), &sError)) {
        QMessageBox::critical(this, tr("导出标签失败"), sError);
        return;
    }

    QMessageBox::information(
        this, tr("导出标签"),
        tr("已导出 %1 个文件的标签。\n\n%2\n\n"
           "提示：标签保存在 NTFS 数据流里，拷到 U 盘(FAT32)、压缩包或网盘会丢失；"
           "文件回来后用【文件 → 导入标签…】即可写回。")
            .arg(qLRecords.size()).arg(sTarget));
}

/**
 * 导入标签：读 JSON 备份，把标签写回文件。
 * 目录整体被搬走时，可以让用户指定新的根目录，用"新根 + 相对路径"重新定位文件。
 */
void FormFileBrowser::importTagsFromFile()
{
    const QString sSource = QFileDialog::getOpenFileName(this, tr("导入标签"), QString(),
                                                         tr("标签备份 (*.json)"));
    if (sSource.isEmpty())
        return;

    QList<TagExport::FileTags> qLRecords;
    QString sRoot;
    QString sError;
    if (!TagExport::readJson(sSource, &qLRecords, &sRoot, &sError)) {
        QMessageBox::critical(this, tr("导入标签失败"), sError);
        return;
    }
    if (qLRecords.isEmpty()) {
        QMessageBox::information(this, tr("导入标签"), tr("这个备份里没有可导入的标签。"));
        return;
    }

    // 统计按原路径还能找到多少文件；找不到的多，就提示用户指定新的根目录
    int nExist = 0;
    int nTags = 0;
    for (const TagExport::FileTags &record : qLRecords) {
        if (QFileInfo::exists(record.sPath))
            ++nExist;
        nTags += record.qLTags.size();
    }

    QString sFallbackRoot;
    if (nExist < qLRecords.size()) {
        const auto answer = QMessageBox::question(
            this, tr("导入标签"),
            tr("备份里共 %1 个文件，其中 %2 个按原路径找不到。\n\n"
               "如果整个目录被移动/改名过，可以现在指定新的根目录；"
               "选“否”则只导入能找到的文件。")
                .arg(qLRecords.size()).arg(qLRecords.size() - nExist),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
        if (answer == QMessageBox::Cancel)
            return;
        if (answer == QMessageBox::Yes) {
            sFallbackRoot = QFileDialog::getExistingDirectory(this, tr("选择新的根目录"), sRoot);
            if (sFallbackRoot.isEmpty())
                return;
        }
    }

    QMessageBox askBox(this);
    askBox.setWindowTitle(tr("导入标签"));
    askBox.setIcon(QMessageBox::Question);
    askBox.setText(tr("备份里共有 %1 个文件、%2 个标签。\n\n"
                      "写回时如何处理文件上已有的标签？（批量写盘，无法整体撤销）")
                       .arg(qLRecords.size()).arg(nTags));
    QPushButton *overwrite = askBox.addButton(tr("覆盖原有标签"), QMessageBox::AcceptRole);
    QPushButton *append = askBox.addButton(tr("追加到原有标签"), QMessageBox::AcceptRole);
    askBox.addButton(tr("取消"), QMessageBox::RejectRole);
    askBox.setDefaultButton(append);
    askBox.exec();
    QAbstractButton *clicked = askBox.clickedButton();
    if (clicked != static_cast<QAbstractButton *>(overwrite)
        && clicked != static_cast<QAbstractButton *>(append))
        return;
    const bool bOverwrite = (clicked == static_cast<QAbstractButton *>(overwrite));

    if (m_longOperationRunning) {
        QMessageBox::information(this, tr("导入标签"), tr("上一个导入/导出还没结束，请稍候。"));
        return;
    }
    m_longOperationRunning = true;

    QProgressDialog progress(tr("正在写回标签…"), tr("取消"), 0, qLRecords.size(), this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);

    int nDone = 0;
    const TagExport::ApplyResult result = TagExport::applyRecords(
        qLRecords, bOverwrite, sFallbackRoot,
        [&progress, &nDone](const QString &sFilePath) {
            progress.setValue(++nDone);
            if ((nDone % 20) == 0) {
                progress.setLabelText(tr("正在写回标签…\n%1").arg(sFilePath));
                QCoreApplication::processEvents();
            }
            return !progress.wasCanceled();
        });
    progress.close();
    m_longOperationRunning = false;

    // 缓存与界面同步：少量文件精确刷新，大量文件由 refreshCachedFiles 内部退化为整根失效
    if (m_sqlOperation != nullptr) {
        QStringList qLChangedPaths;
        for (const TagExport::FileTags &record : qLRecords)
            qLChangedPaths << record.sPath;
        m_sqlOperation->refreshCachedFiles(qLChangedPaths);
    }
    emit sendRefresh();

    QString sReport = tr("已写回 %1 个文件。").arg(result.nApplied);
    if (result.nSkipped > 0)
        sReport += tr("\n\n跳过 %1 个（文件不存在）：\n%2")
                       .arg(result.nSkipped)
                       .arg(result.qLMissingPaths.mid(0, 5).join(QStringLiteral("\n")));
    if (result.nFailed > 0)
        sReport += tr("\n\n失败 %1 个（被占用/只读/权限不足）：\n%2")
                       .arg(result.nFailed)
                       .arg(result.qLFailedPaths.mid(0, 5).join(QStringLiteral("\n")));

    if (result.nFailed > 0 || result.nSkipped > 0)
        QMessageBox::warning(this, tr("导入标签"), sReport);
    else
        QMessageBox::information(this, tr("导入标签"), sReport);
}
