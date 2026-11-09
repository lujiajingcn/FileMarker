/**
** 【文件管理】
**
**
**/
#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <QWidget>

#include "sqliteoperation.h"
#include "threadsearch.h"
#include "threadtraversedirs.h"
#include "threadaddtagbyai.h"
#include <QSet>

namespace Ui {
class FormFileBrowser;
}

enum RUNNINGTHREAD
{
    SEARCHFILE,
    TRAVERSEDIR,
    AIADDTAG
};

enum OPERATION
{
    ADDTAGBYAI_START,
    ADDTAGBYAI_FINISH,
    SEARCH_START,
    SEARCH_FINISH,
    TRAVERSER_START,
    TRAVERER_FINISH
};

class FormFileBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit FormFileBrowser(QWidget *parent = nullptr);
    ~FormFileBrowser();

    // 设置样式，大部分样式已经在filebrowser.ui中设置完成，在此进行少量样式设置
    void setWindowStyleSheet();

    void setActions(QAction *aAddLabelByAI, QAction *aAddLabel, QAction *aDelLabel, QAction *aSearch, QAction *aTraverse, QAction *aSearchConfig);

    QStringList getSelFilePath();

    QStringList getSelDirs();

    /** A2：【标签库】的改名/删除需要查询"哪些文件在用这个标签"，由主窗口把它传给 FormLabels */
    SqliteOperation *sqlOperation() const { return m_sqlOperation; }

    // 启动时恢复上次访问的目录
    void restoreLastDir();

signals:
    // 【C16】能否撤销的状态变化（供菜单项启用/禁用）
    void sendUndoStateChanged(bool bCanUndo);
    void sendStop();
    void sendShowFilteredFiles(QStringList qLFilteredFiles);
    void sendRefresh();
    void sendGotoFile(QString sFilePath);
    void sendNavigationClicked(const QModelIndex &index);
    void sendDirAndLabels(QMap<QString, QSet<QString>> mapDirAndLabel);
    void sendClearDirLabels();
    void sendStatusMessage(const QString &sMsg);
    void sendLabels(QString sLabels);
    void sendLabelsGeneratedByAI(QString sLabels);

public:
    void showProcessPage(RUNNINGTHREAD rt);
    void showFilesWidget();

public slots:
    void showFilteredFile(QStringList qLFilteredFiles);

    void onGotoFilePath(QString sFilePath);

    void onTvNavigationClicked(const QModelIndex &index);

    void onRecvSelLabels(QStringList qLSelLabels);

    /** A4：来自【标签库】的分组（每组 = 选中标签 + 它的全部子标签），搜索时组内 OR、组间按设置的逻辑 */
    void onRecvLabelGroups(const QList<QSet<QString>> &qLLabelGroups);

    // 【D5 修复】结束信号带上统计与首个错误，便于界面提示失败原因
    void onRecvAddLabelByAIFinish(int nSucceeded, int nFailed, QString sFirstError);

    void onRecvLabelsGeneratedByAI(QString sLabels);

private slots:
    /** 调用aAI大模型来自动分析文件，为文件生成标签*/
    void onActionAddLabelByAITriggered();

    /** 为文件关联标签 */
    void onActionAddLabelsTriggered();

    /** 根据标签名查找文件 */
    void onActionSearchFilesbyLabelsTriggered();

    /** 遍历选中的文件夹，查找这些文件夹中有ads信息的文件，将这些文件和ads信息写入数据库，作为下次查找的缓存信息 */
    void onActionTraverseSelDirsTriggered();

    // P1-1：多带"完整遍历过的目录"与"是否被中断"两个参数，未扫完的目录不入缓存。
    void onRecvTraverseResult(QMap<QString, FILE_TAGS> dirAndFileTags,
                              QMap<QString, QSet<QString>> dirAndTags,
                              QStringList qLCompletedDirs,
                              bool bAborted);

    void onActionSearchConfigTriggered();

    void onRecvStopSearch();

    void onRecvLabels(QString sFilePath, QString sLabels);

public slots:
    // 【C16】标签即将被改动：先记住改动前的标签，供"撤销上一次标签操作"
    void onBeforeTagsChanged(const QStringList &qLFilePaths);

    // 【C16】撤销上一次标签操作（把标签恢复成改动前的样子）
    void undoLastTagOperation();

private slots:

protected:
    void setActionState(OPERATION op);

private:
    Ui::FormFileBrowser *ui;

    QAction             *m_aAddLabelByAI;
    QAction             *m_aAddLabel;
    QAction             *m_aDelLabel;
    QAction             *m_aSearch;
    QAction             *m_aTraverse;
    QAction             *m_aSearchConfig;

    QStringList         m_qLSelLabels;          // 选中的标签，可能是【标签管理类】中选中的，也可能是【当前目录文件的标签类】选中的
    // A4：与 m_qLSelLabels 一一对应的"分组"（组 = 标签 + 它的全部子标签），搜索用这个匹配
    QList<QSet<QString>> m_qLLabelGroups;

    // 【C16】"撤销上一次标签操作"的现场：文件 → 改动前的标签列表
    QMap<QString, QStringList> m_mapUndoOldTags;
    QString                    m_sUndoDesc;

    // 【C12】上一次搜索的结果来源（缓存 / 磁盘扫描），显示在结果页
    QString                    m_sSearchSource;

    SqliteOperation     *m_sqlOperation;        // 操作sqlite数据库
    ThreadSearch        *m_threadSearch;        // 处理根据标签查找文件的线程
    ThreadTraverseDirs  *m_threadTraverseDirs;  // 遍历选中文件夹中全部文件的线程
    ThreadAddTagByAI    *m_threadAddLabelByAI;  // AI 自动生成标签的线程

    int                 m_nThread;              // 用来记录当前运行的线程：根据标签查找文件、遍历文件夹、AI自动生成标签。

    QStringList         m_qLFilePathsFromCache; // 部分目录命中缓存时的缓存命中结果，最后与磁盘查找结果合并
};

#endif // FILEBROWSER_H
