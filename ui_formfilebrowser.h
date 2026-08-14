/********************************************************************************
** Form generated from reading UI file 'formfilebrowser.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FORMFILEBROWSER_H
#define UI_FORMFILEBROWSER_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>
#include <formfilenavigation.h>
#include "formpagefiles.h"
#include "formpagefilterdfiles.h"
#include "formpageprogressinfo.h"

QT_BEGIN_NAMESPACE

class Ui_FormFileBrowser
{
public:
    QHBoxLayout *horizontalLayout_2;
    QWidget *widget;
    QHBoxLayout *horizontalLayout;
    QSplitter *splitter;
    FormFileNavigation *wFileNavigation;
    QStackedWidget *stackedWidget;
    FormPageFiles *pageFiles;
    QGridLayout *gridLayout_3;
    FormPageFilterdFiles *pageFilteredFiles;
    QGridLayout *gridLayout;
    FormPageProgressInfo *pageProgressInfo;
    QGridLayout *gridLayout_5;

    void setupUi(QWidget *FormFileBrowser)
    {
        if (FormFileBrowser->objectName().isEmpty())
            FormFileBrowser->setObjectName(QString::fromUtf8("FormFileBrowser"));
        FormFileBrowser->resize(572, 511);
        FormFileBrowser->setMinimumSize(QSize(0, 0));
        horizontalLayout_2 = new QHBoxLayout(FormFileBrowser);
        horizontalLayout_2->setSpacing(0);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
        widget = new QWidget(FormFileBrowser);
        widget->setObjectName(QString::fromUtf8("widget"));
        horizontalLayout = new QHBoxLayout(widget);
        horizontalLayout->setSpacing(0);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        splitter = new QSplitter(widget);
        splitter->setObjectName(QString::fromUtf8("splitter"));
        splitter->setOrientation(Qt::Horizontal);
        splitter->setHandleWidth(5);
        wFileNavigation = new FormFileNavigation(splitter);
        wFileNavigation->setObjectName(QString::fromUtf8("wFileNavigation"));
        splitter->addWidget(wFileNavigation);
        stackedWidget = new QStackedWidget(splitter);
        stackedWidget->setObjectName(QString::fromUtf8("stackedWidget"));
        pageFiles = new FormPageFiles();
        pageFiles->setObjectName(QString::fromUtf8("pageFiles"));
        gridLayout_3 = new QGridLayout(pageFiles);
        gridLayout_3->setSpacing(0);
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        gridLayout_3->setContentsMargins(0, 0, 0, 0);
        stackedWidget->addWidget(pageFiles);
        pageFilteredFiles = new FormPageFilterdFiles();
        pageFilteredFiles->setObjectName(QString::fromUtf8("pageFilteredFiles"));
        gridLayout = new QGridLayout(pageFilteredFiles);
        gridLayout->setSpacing(0);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout->setContentsMargins(0, 0, 0, 0);
        stackedWidget->addWidget(pageFilteredFiles);
        pageProgressInfo = new FormPageProgressInfo();
        pageProgressInfo->setObjectName(QString::fromUtf8("pageProgressInfo"));
        gridLayout_5 = new QGridLayout(pageProgressInfo);
        gridLayout_5->setObjectName(QString::fromUtf8("gridLayout_5"));
        stackedWidget->addWidget(pageProgressInfo);
        splitter->addWidget(stackedWidget);

        horizontalLayout->addWidget(splitter);


        horizontalLayout_2->addWidget(widget);


        retranslateUi(FormFileBrowser);

        stackedWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(FormFileBrowser);
    } // setupUi

    void retranslateUi(QWidget *FormFileBrowser)
    {
        FormFileBrowser->setWindowTitle(QCoreApplication::translate("FormFileBrowser", "Form", nullptr));
    } // retranslateUi

};

namespace Ui {
    class FormFileBrowser: public Ui_FormFileBrowser {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FORMFILEBROWSER_H
