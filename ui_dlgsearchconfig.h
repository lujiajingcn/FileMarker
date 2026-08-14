/********************************************************************************
** Form generated from reading UI file 'dlgsearchconfig.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DLGSEARCHCONFIG_H
#define UI_DLGSEARCHCONFIG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>

QT_BEGIN_NAMESPACE

class Ui_DlgSearchConfig
{
public:
    QGridLayout *gridLayout_5;
    QGroupBox *groupBox;
    QGridLayout *gridLayout_4;
    QGridLayout *gridLayout_2;
    QLabel *label;
    QRadioButton *rbFromDisk;
    QRadioButton *rbFromCache;
    QSpacerItem *verticalSpacer_2;
    QGridLayout *gridLayout_3;
    QLabel *label_2;
    QRadioButton *rbLogicAnd;
    QRadioButton *rbLogicOr;
    QSpacerItem *verticalSpacer;
    QGridLayout *gridLayout;
    QSpacerItem *horizontalSpacer;
    QPushButton *btnOk;
    QPushButton *btnCancel;
    QSpacerItem *horizontalSpacer_2;
    QButtonGroup *buttonGroup_2;
    QButtonGroup *buttonGroup;

    void setupUi(QDialog *DlgSearchConfig)
    {
        if (DlgSearchConfig->objectName().isEmpty())
            DlgSearchConfig->setObjectName(QString::fromUtf8("DlgSearchConfig"));
        DlgSearchConfig->resize(514, 334);
        gridLayout_5 = new QGridLayout(DlgSearchConfig);
        gridLayout_5->setObjectName(QString::fromUtf8("gridLayout_5"));
        groupBox = new QGroupBox(DlgSearchConfig);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        gridLayout_4 = new QGridLayout(groupBox);
        gridLayout_4->setObjectName(QString::fromUtf8("gridLayout_4"));
        gridLayout_2 = new QGridLayout();
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        label = new QLabel(groupBox);
        label->setObjectName(QString::fromUtf8("label"));

        gridLayout_2->addWidget(label, 0, 0, 1, 1);

        rbFromDisk = new QRadioButton(groupBox);
        buttonGroup = new QButtonGroup(DlgSearchConfig);
        buttonGroup->setObjectName(QString::fromUtf8("buttonGroup"));
        buttonGroup->addButton(rbFromDisk);
        rbFromDisk->setObjectName(QString::fromUtf8("rbFromDisk"));

        gridLayout_2->addWidget(rbFromDisk, 0, 2, 1, 1);

        rbFromCache = new QRadioButton(groupBox);
        buttonGroup->addButton(rbFromCache);
        rbFromCache->setObjectName(QString::fromUtf8("rbFromCache"));
        rbFromCache->setChecked(true);

        gridLayout_2->addWidget(rbFromCache, 0, 1, 1, 1);


        gridLayout_4->addLayout(gridLayout_2, 0, 0, 1, 1);

        verticalSpacer_2 = new QSpacerItem(20, 45, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_4->addItem(verticalSpacer_2, 1, 0, 1, 1);

        gridLayout_3 = new QGridLayout();
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        label_2 = new QLabel(groupBox);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        gridLayout_3->addWidget(label_2, 0, 0, 1, 1);

        rbLogicAnd = new QRadioButton(groupBox);
        buttonGroup_2 = new QButtonGroup(DlgSearchConfig);
        buttonGroup_2->setObjectName(QString::fromUtf8("buttonGroup_2"));
        buttonGroup_2->addButton(rbLogicAnd);
        rbLogicAnd->setObjectName(QString::fromUtf8("rbLogicAnd"));
        rbLogicAnd->setChecked(true);

        gridLayout_3->addWidget(rbLogicAnd, 0, 1, 1, 1);

        rbLogicOr = new QRadioButton(groupBox);
        buttonGroup_2->addButton(rbLogicOr);
        rbLogicOr->setObjectName(QString::fromUtf8("rbLogicOr"));

        gridLayout_3->addWidget(rbLogicOr, 0, 2, 1, 1);


        gridLayout_4->addLayout(gridLayout_3, 2, 0, 1, 1);


        gridLayout_5->addWidget(groupBox, 0, 0, 1, 1);

        verticalSpacer = new QSpacerItem(20, 138, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout_5->addItem(verticalSpacer, 1, 0, 1, 1);

        gridLayout = new QGridLayout();
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer, 0, 0, 1, 1);

        btnOk = new QPushButton(DlgSearchConfig);
        btnOk->setObjectName(QString::fromUtf8("btnOk"));

        gridLayout->addWidget(btnOk, 0, 1, 1, 1);

        btnCancel = new QPushButton(DlgSearchConfig);
        btnCancel->setObjectName(QString::fromUtf8("btnCancel"));

        gridLayout->addWidget(btnCancel, 0, 2, 1, 1);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout->addItem(horizontalSpacer_2, 0, 3, 1, 1);


        gridLayout_5->addLayout(gridLayout, 2, 0, 1, 1);


        retranslateUi(DlgSearchConfig);

        QMetaObject::connectSlotsByName(DlgSearchConfig);
    } // setupUi

    void retranslateUi(QDialog *DlgSearchConfig)
    {
        DlgSearchConfig->setWindowTitle(QCoreApplication::translate("DlgSearchConfig", "\346\237\245\346\211\276\346\235\241\344\273\266", nullptr));
        groupBox->setTitle(QString());
        label->setText(QCoreApplication::translate("DlgSearchConfig", "\346\237\245\350\257\242\344\275\215\347\275\256\357\274\232", nullptr));
        rbFromDisk->setText(QCoreApplication::translate("DlgSearchConfig", "\347\233\264\346\216\245\344\273\216\347\243\201\347\233\230\346\237\245\350\257\242", nullptr));
        rbFromCache->setText(QCoreApplication::translate("DlgSearchConfig", "\344\274\230\345\205\210\344\273\216\347\274\223\345\255\230\346\225\260\346\215\256\345\272\223\346\237\245\350\257\242", nullptr));
        label_2->setText(QCoreApplication::translate("DlgSearchConfig", "\346\237\245\350\257\242\351\200\273\350\276\221\357\274\232", nullptr));
        rbLogicAnd->setText(QCoreApplication::translate("DlgSearchConfig", "\346\240\207\347\255\276\347\232\204\351\200\273\350\276\221\345\205\263\347\263\273\344\270\272\344\270\216", nullptr));
        rbLogicOr->setText(QCoreApplication::translate("DlgSearchConfig", "\346\240\207\347\255\276\347\232\204\351\200\273\350\276\221\345\205\263\347\263\273\344\270\272\346\210\226", nullptr));
        btnOk->setText(QCoreApplication::translate("DlgSearchConfig", "\347\241\256\345\256\232", nullptr));
        btnCancel->setText(QCoreApplication::translate("DlgSearchConfig", "\345\217\226\346\266\210", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DlgSearchConfig: public Ui_DlgSearchConfig {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DLGSEARCHCONFIG_H
