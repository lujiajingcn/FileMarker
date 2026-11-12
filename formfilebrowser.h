/**
** 【文件管理】
**
**
**/
#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include <QDateTime>
#include <QWidget>

class QToolButton;
class QMenu;

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

    void setActions(QAction *aAddLabelByAI, QAction *aAddLabel, QAction *aDelLabel, QAction *aSearch, QAction *aTraverse);

    QStringList getSelFilePath();

    QStringList getSelDirs(bool bFallbackToCurrent = true);

    /** 【标签库】的改名/删除需要查询"哪些文件在用这个标签"，由主窗口把它传给 FormLabels */
    SqliteOperation *sqlOperation() const { return m_sqlOperation; }

    // 启动时恢复上次访问的目录
    void restoreLastDir();

    /** 把选中目录（或当前目录）下所有文件的标签导出成 JSON 备份 */
    void exportTagsToFile();

    /** 从 JSON 备份把标签写回文件（支持指定新的根目录、覆盖或追加） */
    void importTagsFromFile();

signals:
    // 能否撤销的状态变化（供菜单项启用/禁用）
    void sendUndoStateChanged(bool bCanUndo);
    void sendStop();
    void sendShowFilteredFiles(const QStringList &qLFilteredFiles);
    void sendRefresh();
    void sendGotoFile(const QString &sFilePath);
    void sendNavigationClicked(const QModelIndex &index);
    void sendDirAndLabels(QMap<QString, QSet<QString>> mapDirAndLabel);
    void sendClearDirLabels();
    void sendStatusMessage(const QString &sMsg);
    void sendLabels(const QString &sLabels);
    void sendLabelsGeneratedByAI(const QString &sLabels);

public:
    void showProcessPage(RUNNINGTHREAD rt);
    void showFilesWidget();

public slots:

    /** 标签库把文件拖到标签上时，转给文件页去写盘（打在哪个文件上由文件页决定） */
    void onFilesDroppedOnLabel(const QStringList &qLFiles, const QStringList &qLLabels);

    void showFilteredFile(QStringList qLFilteredFiles, bool bCompleted = true);

    /** 文件被重命名：把缓存里对应的行一起改名，避免"改名后搜不到" */
    void onFileRenamedInCache(const QString &sOldPath, const QString &sNewPath);

    void onGotoFilePath(const QString &sFilePath);

    void onTvNavigationClicked(const QModelIndex &index);

    void onRecvSelLabels(const QStringList &qLSelLabels);

    /** 来自【标签库】的分组（每组 = 选中标签 + 它的全部子标签），搜索时组内 OR、组间按设置的逻辑 */
    void onRecvLabelGroups(const QList<QSet<QString>> &qLLabelGroups);

    // 结束信号带上统计与首个错误，便于界面提示失败原因
    void onRecvAddLabelByAIFinish(int nSucceeded, int nFailed, const QString &sFirstError);

    void onRecvLabelsGeneratedByAI(const QString &sLabels);

private slots:
    /** 调用aAI大模型来自动分析文件，为文件生成标签*/
    void onActionAddLabelByAITriggered();

    /** 为文件关联标签 */
    void onActionAddLabelsTriggered();

    /** 根据标签名查找文件 */
    void onActionSearchFilesbyLabelsTriggered();

    /** 结果页【重新扫描】：忽略缓存，用同样的条件从磁盘重查 */
    void onRescanFromDiskRequested();

    /** 遍历选中的文件夹，查找这些文件夹中有ads信息的文件，将这些文件和ads信息写入数据库，作为下次查找的缓存信息 */
    void onActionTraverseSelDirsTriggered();

    // 多带"完整遍历过的目录"与"是否被中断"两个参数，未扫完的目录不入缓存。
    void onRecvTraverseResult(const QMap<QString, FILE_TAGS> &dirAndFileTags,
                              QMap<QString, QSet<QString>> dirAndTags,
                              const QStringList &qLCompletedDirs,
                              const QStringList &qLSkippedDirs,
                              const QStringList &qLIncompleteDirs,
                              bool bAborted);


    void onRecvStopSearch();

    void onRecvLabels(const QString &sFilePath, const QString &sLabels);

public slots:
    // 标签即将被改动：先记住改动前的标签，供"撤销上一次标签操作"
    void onBeforeTagsChanged(const QStringList &qLFilePaths);

    // 撤销上一次标签操作（把标签恢复成改动前的样子）
    void undoLastTagOperation();

private slots:

protected:
    void setActionState(OPERATION op);

private:
    Ui::FormFileBrowser *ui;

    // 左侧【文件导航】面板的收起/展开：
    // 收起后整个面板只剩一条窄条（kNavRailWidth = 36px），按钮还在上面，点一下即可展开。
    void initNavPanelToggle();
    void setNavPanelVisible(bool bVisible, bool bSave = true);
    void saveNavPanelVisible(bool bVisible) const;

    QWidget             *m_navPanel = nullptr;      // 包住目录树的容器（含顶部收起按钮）
    QToolButton         *m_btnToggleNav = nullptr;  // 收起/展开按钮
    bool                 m_navPanelVisible = true;
    int                  m_navExpandedWidth = 200; // 展开时的宽度，收起再展开时按它恢复

    QAction             *m_aiTagAction;
    QAction             *m_addLabelAction;
    QAction             *m_clearTagsAction;
    QAction             *m_searchAction;
    QAction             *m_traverseAction;

    // 搜索按钮右侧的小箭头菜单：直接切换"标签关系 / 数据来源"，不用每次弹对话框
    QMenu               *m_searchMenu = nullptr;
    QAction             *m_logicAndAction = nullptr;
    QAction             *m_logicOrAction = nullptr;
    QAction             *m_sourceCache = nullptr;
    QAction             *m_sourceDisk = nullptr;
    void                 initSearchButtonMenu();
    void                 refreshSearchButtonMenu();
    void                 applySearchLogic(labelLogic logic);
    void                 applySearchSource(bool bFromCache);

    QStringList         m_selectedLabels;          // 选中的标签，可能是【标签管理类】中选中的，也可能是【当前目录文件的标签类】选中的
    // 与 m_selectedLabels 一一对应的"分组"（组 = 标签 + 它的全部子标签），搜索用这个匹配
    QList<QSet<QString>> m_labelGroups;

    // "撤销上一次标签操作"的现场：文件 → 改动前的标签列表
    QMap<QString, QStringList> m_mapUndoOldTags;
    QString                    m_undoDesc;

    // 上一次搜索的结果来源（缓存 / 磁盘扫描），显示在结果页
    QString                    m_searchSource;
    QString                    m_excludedDirNote;   // 结果页提示"已排除 N 个目录"

    SqliteOperation     *m_sqlOperation;        // 操作sqlite数据库
    ThreadSearch        *m_threadSearch;        // 处理根据标签查找文件的线程
    ThreadTraverseDirs  *m_threadTraverseDirs;  // 遍历选中文件夹中全部文件的线程
    ThreadAddTagByAI    *m_threadAddLabelByAI;  // AI 自动生成标签的线程

    int                 m_thread;              // 用来记录当前运行的线程：根据标签查找文件、遍历文件夹、AI自动生成标签。

    QStringList         m_cachedFilePaths; // 部分目录命中缓存时的缓存命中结果，最后与磁盘查找结果合并

    // 结果页的【重新扫描】：记录上次搜索条件，并支持强制走磁盘
    static QString humanReadableCacheAge(const QDateTime &cacheTime);
    QStringList         m_lastSearchDirs;     // 上一次搜索用的目录，重扫时复用
    QStringList         m_rescanDirs;         // 非空表示这次搜索是重扫，用这些目录
    bool                m_forceDiskScan = false;
    bool                m_longOperationRunning = false;  // 导入/导出进行中：防止重入
};

#endif // FILEBROWSER_H
