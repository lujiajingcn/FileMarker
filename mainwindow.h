#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSet>

#include "filebrowser.h"
#include "formlabels.h"
#include "threadsearch.h"
#include "threadtraversedirs.h"
#include "introductionwidget.h"
#include "sqliteoperation.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void sendRefresh();

private slots:
    /** 为文件关联标签 */
    void on_actionAddLabels_triggered();

    /** 根据标签名查找文件 */
    void on_actionSearchFilesbyLabels_triggered();

    /** 遍历选中的文件夹，查找这些文件夹中有ads信息的文件，将这些文件和ads信息写入数据库，作为下次查找的缓存信息 */
    void on_actionTraverseSelDirs_triggered();

    void onRecvTraverseResult(QMap<QString, QMap<QString, QStringList>> mapDirAndmapHostFilesAndLabel);

    void on_actionHelp_triggered();

    void recvStop();

    /** 查找操作的两对互斥条件的状态变化 */
    void on_actionFromCache_changed();
    void on_actionFromDisk_changed();
    void on_actionLabelLogicAnd_changed();
    void on_actionLabelLogicOr_changed();

    void on_actionAuthor_triggered();

protected:
    void initLogMessageHanlder();
    void readSearchConfig();
    void writeSearchConfig();

private:
    SqliteOperation         *m_sqlOperation;        // 操作sqlite数据库
    FileBrowser             *m_fileBrowser;         // 文件操作
    FormLabels              *m_formLabels;          // 标签管理
    ThreadSearch            *m_threadSearch;        // 处理根据标签查找文件的线程
    ThreadTraverseDirs      *m_threadTraverseDirs;  // 遍历选中文件夹中全部文件的线程
    QFile                   m_logFile;
    IntroductionWidget *w;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
