// FormFileBrowser 的标签操作实现：AI 打标、添加标签、撤销、标签回传。

#include "formfilebrowser.h"
#include "threadaiprepare.h"
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

void FormFileBrowser::invalidateAiEnvironment()
{
    m_aiEnvReady = false;
    m_aiSettingsSnapshot = AiSettings::load();
}

void FormFileBrowser::onActionAddLabelByAITriggered()
{
    setActionState(ADDTAGBYAI_START);
    showProcessPage(RUNNINGTHREAD::AIADDTAG);

    // 选中的文件优先；没有选文件才按选中的目录（含子目录）处理。
    // 注意这里用 getSelDirs(false)：不能用会回退"当前目录"的那个版本，
    // 否则"我只选了一个文件"会变成"给整个文件夹打标签"。
    const QStringList qLSelFiles = FormPageFiles::filesOnly(getSelFilePath());
    const QStringList qLSelDirs = getSelDirs(false);
    if (qLSelFiles.isEmpty() && qLSelDirs.isEmpty()) {
        showFilesWidget();
        setActionState(ADDTAGBYAI_FINISH);
        QMessageBox::information(this, tr("AI 自动生成标签"),
                                 tr("请先在文件列表里选中要处理的文件或文件夹。"));
        return;
    }

    // 服务端配置变了（用户刚在设置里切换本地/云端）也要重新检查环境
    const AiSettings aiNow = AiSettings::load();
    if (aiNow != m_aiSettingsSnapshot) {
        m_aiSettingsSnapshot = aiNow;
        m_aiEnvReady = false;
    }

    // 第一次使用（或上次整体失败）先自动准备环境：缺 Python/pip 库/Ollama/模型就下载安装，
    // 装好后会自动继续打标；已就绪则直接开始。
    if (!m_aiEnvReady) {
        startAiPrepare(qLSelFiles, qLSelDirs);
        return;
    }
    startAiTagging(qLSelFiles, qLSelDirs);
}

/**
 * AI 环境准备：缺什么装什么（Python / pip 依赖 / Ollama / 模型）。
 * 进度和命令输出都显示在进度页，可随时【停止全部】。
 */
void FormFileBrowser::startAiPrepare(const QStringList &qLFiles, const QStringList &qLDirs)
{
    m_pendingAiFiles = qLFiles;
    m_pendingAiDirs = qLDirs;

    ui->pageProgressInfo->beginAIPrepare();
    emit sendStatusMessage(tr("正在检查 AI 运行环境（缺少的依赖会自动下载安装，可停止）…"));

    m_threadAiPrepare = new ThreadAIPrepare(this);

    // 只准备用得到的模型：只选纯文本文件时不必下载 6GB 的视觉模型
    bool bNeedText = !qLFiles.isEmpty() || !qLDirs.isEmpty();
    bool bNeedImage = !qLDirs.isEmpty();
    for (const QString &sFile : qLFiles) {
        const QString sSuffix = QFileInfo(sFile).suffix().toLower();
        if (sSuffix == QLatin1String("png") || sSuffix == QLatin1String("jpg")
            || sSuffix == QLatin1String("jpeg")) {
            bNeedImage = true;
        } else {
            bNeedText = true;
        }
    }
    if (!bNeedText && !bNeedImage)
        bNeedText = true;
    m_threadAiPrepare->setNeededModels(bNeedText, bNeedImage);

    connect(m_threadAiPrepare, &ThreadAIPrepare::sendStage, ui->pageProgressInfo,
            &FormPageProgressInfo::setAIPrepareStage);
    connect(m_threadAiPrepare, &ThreadAIPrepare::sendLog, ui->pageProgressInfo,
            &FormPageProgressInfo::appendAILog);
    connect(m_threadAiPrepare, &ThreadAIPrepare::sendFinished, this,
            &FormFileBrowser::onAiPrepareFinished);
    connect(m_threadAiPrepare, &QThread::finished, this, [this]() { m_threadAiPrepare = nullptr; });

    m_threadAiPrepare->start();
}

void FormFileBrowser::onAiPrepareFinished(bool bOk, const QString &sError)
{
    if (bOk) {
        m_aiEnvReady = true;
        emit sendStatusMessage(tr("AI 运行环境已就绪，开始打标…"));
        const QStringList qLFiles = m_pendingAiFiles;
        const QStringList qLDirs = m_pendingAiDirs;
        m_pendingAiFiles.clear();
        m_pendingAiDirs.clear();
        startAiTagging(qLFiles, qLDirs);
        return;
    }

    showFilesWidget();
    setActionState(ADDTAGBYAI_FINISH);
    ui->pageProgressInfo->clear();
    QMessageBox::warning(this, tr("AI 运行环境准备失败"),
                         sError + tr("\n\n也可以手动安装后重试：\n"
                                     "  pip install requests pypdf python-docx\n"
                                     "  ollama pull qwen2.5:7b"));
}

void FormFileBrowser::startAiTagging(const QStringList &qLFiles, const QStringList &qLDirs)
{
    ui->pageProgressInfo->beginAITask();

    const int nTargets = qLFiles.isEmpty() ? int(qLDirs.count()) : int(qLFiles.count());
    emit sendStatusMessage(qLFiles.isEmpty()
                               ? tr("AI 正在分析 %1 个目录…").arg(nTargets)
                               : tr("AI 正在分析 %1 个文件…").arg(nTargets));

    m_threadAddLabelByAI = new ThreadAddTagByAI(this);

    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendProcessInfo, ui->pageProgressInfo, &FormPageProgressInfo::onRecvProgress);
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendAIProgress, ui->pageProgressInfo, &FormPageProgressInfo::setAIProgress);
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendFinish, this, &FormFileBrowser::onRecvAddLabelByAIFinish);
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendTags, this, &FormFileBrowser::onRecvLabelsGeneratedByAI);
    // AI 返回的 summary 之前被丢弃，这里记入日志（程序无专门展示 UI）。
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendSummary, this, [](const QString &s){
        qInfo() << "AI 摘要:" << s;
    });
    connect(m_threadAddLabelByAI, &ThreadAddTagByAI::sendTagChanged, this,
            [this](const QString &sFilePath) {
                if (m_sqlOperation != nullptr)
                    m_sqlOperation->refreshCachedFile(sFilePath);   // 只刷新这个文件在缓存里的行
            });

    // 线程结束后自动释放对象并把指针置空，避免反复创建导致对象泄漏以及悬空指针误用。
    connect(m_threadAddLabelByAI, &QThread::finished, m_threadAddLabelByAI, &QObject::deleteLater);
    connect(m_threadAddLabelByAI, &QThread::finished, this, [this](){ m_threadAddLabelByAI = nullptr; });

    m_threadAddLabelByAI->setDirs(qLDirs);
    m_threadAddLabelByAI->setFiles(qLFiles);

    m_threadAddLabelByAI->start();
}

void FormFileBrowser::onRecvAddLabelByAIFinish(int nSucceeded, int nFailed, int nSkipped,
                                               const QString &sFirstError)
{
    showFilesWidget();
    setActionState(ADDTAGBYAI_FINISH);
    ui->pageProgressInfo->clear();
    // 一个都没成功且有错误：说明环境仍有问题，下次点 AI 时重新检查/安装
    if (nSucceeded == 0 && !sFirstError.isEmpty())
        m_aiEnvReady = false;
    // 状态栏给一句结果摘要（失败时的详细原因仍用对话框说明）
    if (nFailed > 0) {
        emit sendStatusMessage(tr("AI 打标结束：成功 %1 个，失败 %2 个，跳过 %3 个")
                                   .arg(nSucceeded).arg(nFailed).arg(nSkipped));
    } else if (!sFirstError.isEmpty()) {
        // 例如脚本缺失/依赖缺失：nFailed 还是 0，不能说"完成：成功 0 个"
        emit sendStatusMessage(tr("AI 打标未能运行：%1").arg(sFirstError.left(120)));
    } else {
        emit sendStatusMessage(nSkipped > 0
                                   ? tr("AI 打标完成：成功 %1 个文件，跳过 %2 个").arg(nSucceeded).arg(nSkipped)
                                   : tr("AI 打标完成：成功 %1 个文件").arg(nSucceeded));
    }
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
            if (nSkipped > 0)
                sMsg += tr("\n跳过 %1 个不支持的文件类型。").arg(nSkipped);
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
    // 文件夹不参与打标签（与文件列表、拖拽保持同一套规则）
    const QStringList qLSelected = getSelFilePath();
    const QStringList qLFilePaths = FormPageFiles::filesOnly(qLSelected);
    if (qLFilePaths.isEmpty())
    {
        if (!qLSelected.isEmpty()) {
            // 选中了东西但全是文件夹：不能说"未选中"，否则用户以为选择丢了
            QMessageBox::information(this, tr("添加标签"),
                                     tr("选中的是文件夹。文件夹不支持打标签，请选择文件后再试。"));
        } else {
            QMessageBox::information(this, tr("添加标签"), tr("未选中文件或文件夹！"));
        }
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
    QStringList qLFailedPaths;          // 失败的文件（用来区分"只读"和其它原因）
    for (const QString &sFilePath : qLFilePaths) {
        // 追加而不是覆盖：保留文件已有标签的顺序，新标签按选中顺序追加到末尾
        if (!ADSOperation::appendTags(sFilePath, m_selectedLabels)) {
            bAllOk = false;
            qLFailedPaths << sFilePath;
        }
    }

    // 标签变化后让搜索缓存失效，避免搜索仍按旧标签命中/漏掉新标签
    // 按文件反查所有包含它的"遍历根"，不能只按父目录删缓存。
    if (m_sqlOperation != nullptr) {
        for (const QString &sFilePath : qLFilePaths)
            m_sqlOperation->refreshCachedFile(sFilePath);
    }

    emit sendRefresh();

    if (!bAllOk) {
        // 失败原因分开说：只读文件不是"被占用"，要让用户知道去取消只读属性。
        // （程序不会去改用户文件的属性。）
        QStringList qLReadOnly, qLOther;
        for (const QString &sPath : qLFailedPaths) {
            (ADSOperation::isFileReadOnly(sPath) ? qLReadOnly : qLOther)
                << QFileInfo(sPath).fileName();
        }
        QStringList qLMsg;
        if (!qLReadOnly.isEmpty()) {
            qLMsg << tr("以下 %1 个文件是只读的，无法写入标签。\n请先取消文件的「只读」属性后重试：\n%2")
                         .arg(qLReadOnly.size())
                         .arg(qLReadOnly.mid(0, 8).join(QStringLiteral("\n")));
        }
        if (!qLOther.isEmpty()) {
            qLMsg << tr("以下 %1 个文件的标签未能写入（被占用、权限不足或磁盘空间不足）：\n%2")
                         .arg(qLOther.size())
                         .arg(qLOther.mid(0, 8).join(QStringLiteral("\n")));
        }
        QMessageBox::warning(this, tr("保存标签失败"),
                             qLMsg.join(QStringLiteral("\n\n"))
                                 + tr("\n\n已有标签不会被删除，详细信息见程序目录下的 FileMarker.log。"));
    }
}

/** 记住"这次改动之前的标签"，供【撤销上一次标签操作】使用（只保留最近一次）。 */
void FormFileBrowser::onBeforeTagsChanged(const QStringList &qLFilePaths)
{
    if (qLFilePaths.isEmpty())
        return;

    UndoTagEntry entry;
    for (const QString &sFilePath : qLFilePaths) {
        if (sFilePath.isEmpty() || !QFileInfo(sFilePath).isFile())
            continue;
        const QString sClean = QDir::cleanPath(sFilePath);
        if (entry.oldTags.contains(sClean))
            continue;
        entry.oldTags.insert(sClean, ADSOperation::listADSTagsInOrder(sClean));
    }
    if (entry.oldTags.isEmpty())
        return;

    entry.desc = tr("恢复 %1 个文件的标签").arg(entry.oldTags.size());
    m_undoStack.append(entry);
    while (m_undoStack.size() > kMaxUndoDepth)
        m_undoStack.removeFirst();
    emit sendUndoStateChanged(true);
}

/** 把最近一次标签操作改回原样。 */
void FormFileBrowser::undoLastTagOperation()
{
    if (m_undoStack.isEmpty()) {
        QMessageBox::information(this, tr("撤销"), tr("没有可以撤销的标签操作。"));
        return;
    }

    const UndoTagEntry entry = m_undoStack.takeLast();
    int nFailed = 0;
    QStringList qLChanged;
    QStringList qLFailedReadOnly;
    for (auto cItr = entry.oldTags.cbegin(); cItr != entry.oldTags.cend(); ++cItr) {
        if (!ADSOperation::replaceTags(cItr.key(), cItr.value())) {
            ++nFailed;
            if (ADSOperation::isFileReadOnly(cItr.key()))
                qLFailedReadOnly << QFileInfo(cItr.key()).fileName();
            continue;
        }
        qLChanged << cItr.key();
    }

    // 缓存精确刷新 + 结果页就地刷新（撤销也是"标签变了"）
    if (!qLChanged.isEmpty()) {
        if (m_sqlOperation != nullptr)
            m_sqlOperation->refreshCachedFiles(qLChanged);
        ui->pageFilteredFiles->onFileTagsChanged(qLChanged);
    }
    emit sendUndoStateChanged(!m_undoStack.isEmpty());
    emit sendRefresh();

    if (nFailed > 0) {
        QString sMsg = tr("已恢复 %1 个文件，%2 个失败。")
                           .arg(entry.oldTags.size() - nFailed).arg(nFailed);
        if (!qLFailedReadOnly.isEmpty()) {
            sMsg += tr("\n\n其中 %1 个文件是只读的，无法写入（请先取消「只读」属性）：\n%2")
                        .arg(qLFailedReadOnly.size())
                        .arg(qLFailedReadOnly.mid(0, 8).join(QStringLiteral("\n")));
        } else {
            sMsg += tr("\n（可能被占用或权限不足）");
        }
        QMessageBox::warning(this, tr("撤销"), sMsg);
    } else {
        emit sendStatusMessage(tr("已撤销：%1（还可撤销 %2 次）")
                                   .arg(entry.desc).arg(m_undoStack.size()));
    }
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
    if (ADSOperation::isFileReadOnly(sFilePath)) {
        QMessageBox::warning(this, tr("文件是只读的"),
                             tr("无法修改【%1】的标签。\n\n"
                                "Windows 不允许在只读文件上写入附加数据流，请先取消该文件的「只读」属性后重试。")
                                 .arg(QFileInfo(sFilePath).fileName()));
        return;
    }

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
        m_sqlOperation->refreshCachedFile(sFilePath);   // 只刷新这个文件在缓存里的行

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
