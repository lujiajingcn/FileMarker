/********************************************************************************
** Form generated from reading UI file 'dlgauthor.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DLGAUTHOR_H
#define UI_DLGAUTHOR_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_DlgAuthor
{
public:
    QWidget *layoutWidget;
    QGridLayout *gridLayout;
    QLabel *label;
    QLabel *label_4;
    QLabel *label_5;
    QLabel *label_6;
    QLabel *label_3;
    QLabel *label_2;

    void setupUi(QDialog *DlgAuthor)
    {
        if (DlgAuthor->objectName().isEmpty())
            DlgAuthor->setObjectName(QString::fromUtf8("DlgAuthor"));
        DlgAuthor->resize(400, 300);
        layoutWidget = new QWidget(DlgAuthor);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(40, 80, 142, 61));
        gridLayout = new QGridLayout(layoutWidget);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout->setContentsMargins(0, 0, 0, 0);
        label = new QLabel(layoutWidget);
        label->setObjectName(QString::fromUtf8("label"));

        gridLayout->addWidget(label, 0, 0, 1, 1);

        label_4 = new QLabel(layoutWidget);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        gridLayout->addWidget(label_4, 0, 1, 1, 1);

        label_5 = new QLabel(layoutWidget);
        label_5->setObjectName(QString::fromUtf8("label_5"));

        gridLayout->addWidget(label_5, 1, 0, 1, 1);

        label_6 = new QLabel(layoutWidget);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        gridLayout->addWidget(label_6, 1, 1, 1, 1);

        label_3 = new QLabel(layoutWidget);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        gridLayout->addWidget(label_3, 2, 0, 1, 1);

        label_2 = new QLabel(layoutWidget);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        gridLayout->addWidget(label_2, 2, 1, 1, 1);


        retranslateUi(DlgAuthor);

        QMetaObject::connectSlotsByName(DlgAuthor);
    } // setupUi

    void retranslateUi(QDialog *DlgAuthor)
    {
        DlgAuthor->setWindowTitle(QCoreApplication::translate("DlgAuthor", "\344\275\234\350\200\205\344\277\241\346\201\257", nullptr));
        label->setText(QCoreApplication::translate("DlgAuthor", "\345\247\223\345\220\215\357\274\232", nullptr));
        label_4->setText(QCoreApplication::translate("DlgAuthor", "\345\215\242\345\256\266\346\225\254", nullptr));
        label_5->setText(QCoreApplication::translate("DlgAuthor", "\347\224\265\350\257\235\357\274\232", nullptr));
        label_6->setText(QString());
        label_3->setText(QCoreApplication::translate("DlgAuthor", "QQ:", nullptr));
        label_2->setText(QCoreApplication::translate("DlgAuthor", "542873980", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DlgAuthor: public Ui_DlgAuthor {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DLGAUTHOR_H
