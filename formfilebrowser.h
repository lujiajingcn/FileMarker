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
#include "threadaddlabelbyai.h"

namespace Ui {
class FormFileBrowser;
}

enum RUNNINGTHREAD
{
    SEARCHFILE,
    TRAVERSEDIR,
    AIADDTAG
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

signals:
    void sendStop();
    void sendProgress(QString sProcessedFilePath);
    void sendShowFilteredFiles(QStringList qLFilteredFiles);
    void sendRefresh();
    void sendGotoFile(QString sFilePath);
    void sendNavigationClicked(const QModelIndex &index);
    void sendDirAndLabels(QMap<QString, QSet<QString>> mapDirAndLabel);
    void sendLabels(QString sLabels);

public:
    void showProcessPage(RUNNINGTHREAD rt);
    void showFilesWidget();

public slots:
    void showFilteredFile(QStringList qLFilteredFiles);

    void recvProcessInfo(QString sProcessedFilePath);

    void onGotoFilePath(QString sFilePath);

    void onTvNavigationClicked(const QModelIndex &index);

    void onRecvSelLabels(QStringList qLSelLabels);

    void onRecvAddLabelByAIFinish();

private slots:
    void on_btnStop_clicked();

    /** 调用aAI大模型来自动分析文件，为文件生成标签*/
    void onActionAddLabelByAITriggered();

    /** 为文件关联标签 */
    void onActionAddLabelsTriggered();

    /** 根据标签名查找文件 */
    void onActionSearchFilesbyLabelsTriggered();

    /** 遍历选中的文件夹，查找这些文件夹中有ads信息的文件，将这些文件和ads信息写入数据库，作为下次查找的缓存信息 */
    void onActionTraverseSelDirsTriggered();

    void onRecvTraverseResult(QMap<QString, QMap<QString, QStringList>> mapDirAndmapHostFilesAndLabel);

    void onActionSearchConfigTriggered();

    void onRecvDirAndLabels(QMap<QString, QSet<QString>> mapDirAndLabel);

    void onRecvStopSearch();

    void onRecvLabels(QString sLabels);

private:
    Ui::FormFileBrowser *ui;

    QAction             *m_aAddLabelByAI;
    QAction             *m_aAddLabel;
    QAction             *m_aDelLabel;
    QAction             *m_aSearch;
    QAction             *m_aTraverse;
    QAction             *m_aSearchConfig;

    QStringList         m_qLSelLabels;          // 选中的标签，可能是【标签管理类】中选中的，也可能是【当前目录文件的标签类】选中的

    SqliteOperation     *m_sqlOperation;        // 操作sqlite数据库
    ThreadSearch        *m_threadSearch;        // 处理根据标签查找文件的线程
    ThreadTraverseDirs  *m_threadTraverseDirs;  // 遍历选中文件夹中全部文件的线程
    ThreadAddLabelByAI  *m_threadAddLabelByAI;

    int                 m_nThread;              // 用来记录当前运行的线程：根据标签查找文件、遍历文件夹、AI自动生成标签。
};

#endif // FILEBROWSER_H
