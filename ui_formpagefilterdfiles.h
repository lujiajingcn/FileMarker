/********************************************************************************
** Form generated from reading UI file 'formpagefilterdfiles.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FORMPAGEFILTERDFILES_H
#define UI_FORMPAGEFILTERDFILES_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_FormPageFilterdFiles
{
public:
    QGridLayout *gridLayout;
    QTreeView *tvFilteredFiles;

    void setupUi(QWidget *FormPageFilterdFiles)
    {
        if (FormPageFilterdFiles->objectName().isEmpty())
            FormPageFilterdFiles->setObjectName(QString::fromUtf8("FormPageFilterdFiles"));
        FormPageFilterdFiles->resize(400, 300);
        gridLayout = new QGridLayout(FormPageFilterdFiles);
        gridLayout->setSpacing(0);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout->setContentsMargins(0, 0, 0, 0);
        tvFilteredFiles = new QTreeView(FormPageFilterdFiles);
        tvFilteredFiles->setObjectName(QString::fromUtf8("tvFilteredFiles"));
        tvFilteredFiles->setContextMenuPolicy(Qt::CustomContextMenu);
        tvFilteredFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tvFilteredFiles->setHeaderHidden(true);
        tvFilteredFiles->header()->setVisible(false);

        gridLayout->addWidget(tvFilteredFiles, 0, 0, 1, 1);


        retranslateUi(FormPageFilterdFiles);

        QMetaObject::connectSlotsByName(FormPageFilterdFiles);
    } // setupUi

    void retranslateUi(QWidget *FormPageFilterdFiles)
    {
        FormPageFilterdFiles->setWindowTitle(QCoreApplication::translate("FormPageFilterdFiles", "Form", nullptr));
    } // retranslateUi

};

namespace Ui {
    class FormPageFilterdFiles: public Ui_FormPageFilterdFiles {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FORMPAGEFILTERDFILES_H
