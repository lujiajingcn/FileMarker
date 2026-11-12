// FormFileBrowser 的标签操作实现：AI 打标、添加标签、撤销、标签回传。

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

void FormFileBrowser::onRecvSelLabels(const QStringList &qLSelLabels)
{
    m_selectedLabels = qLSelLabels;
    // 没有收到"分组"信息时（例如从【当前目录中的标签】选择），每个标签自成一组。
    m_labelGroups.clear();
    for (const QString &sLabel : qLSelLabels) {
        QSet<QString> setGroup;
        setGroup.insert(sLabel);
        m_labelGroups << setGroup;
    }

    // 两个面板都会走到这里：点一下标签就等于"选中"，要明确告诉用户选中的是什么、
    // 接下来能用来做什么，否则【当前目录中的标签】看起来像只读面板。
    if (qLSelLabels.isEmpty())
        emit sendStatusMessage(tr("已清空标签选择。"));
    else
        emit sendStatusMessage(tr("已选中标签：%1（可用于【添加到选中文件】或【查找文件】）")
                                   .arg(qLSelLabels.join("、")));
}

/** 接收【标签库】给出的分组（每组 = 选中标签 + 它的全部子标签）。 */
void FormFileBrowser::onRecvLabelGroups(const QList<QSet<QString>> &qLLabelGroups)
{
    m_labelGroups = qLLabelGroups;
}

void FormFileBrowser::onActionAddLabelByAITriggered()
{
    setActionState(ADDTAGBYAI_START);
    showProcessPage(RUNNINGTHREAD::AIADDTAG);

    // 选中的文件优先；没有选文件才按选中的目录（含子目录）处理。
    // 注意这里用 getSelDirs(false)：不能用会回退"当前目录"的那个版本，
    // 否则"我只选了一个文件"会变成"给整个文件夹打标签"。
    const QStringList qLSelFiles = getSelFilePath();
    const QStringList qLSelDirs = getSelDirs(false);
    if (qLSelFiles.isEmpty() && qLSelDirs.isEmpty()) {
        showFilesWidget();
        setActionState(ADDTAGBYAI_FINISH);
        QMessageBox::information(this, tr("AI 自动生成标签"),
                                 tr("请先在文件列表里选中要处理的文件或文件夹。"));
        return;
    }

    m_threadAddLabelByAI = new ThreadAddTagByAI(this);

    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendProcessInfo, ui->pageProgressInfo, &FormPageProgressInfo::onRecvProgress);
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendFinish, this, &FormFileBrowser::onRecvAddLabelByAIFinish);
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendTags, this, &FormFileBrowser::onRecvLabelsGeneratedByAI);
    // AI 返回的 summary 之前被丢弃，这里记入日志（程序无专门展示 UI）。
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendSummary, this, [](const QString &s){
        qInfo() << "AI 摘要:" << s;
    });
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendTagChanged, this,
            [this](const QString &sFilePath) {
                if (m_sqlOperation != nullptr)
                    m_sqlOperation->invalidateCacheForFile(sFilePath);   // 按文件反查遍历根
            });

    // 线程结束后自动释放对象并把指针置空，避免反复创建导致对象泄漏以及悬空指针误用。
    connect(m_threadAddLabelByAI, &QThread::finished, m_threadAddLabelByAI, &QObject::deleteLater);
    connect(m_threadAddLabelByAI, &QThread::finished, this, [this](){ m_threadAddLabelByAI = nullptr; });

    m_threadAddLabelByAI->setDirs(qLSelDirs);
    m_threadAddLabelByAI->setFiles(qLSelFiles);

    m_threadAddLabelByAI->start();
}

void FormFileBrowser::onRecvAddLabelByAIFinish(int nSucceeded, int nFailed, const QString &sFirstError)
{
    showFilesWidget();
    setActionState(ADDTAGBYAI_FINISH);
    ui->pageProgressInfo->clear();
    // AI 线程写 ADS 时 FileTimeGuard 保持 mtime 不变，模型不会自动感知标签变化；
    // 这里主动刷新并清空标签缓存，否则标签列会一直显示旧值。
    emit sendRefresh();

    // 进度页在结束时会被清空，所以失败必须在这里用对话框告诉用户，
    // 否则"脚本缺失/缺依赖/解释器不可用"这类失败对用户完全不可见。
    if (nFailed > 0 || !sFirstError.isEmpty()) {
        QString sMsg;
        if (nFailed > 0) {
            sMsg = tr("AI 自动生成标签未全部成功：成功 %1 个文件，失败 %2 个。")
                       .arg(nSucceeded).arg(nFailed);
        } else {
            sMsg = tr("AI 自动生成标签未能运行。");
        }
        if (!sFirstError.isEmpty())
            sMsg += tr("\n\n原因：\n%1").arg(sFirstError);
        sMsg += tr("\n\n详细信息见程序目录下的 FileMarker.log。");
        QMessageBox::warning(this, tr("AI 自动生成标签"), sMsg);
    } else if (nSucceeded == 0) {
        QMessageBox::information(this, tr("AI 自动生成标签"),
                                 tr("没有处理任何文件。\n\n请确认已选中文件（或目录），"
                                    "并且目录里包含可处理的文件。"));
    }
}

void FormFileBrowser::onRecvLabelsGeneratedByAI(const QString &sLabels)
{
    emit sendLabelsGeneratedByAI(sLabels);
}

void FormFileBrowser::onActionAddLabelsTriggered()
{
    QStringList qLFilePaths = getSelFilePath();
    if (qLFilePaths.count() == 0)
    {
        QMessageBox::information(this, "提示", "未选中文件或文件夹！");
        return;
    }

    if (m_selectedLabels.count() == 0)
    {
        QMessageBox::information(this, "提示", "请先在右侧【标签库（分类）】里选择要添加的标签。");
        return;
    }

    // 批量打标签是不可撤销的写盘操作，数量多时先确认一次
    if (qLFilePaths.size() > 20) {
        const QMessageBox::StandardButton btn =
            QMessageBox::question(this, tr("添加标签"),
                                  tr("将为 %1 个文件添加标签【%2】，是否继续？")
                                      .arg(qLFilePaths.size()).arg(m_selectedLabels.join("、")),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes)
            return;
    }

    onBeforeTagsChanged(qLFilePaths);   // 先记下改动前的标签，供撤销

    bool bAllOk = true;
    QStringList qLFailedFiles;          // 把失败的文件名带回给用户
    for (const QString &sFilePath : qLFilePaths) {
        // 追加而不是覆盖：保留文件已有标签的顺序，新标签按选中顺序追加到末尾
        if (!ADSOperation::appendTags(sFilePath, m_selectedLabels)) {
            bAllOk = false;
            qLFailedFiles << QFileInfo(sFilePath).fileName();
        }
    }

    // 标签变化后让搜索缓存失效，避免搜索仍按旧标签命中/漏掉新标签
    // 按文件反查所有包含它的"遍历根"，不能只按父目录删缓存。
    if (m_sqlOperation != nullptr) {
        for (const QString &sFilePath : qLFilePaths)
            m_sqlOperation->invalidateCacheForFile(sFilePath);
    }

    emit sendRefresh();

    if (!bAllOk) {
        // 以前只给一句通用提示，批量操作时无法知道是哪个文件失败；
        // 现在列出前 5 个失败文件名（完整列表见日志）。
        const QString sList = qLFailedFiles.mid(0, 5).join(QStringLiteral("\n"));
        const QString sMore = qLFailedFiles.size() > 5
                                  ? tr("\n…另有 %1 个文件").arg(qLFailedFiles.size() - 5)
                                  : QString();
        QMessageBox::warning(this, tr("保存标签失败"),
                             tr("以下 %1 个文件的标签未能写入（可能是文件被占用、只读或磁盘空间不足）：\n\n%2%3\n\n"
                                "已有标签不会被删除，请检查后重试。")
                                 .arg(qLFailedFiles.size()).arg(sList).arg(sMore));
    }
}

/** 记住"这次改动之前的标签"，供【撤销上一次标签操作】使用（只保留最近一次）。 */
void FormFileBrowser::onBeforeTagsChanged(const QStringList &qLFilePaths)
{
    if (qLFilePaths.isEmpty())
        return;

    m_mapUndoOldTags.clear();
    for (const QString &sFilePath : qLFilePaths) {
        if (sFilePath.isEmpty() || !QFileInfo(sFilePath).isFile())
            continue;
        m_mapUndoOldTags.insert(sFilePath, ADSOperation::listADSTagsInOrder(sFilePath));
    }
    m_undoDesc = tr("撤销：恢复 %1 个文件的标签").arg(m_mapUndoOldTags.size());
    emit sendUndoStateChanged(!m_mapUndoOldTags.isEmpty());
}

/** 把最近一次标签操作改回原样。 */
void FormFileBrowser::undoLastTagOperation()
{
    if (m_mapUndoOldTags.isEmpty()) {
        QMessageBox::information(this, tr("撤销"), tr("没有可以撤销的标签操作。"));
        return;
    }

    int nFailed = 0;
    for (auto cItr = m_mapUndoOldTags.cbegin(); cItr != m_mapUndoOldTags.cend(); ++cItr) {
        if (!ADSOperation::replaceTags(cItr.key(), cItr.value())) {
            ++nFailed;
            continue;
        }
        if (m_sqlOperation != nullptr)
            m_sqlOperation->invalidateCacheForFile(cItr.key());
    }

    const int nTotal = m_mapUndoOldTags.size();
    m_mapUndoOldTags.clear();
    emit sendUndoStateChanged(false);
    emit sendRefresh();

    if (nFailed > 0)
        QMessageBox::warning(this, tr("撤销"),
                             tr("已恢复 %1 个文件，%2 个失败（可能被占用或只读）。")
                                 .arg(nTotal - nFailed).arg(nFailed));
    else
        emit sendStatusMessage(tr("已撤销上一次标签操作（%1 个文件）。").arg(nTotal));
}

/** 标签库那边把文件拖到了某个标签上：交给文件页统一写盘（撤销点/缓存/刷新都在那里）。 */
void FormFileBrowser::onFilesDroppedOnLabel(const QStringList &qLFiles, const QStringList &qLLabels)
{
    ui->pageFiles->onFilesDroppedOnLabel(qLFiles, qLLabels);
}

void FormFileBrowser::onRecvLabels(const QString &sFilePath, const QString &sLabels)
{
    // 内联编辑/弹框编辑标签时，只作用于被编辑的那一个文件，
    // 不再误覆盖当前选中的其它文件（批量打标签请用工具栏“添加标签”按钮）。
    if (sFilePath.isEmpty())
    {
        QMessageBox::information(this, "提示", "未选中文件！");
        return;
    }

    // 拆分标签：支持中英文分隔符；历史标签名可能合法包含 "，、；"，
    // 用 splitLabelsPreservingExisting 保证已有标签不会被拆开。
    const QStringList qLExisting = ADSOperation::listADSTagsInOrder(sFilePath);
    QStringList qLLabels;
    QStringList qLChanged;
    for (const QString &sLabel : Utility::splitLabelsPreservingExisting(sLabels, qLExisting)) {
        // 旧标签原样保留，不重新净化（否则 "a，b" 会变成 "a_b"）
        if (qLExisting.contains(sLabel)) {
            if (!qLLabels.contains(sLabel))
                qLLabels << sLabel;
            continue;
        }
        const QString sSanitized = ADSOperation::sanitizeTagName(sLabel);
        if (!sSanitized.isEmpty() && !qLLabels.contains(sSanitized)) {
            qLLabels << sSanitized;
            if (sSanitized != sLabel.trimmed())
                qLChanged << sLabel + QStringLiteral("→") + sSanitized;
        } else if (sSanitized.isEmpty() && !sLabel.trimmed().isEmpty()) {
            qLChanged << sLabel + tr("（已忽略）");
        }
    }
    // 输入非空但净化后一个都不剩 → 中止并提示，不能当成"清空全部标签"。
    if (qLLabels.isEmpty() && !sLabels.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("标签名无效"),
                             tr("输入的标签名都不合法（不能用 : / \\ * ? \" < > | 和分隔符），"
                                "未做任何修改。\n\n原始输入：%1")
                                 .arg(sLabels.trimmed()));
        return;
    }

    if (!qLChanged.isEmpty())
        emit sendStatusMessage(tr("部分标签名已规范化：%1").arg(qLChanged.join("，")));

    // replaceTags 会按 qLLabels 的先后顺序写入，并把顺序号存进流内容，
    // 保证下次显示时仍是用户录入的顺序（NTFS 枚举流本身是按流名排序的）。
    onBeforeTagsChanged(QStringList() << sFilePath);   // 先记下改之前的标签
    const bool bOk = ADSOperation::replaceTags(sFilePath, qLLabels);

    if (m_sqlOperation != nullptr)
        m_sqlOperation->invalidateCacheForFile(sFilePath);   // 按文件反查遍历根

    emit sendRefresh();

    if (!bOk) {
        QMessageBox::warning(this, tr("保存标签失败"),
                             tr("标签未能完整写入：%1\n"
                                "已尝试恢复操作前的标签。请检查文件是否被占用、只读或磁盘空间不足。")
                                 .arg(sFilePath));
        return;
    }

    // 只把规范化后的非空标签转发给【标签管理】面板
    const QString sNormalizedLabels = qLLabels.join(",");
    if (!sNormalizedLabels.isEmpty())
        emit sendLabels(sNormalizedLabels);
}

// 设置菜单和工具栏中动作的状态（可用/不可用）
void FormFileBrowser::setActionState(OPERATION op)
{
    switch (op) {
    case ADDTAGBYAI_START:
    case SEARCH_START:
    case TRAVERSER_START:
        // QAction 指针可能为 null（setActions 传入 null 时 connect 静默失败），需判空。
        if (m_aiTagAction) m_aiTagAction->setEnabled(false);
        if (m_searchAction) m_searchAction->setEnabled(false);
        if (m_traverseAction) m_traverseAction->setEnabled(false);
        break;
    case ADDTAGBYAI_FINISH:
    case SEARCH_FINISH:
    case TRAVERER_FINISH:
        if (m_aiTagAction) m_aiTagAction->setEnabled(true);
        if (m_searchAction) m_searchAction->setEnabled(true);
        if (m_traverseAction) m_traverseAction->setEnabled(true);
        break;
    }
}
