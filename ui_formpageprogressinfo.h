/********************************************************************************
** Form generated from reading UI file 'formpageprogressinfo.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FORMPAGEPROGRESSINFO_H
#define UI_FORMPAGEPROGRESSINFO_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_FormPageProgressInfo
{
public:
    QGridLayout *gridLayout;
    QPlainTextEdit *plainTextEdit;
    QGridLayout *gridLayout_4;
    QSpacerItem *horizontalSpacer;
    QPushButton *btnStop;

    void setupUi(QWidget *FormPageProgressInfo)
    {
        if (FormPageProgressInfo->objectName().isEmpty())
            FormPageProgressInfo->setObjectName(QString::fromUtf8("FormPageProgressInfo"));
        FormPageProgressInfo->resize(400, 300);
        gridLayout = new QGridLayout(FormPageProgressInfo);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        plainTextEdit = new QPlainTextEdit(FormPageProgressInfo);
        plainTextEdit->setObjectName(QString::fromUtf8("plainTextEdit"));

        gridLayout->addWidget(plainTextEdit, 1, 0, 1, 1);

        gridLayout_4 = new QGridLayout();
        gridLayout_4->setObjectName(QString::fromUtf8("gridLayout_4"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_4->addItem(horizontalSpacer, 1, 1, 1, 1);

        btnStop = new QPushButton(FormPageProgressInfo);
        btnStop->setObjectName(QString::fromUtf8("btnStop"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/Resources/stop.png"), QSize(), QIcon::Normal, QIcon::Off);
        btnStop->setIcon(icon);

        gridLayout_4->addWidget(btnStop, 1, 0, 1, 1);


        gridLayout->addLayout(gridLayout_4, 0, 0, 1, 1);


        retranslateUi(FormPageProgressInfo);

        QMetaObject::connectSlotsByName(FormPageProgressInfo);
    } // setupUi

    void retranslateUi(QWidget *FormPageProgressInfo)
    {
        FormPageProgressInfo->setWindowTitle(QCoreApplication::translate("FormPageProgressInfo", "Form", nullptr));
        btnStop->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class FormPageProgressInfo: public Ui_FormPageProgressInfo {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FORMPAGEPROGRESSINFO_H
