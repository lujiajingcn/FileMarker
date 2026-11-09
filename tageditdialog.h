/**
** 【标签编辑对话框】
**
** 用"列表 + 添加/移除"的方式编辑一个文件的标签，语义是"最终集合"，
** 但每一步都是显式增删，不再像以前那样把全部标签当一行文本编辑
** （以前全选后输入新名字 = 静默替换掉所有旧标签）。
**
** 输入的新标签在这里就按磁盘流名规则净化；净化后为空的直接拒绝并提示，
** 不会出现"输入了一堆非法字符、保存后标签全没了"的情况。
**/

#ifndef TAGEDITDIALOG_H
#define TAGEDITDIALOG_H

#include <QDialog>
#include <QStringList>

class QListWidget;
class QLineEdit;
class QLabel;

class TagEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TagEditDialog(const QStringList &qLExistingTags, const QString &sFileName, QWidget *parent = nullptr);

    /** 对话框里最终的标签集合（保持列表顺序） */
    QStringList tags() const;

private:
    void addFromInput();
    void removeSelected();
    void updateSummary();

    QListWidget *m_list = nullptr;
    QLineEdit   *m_edit = nullptr;
    QLabel      *m_summary = nullptr;
};

#endif // TAGEDITDIALOG_H
