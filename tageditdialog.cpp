#include "tageditdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>

#include "adsoperation.h"
#include "utility.h"

TagEditDialog::TagEditDialog(const QStringList &qLExistingTags, const QString &sFileName, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("编辑标签"));
    resize(540, 420);

    QVBoxLayout *pVLayout = new QVBoxLayout(this);

    QLabel *pFileLabel = new QLabel(tr("文件：%1").arg(sFileName), this);
    pFileLabel->setWordWrap(true);
    pVLayout->addWidget(pFileLabel);

    QLabel *pListLabel = new QLabel(tr("当前标签（选中后点【移除所选】可删除；保存时以这个列表为准）："), this);
    pListLabel->setWordWrap(true);
    pVLayout->addWidget(pListLabel);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->addItems(qLExistingTags);
    pVLayout->addWidget(m_list, 1);

    QHBoxLayout *pAddLayout = new QHBoxLayout();
    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(tr("输入要追加的标签，多个用逗号/顿号/分号分隔，回车添加"));
    m_edit->setClearButtonEnabled(true);
    pAddLayout->addWidget(m_edit, 1);

    QPushButton *pBtnAdd = new QPushButton(tr("添加"), this);
    pBtnAdd->setDefault(true);
    pAddLayout->addWidget(pBtnAdd);

    QPushButton *pBtnRemove = new QPushButton(tr("移除所选"), this);
    pAddLayout->addWidget(pBtnRemove);
    pVLayout->addLayout(pAddLayout);

    m_summary = new QLabel(this);
    m_summary->setStyleSheet(QStringLiteral("color:#666;"));
    pVLayout->addWidget(m_summary);

    QDialogButtonBox *pButtonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    // 显式设置中文按钮：QDialogButtonBox 的默认文字来自 Qt 自带翻译，未加载 .qm 时是英文。
    pButtonBox->button(QDialogButtonBox::Save)->setText(tr("保存"));
    pButtonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    pVLayout->addWidget(pButtonBox);

    connect(pBtnAdd, &QPushButton::clicked, this, &TagEditDialog::addFromInput);
    connect(pBtnRemove, &QPushButton::clicked, this, &TagEditDialog::removeSelected);
    connect(m_edit, &QLineEdit::returnPressed, this, &TagEditDialog::addFromInput);
    connect(pButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &TagEditDialog::updateSummary);

    updateSummary();
    m_edit->setFocus();
}

void TagEditDialog::addFromInput()
{
    const QString sRaw = m_edit->text().trimmed();
    if (sRaw.isEmpty())
        return;

    QStringList qLBad;
    int nAdded = 0;
    foreach (const QString &sPart, Utility::splitLabels(sRaw)) {
        const QString sSanitized = ADSOperation::sanitizeTagName(sPart);
        if (sSanitized.isEmpty()) {
            qLBad << sPart;
            continue;
        }
        bool bExists = false;
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->text().compare(sSanitized, Qt::CaseInsensitive) == 0) {
                bExists = true;
                break;
            }
        }
        if (bExists)
            continue;
        m_list->addItem(sSanitized);
        ++nAdded;
    }

    m_edit->clear();
    if (!qLBad.isEmpty()) {
        QMessageBox::warning(this, tr("标签名无效"),
                             tr("以下标签名不合法（不能用 : / \\ * ? \" < > | 和分隔符），已忽略：\n%1")
                                 .arg(qLBad.join(QStringLiteral("、"))));
    }
    if (nAdded > 0)
        m_list->setCurrentRow(m_list->count() - 1);
    updateSummary();
}

void TagEditDialog::removeSelected()
{
    const QList<QListWidgetItem *> qLSel = m_list->selectedItems();
    for (QListWidgetItem *pItem : qLSel)
        delete m_list->takeItem(m_list->row(pItem));
    updateSummary();
}

void TagEditDialog::updateSummary()
{
    const int nCount = m_list->count();
    m_summary->setText(nCount == 0
                           ? tr("当前列表为空：保存后该文件将没有任何标签。")
                           : tr("保存后该文件将有 %1 个标签。").arg(nCount));
}

QStringList TagEditDialog::tags() const
{
    QStringList qLTags;
    for (int i = 0; i < m_list->count(); ++i)
        qLTags << m_list->item(i)->text();
    return qLTags;
}
