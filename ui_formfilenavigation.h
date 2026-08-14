/********************************************************************************
** Form generated from reading UI file 'formfilenavigation.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FORMFILENAVIGATION_H
#define UI_FORMFILENAVIGATION_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_FormFileNavigation
{
public:
    QHBoxLayout *horizontalLayout;
    QTreeView *tvNavigation;

    void setupUi(QWidget *FormFileNavigation)
    {
        if (FormFileNavigation->objectName().isEmpty())
            FormFileNavigation->setObjectName(QString::fromUtf8("FormFileNavigation"));
        FormFileNavigation->resize(400, 300);
        horizontalLayout = new QHBoxLayout(FormFileNavigation);
        horizontalLayout->setSpacing(0);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        tvNavigation = new QTreeView(FormFileNavigation);
        tvNavigation->setObjectName(QString::fromUtf8("tvNavigation"));

        horizontalLayout->addWidget(tvNavigation);


        retranslateUi(FormFileNavigation);

        QMetaObject::connectSlotsByName(FormFileNavigation);
    } // setupUi

    void retranslateUi(QWidget *FormFileNavigation)
    {
        FormFileNavigation->setWindowTitle(QCoreApplication::translate("FormFileNavigation", "Form", nullptr));
    } // retranslateUi

};

namespace Ui {
    class FormFileNavigation: public Ui_FormFileNavigation {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FORMFILENAVIGATION_H
