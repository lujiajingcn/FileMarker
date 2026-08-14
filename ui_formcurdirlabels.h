/********************************************************************************
** Form generated from reading UI file 'formcurdirlabels.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FORMCURDIRLABELS_H
#define UI_FORMCURDIRLABELS_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_FormCurDirLabels
{
public:
    QGridLayout *gridLayout;
    QTreeView *treeView;

    void setupUi(QWidget *FormCurDirLabels)
    {
        if (FormCurDirLabels->objectName().isEmpty())
            FormCurDirLabels->setObjectName(QString::fromUtf8("FormCurDirLabels"));
        FormCurDirLabels->resize(400, 300);
        gridLayout = new QGridLayout(FormCurDirLabels);
        gridLayout->setSpacing(0);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout->setContentsMargins(0, 0, 0, 0);
        treeView = new QTreeView(FormCurDirLabels);
        treeView->setObjectName(QString::fromUtf8("treeView"));
        treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        treeView->header()->setVisible(false);

        gridLayout->addWidget(treeView, 0, 0, 1, 1);


        retranslateUi(FormCurDirLabels);

        QMetaObject::connectSlotsByName(FormCurDirLabels);
    } // setupUi

    void retranslateUi(QWidget *FormCurDirLabels)
    {
        FormCurDirLabels->setWindowTitle(QCoreApplication::translate("FormCurDirLabels", "Form", nullptr));
    } // retranslateUi

};

namespace Ui {
    class FormCurDirLabels: public Ui_FormCurDirLabels {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FORMCURDIRLABELS_H
