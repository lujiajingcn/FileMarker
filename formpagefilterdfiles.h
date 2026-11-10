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
    void sendGotoFile(QString sFilePath);
    // 用户点击“返回目录”时，通知外层切回文件列表页面
    void sendBackRequested();
    // 用户点【重新扫描】：不用缓存，直接从磁盘把这次搜索重跑一遍
    void sendRescanRequested();

public slots:
    void onShowFilteredFiles(QStringList qLFilteredFiles);
    void onGotoFilePath();
    void setSearchCondition(const QString &sCondition);

    // 结果来源（缓存 / 磁盘扫描），显示在结果计数后面，让"缓存可能过期"这件事可见
    void setSearchSource(const QString &sSource);

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

    // 分片读取结果的标签列，避免一次枚举上千条结果卡住界面
    void continueFillTags();

private:
    Ui::FormPageFilterdFiles *ui;

    QTreeView               *m_tvFilteredFiles;
    QStandardItemModel      *m_modelFilteredFiles;
    QAction                 *m_gotoFilePath;
    QAction                 *m_revealInExplorer;
    QAction                 *m_openFile;
    QAction                 *m_copyFilePath;

    // 标签列分片填充状态
    QTimer                  *m_pTagScanTimer;
    QStringList              m_qLTagScanPaths;
    QString                  m_sSearchSource;
    int                      m_nTagScanRow;
    QString                  m_sSearchCondition;
};

#endif // FORMPAGEFILTERDFILES_H
