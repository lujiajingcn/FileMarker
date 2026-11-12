/**
** 【查询过程】
**
** 现在用"目录进度树"显示进度：每个搜索到的文件夹一行，带状态（等待/搜索中/完成/已停止/已跳过），
** 并且可以针对单个文件夹「停止」或「跳过」，不必整盘全停。
**
** 只显示 3 层（相对搜索根）：整盘动辄几万个目录，全量建树既慢又刷不满；
** 实测 E: 盘 3 层内只有 412 个目录，够用且界面毫无压力。更深的目录聚合到第 3 层节点上。
**/
#ifndef FORMPAGEPROGRESSINFO_H
#define FORMPAGEPROGRESSINFO_H

#include <QHash>
#include <QWidget>

#include "traversedirectory.h"

namespace Ui {
class FormPageProgressInfo;
}

class QLabel;
class QStandardItem;
class QStandardItemModel;
class QTreeView;

class FormPageProgressInfo : public QWidget
{
    Q_OBJECT

public:
    explicit FormPageProgressInfo(QWidget *parent = nullptr);
    ~FormPageProgressInfo();

    /** 开始新一轮：清空进度树，并把要搜索的根目录建成顶层节点 */
    void resetProgress(const QStringList &qLSelDirs);
    void clear();

signals:
    void sendStop();                          // 全局停止（保留原来的停止按钮）
    void sendStopDir(const QString &sPath);   // 只停止这个文件夹（及其子树）
    void sendSkipDir(const QString &sPath);   // 跳过这个文件夹（及其子树）

public slots:
    void onRecvProgress(const QString &sProcessedFilePath);
    void onRecvDirSnapshot(const DirProgressSnapshot &snapshot);

private:
    void initTree();
    /** 按路径找到/创建节点；返回 nullptr 表示层级超出显示范围 */
    QStandardItem *ensureItem(const QString &sPath, int nLevel);
    void applyNode(const DirProgressNode &node);
    void updateStatusLabel();

    // 停止按钮处理函数：构造函数中显式 connect，
    // 不再使用 on_<对象名>_<信号名> 的自动连接（connectSlotsByName）。
    void onBtnStopClicked();
    void onTreeClicked(const QModelIndex &index);

private:
    Ui::FormPageProgressInfo *ui;
    QTreeView                *m_tree = nullptr;
    QStandardItemModel       *m_model = nullptr;
    QLabel                   *m_statusLabel = nullptr;
    QHash<QString, QStandardItem *> m_items;   // 目录路径 -> 树节点
    QString                   m_sCurrentFile;
    int                       m_nScanned = 0;
    int                       m_nMatched = 0;
};

#endif // FORMPAGEPROGRESSINFO_H
