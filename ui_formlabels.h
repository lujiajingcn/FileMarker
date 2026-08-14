/********************************************************************************
** Form generated from reading UI file 'formlabels.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FORMLABELS_H
#define UI_FORMLABELS_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_FormLabels
{
public:
    QAction *actionAdd;
    QAction *actionAddChild;
    QAction *actionDelete;
    QAction *actionMoveUp;
    QAction *actionMoveDown;
    QAction *actionMoveLeft;
    QAction *actionMoveRight;
    QGridLayout *gridLayout_2;
    QGridLayout *gridLayout;
    QToolButton *btnAddChild;
    QToolButton *btnMoveLeft;
    QToolButton *btnMoveUp;
    QToolButton *btnMoveRight;
    QToolButton *btnDelete;
    QSpacerItem *horizontalSpacer;
    QToolButton *btnMoveDown;
    QToolButton *btnAdd;
    QTreeView *treeView;

    void setupUi(QWidget *FormLabels)
    {
        if (FormLabels->objectName().isEmpty())
            FormLabels->setObjectName(QString::fromUtf8("FormLabels"));
        FormLabels->resize(460, 300);
        actionAdd = new QAction(FormLabels);
        actionAdd->setObjectName(QString::fromUtf8("actionAdd"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/Resources/addnode.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionAdd->setIcon(icon);
        actionAddChild = new QAction(FormLabels);
        actionAddChild->setObjectName(QString::fromUtf8("actionAddChild"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/Resources/addchildnode.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionAddChild->setIcon(icon1);
        actionDelete = new QAction(FormLabels);
        actionDelete->setObjectName(QString::fromUtf8("actionDelete"));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/Resources/deletenode.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionDelete->setIcon(icon2);
        actionMoveUp = new QAction(FormLabels);
        actionMoveUp->setObjectName(QString::fromUtf8("actionMoveUp"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/Resources/moveup.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionMoveUp->setIcon(icon3);
        actionMoveDown = new QAction(FormLabels);
        actionMoveDown->setObjectName(QString::fromUtf8("actionMoveDown"));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/Resources/movedown.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionMoveDown->setIcon(icon4);
        actionMoveLeft = new QAction(FormLabels);
        actionMoveLeft->setObjectName(QString::fromUtf8("actionMoveLeft"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/Resources/moveleft.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionMoveLeft->setIcon(icon5);
        actionMoveRight = new QAction(FormLabels);
        actionMoveRight->setObjectName(QString::fromUtf8("actionMoveRight"));
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/Resources/moveright.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionMoveRight->setIcon(icon6);
        gridLayout_2 = new QGridLayout(FormLabels);
        gridLayout_2->setSpacing(0);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        gridLayout_2->setContentsMargins(0, 0, 0, 0);
        gridLayout = new QGridLayout();
        gridLayout->setSpacing(0);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        btnAddChild = new QToolButton(FormLabels);
        btnAddChild->setObjectName(QString::fromUtf8("btnAddChild"));

        gridLayout->addWidget(btnAddChild, 0, 1, 1, 1);

        btnMoveLeft = new QToolButton(FormLabels);
        btnMoveLeft->setObjectName(QString::fromUtf8("btnMoveLeft"));

        gridLayout->addWidget(btnMoveLeft, 0, 5, 1, 1);

        btnMoveUp = new QToolButton(FormLabels);
        btnMoveUp->setObjectName(QString::fromUtf8("btnMoveUp"));

        gridLayout->addWidget(btnMoveUp, 0, 3, 1, 1);

        btnMoveRight = new QToolButton(FormLabels);
        btnMoveRight->setObjectName(QString::fromUtf8("btnMoveRight"));

        gridLayout->addWidget(btnMoveRight, 0, 6, 1, 1);

        btnDelete = new QToolButton(FormLabels);
        btnDelete->setObjectName(QString::fromUtf8("btnDelete"));

        gridLayout->addWidget(btnDelete, 0, 2, 1, 1);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 0, 7, 1, 1);

        btnMoveDown = new QToolButton(FormLabels);
        btnMoveDown->setObjectName(QString::fromUtf8("btnMoveDown"));

        gridLayout->addWidget(btnMoveDown, 0, 4, 1, 1);

        btnAdd = new QToolButton(FormLabels);
        btnAdd->setObjectName(QString::fromUtf8("btnAdd"));
        btnAdd->setToolButtonStyle(Qt::ToolButtonIconOnly);

        gridLayout->addWidget(btnAdd, 0, 0, 1, 1);


        gridLayout_2->addLayout(gridLayout, 0, 0, 1, 1);

        treeView = new QTreeView(FormLabels);
        treeView->setObjectName(QString::fromUtf8("treeView"));
        treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        treeView->header()->setVisible(false);

        gridLayout_2->addWidget(treeView, 1, 0, 1, 1);


        retranslateUi(FormLabels);

        QMetaObject::connectSlotsByName(FormLabels);
    } // setupUi

    void retranslateUi(QWidget *FormLabels)
    {
        FormLabels->setWindowTitle(QCoreApplication::translate("FormLabels", "Form", nullptr));
        actionAdd->setText(QCoreApplication::translate("FormLabels", "\346\267\273\345\212\240\346\240\207\347\255\276", nullptr));
        actionAddChild->setText(QCoreApplication::translate("FormLabels", "\346\267\273\345\212\240\345\255\220\346\240\207\347\255\276", nullptr));
        actionDelete->setText(QCoreApplication::translate("FormLabels", "\345\210\240\351\231\244\346\240\207\347\255\276", nullptr));
        actionMoveUp->setText(QCoreApplication::translate("FormLabels", "\344\270\212\347\247\273\346\240\207\347\255\276", nullptr));
        actionMoveDown->setText(QCoreApplication::translate("FormLabels", "\344\270\213\347\247\273\346\240\207\347\255\276", nullptr));
        actionMoveLeft->setText(QCoreApplication::translate("FormLabels", "\345\267\246\347\247\273\346\240\207\347\255\276", nullptr));
        actionMoveRight->setText(QCoreApplication::translate("FormLabels", "\345\217\263\347\247\273\346\240\207\347\255\276", nullptr));
#if QT_CONFIG(tooltip)
        btnAddChild->setToolTip(QCoreApplication::translate("FormLabels", "\346\267\273\345\212\240\345\255\220\346\240\207\347\255\276", nullptr));
#endif // QT_CONFIG(tooltip)
        btnAddChild->setText(QCoreApplication::translate("FormLabels", "...", nullptr));
#if QT_CONFIG(tooltip)
        btnMoveLeft->setToolTip(QCoreApplication::translate("FormLabels", "\345\267\246\347\247\273\346\240\207\347\255\276", nullptr));
#endif // QT_CONFIG(tooltip)
        btnMoveLeft->setText(QCoreApplication::translate("FormLabels", "...", nullptr));
#if QT_CONFIG(tooltip)
        btnMoveUp->setToolTip(QCoreApplication::translate("FormLabels", "\344\270\212\347\247\273\346\240\207\347\255\276", nullptr));
#endif // QT_CONFIG(tooltip)
        btnMoveUp->setText(QCoreApplication::translate("FormLabels", "...", nullptr));
#if QT_CONFIG(tooltip)
        btnMoveRight->setToolTip(QCoreApplication::translate("FormLabels", "\345\217\263\347\247\273\346\240\207\347\255\276", nullptr));
#endif // QT_CONFIG(tooltip)
        btnMoveRight->setText(QCoreApplication::translate("FormLabels", "...", nullptr));
#if QT_CONFIG(tooltip)
        btnDelete->setToolTip(QCoreApplication::translate("FormLabels", "\345\210\240\351\231\244\346\240\207\347\255\276", nullptr));
#endif // QT_CONFIG(tooltip)
        btnDelete->setText(QCoreApplication::translate("FormLabels", "...", nullptr));
#if QT_CONFIG(tooltip)
        btnMoveDown->setToolTip(QCoreApplication::translate("FormLabels", "\344\270\213\347\247\273\346\240\207\347\255\276", nullptr));
#endif // QT_CONFIG(tooltip)
        btnMoveDown->setText(QCoreApplication::translate("FormLabels", "...", nullptr));
#if QT_CONFIG(tooltip)
        btnAdd->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        btnAdd->setText(QCoreApplication::translate("FormLabels", "...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class FormLabels: public Ui_FormLabels {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FORMLABELS_H
