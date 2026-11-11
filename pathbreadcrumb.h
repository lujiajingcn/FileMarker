#ifndef PATHBREADCRUMB_H
#define PATHBREADCRUMB_H

#include <QString>
#include <QWidget>

class QHBoxLayout;
class QLineEdit;
class QToolButton;

/**
 * 资源管理器风格的路径面包屑。
 *
 * 和 Windows 资源管理器的地址栏一样：路径按层拆成几段，段与段之间用 › 分隔，
 * 点哪一段就跳到哪一层；双击空白处（或按 Ctrl+L）切换成输入框，可以手打路径。
 *
 * 原来这里是一个可编辑的 QComboBox，只能看到一整条 "E:/testfiles_copy"，
 * 想回上一级只能用 ←/↑ 按钮。改成面包屑后可以直接点"我的电脑"或盘符跳转。
 */
class PathBreadcrumb : public QWidget
{
    Q_OBJECT
public:
    explicit PathBreadcrumb(QWidget *parent = nullptr);

    /** 设置当前路径（文件浏览面板的 m_curDir；"我的电脑"用空串或它的虚拟名字表示） */
    void setPath(const QString &path);
    QString path() const { return m_path; }

    /** 进入输入模式（Ctrl+L），方便手打路径 */
    void startEditing();

signals:
    /** 用户点了某一段：参数是该段对应的完整路径 */
    void pathActivated(const QString &path);
    /** 用户在输入框里回车 */
    void editCommitted(const QString &path);
    /** 用户右键（或点末尾的箭头）：请求弹出"最近访问"菜单 */
    void recentDirsRequested(const QPoint &globalPos);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuild();
    void relayoutSegments();
    void enterEditMode();
    void leaveEditMode(bool commit);
    static QString driveDisplayName(const QString &rootPath);
    static QString elided(const QString &text, int maxWidth, const QFont &font);

    QHBoxLayout *m_layout;
    QLineEdit *m_edit;
    QString m_path;
    QList<QToolButton *> m_segmentButtons;
    QList<QToolButton *> m_separators;
    QToolButton *m_overflowButton;
    bool m_editing;
};

#endif   // PATHBREADCRUMB_H
