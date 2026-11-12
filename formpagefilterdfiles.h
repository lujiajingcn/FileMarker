/**
** 【查询结果】
**
**/

#ifndef FORMPAGEFILTERDFILES_H
#define FORMPAGEFILTERDFILES_H

#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QAction>

namespace Ui {
class FormPageFilterdFiles;
}

class QTimer;

class FormPageFilterdFiles : public QWidget
{
    Q_OBJECT

public:
    explicit FormPageFilterdFiles(QWidget *parent = nullptr);
    ~FormPageFilterdFiles();

signals:
    void sendGotoFile(const QString &sFilePath);
    // 用户点击“返回目录”时，通知外层切回文件列表页面
    void sendBackRequested();
    // 用户点【重新扫描】：不用缓存，直接从磁盘把这次搜索重跑一遍
    void sendRescanRequested();

    /** 结果页里直接编辑/清除某个文件的标签（不用先回目录） */
    void sendEditTagsRequested(const QStringList &qLFilePaths);
    void sendClearTagsRequested(const QStringList &qLFilePaths);

    /** 结果页里重命名文件（F2 / 右键），路径变更由外层同步到缓存 */
    void sendRenameRequested(const QString &sFilePath, const QString &sNewName);

public slots:
    void onShowFilteredFiles(const QStringList &qLFilteredFiles);
    void onGotoFilePath();
    void setSearchCondition(const QString &sCondition);

    // 结果来源（缓存 / 磁盘扫描），显示在结果计数后面，让"缓存可能过期"这件事可见
    void setSearchSource(const QString &sSource);

    // 这次搜索是否被用户中途停掉：是的话结果不完整，必须在标题里写明
    void setSearchInterrupted(bool bInterrupted);

protected:
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    // 双击结果项：直接打开文件
    void onTvFilteredFilesDoubleClicked(const QModelIndex &index);

    // 复制选中项的路径
    void onCopyFilePath();

    // 右键菜单处理函数：构造函数中显式 connect，
    // 不再使用 on_<对象名>_<信号名> 的自动连接（connectSlotsByName）。
    void onTvFilteredFilesCustomContextMenuRequested(const QPoint &pos);

    // 取当前选中行对应文件的完整路径（第 2 列）
    QString currentFilePath() const;

    // 当前选中的全部结果路径
    QStringList selectedFilePaths() const;

    // 结果页里重命名当前行（F2 / 右键）
    void renameCurrentFileInteractive();

    // 分片读取结果的标签列，避免一次枚举上千条结果卡住界面
    void continueFillTags();

private:
    Ui::FormPageFilterdFiles *ui;

    QTreeView               *m_filteredFilesTree;
    QStandardItemModel      *m_modelFilteredFiles;
    QAction                 *m_gotoFilePath;
    QAction                 *m_revealInExplorer;
    QAction                 *m_openFile;
    QAction                 *m_copyFilePath;
    QAction                 *m_editTags;
    QAction                 *m_clearTags;
    QAction                 *m_renameFile;

    // 标签列分片填充状态
    QTimer                  *m_tagScanTimer;
    // 结果行分片填充：几万条结果一次性 appendRow 会把 UI 线程堵住（列宽还是
    // ResizeToContents，每插一行都要重算），所以分批插入、填完再恢复列宽策略。
    QTimer                  *m_rowFillTimer = nullptr;
    QStringList              m_rowSourcePaths;
    int                      m_rowSourceRow = 0;
    QString                  m_resultCountText;   // 填完后再显示"搜索结果 N 项（…）"
    void continueAddRows();
    QStringList              m_tagScanPaths;
    QString                  m_searchSource;
    int                      m_tagScanRow;
    QString                  m_searchCondition;
    bool                     m_bInterrupted = false;
};

#endif // FORMPAGEFILTERDFILES_H
