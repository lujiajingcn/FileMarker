/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>
#include "formcurdirlabels.h"
#include "formfilebrowser.h"
#include "formlabels.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionSearchFilesbyLabels;
    QAction *actionAddLabels;
    QAction *actionTraverseSelDirs;
    QAction *actionHelp;
    QAction *actionDelLabels;
    QAction *actionAuthor;
    QAction *actionSearchConfig;
    QAction *actionAddLabelbyAI;
    QWidget *centralWidget;
    QGridLayout *gridLayout;
    QWidget *twFileBrowser;
    QGridLayout *gridLayout_2;
    FormFileBrowser *wFileBrowser;
    QMenuBar *menuBar;
    QMenu *menuFileManage;
    QMenu *menuLabelManage;
    QMenu *menuHelp;
    QMenu *menuFileSearch;
    QToolBar *mainToolBar;
    QStatusBar *statusBar;
    QDockWidget *dwLabels;
    FormLabels *dwcLabels;
    QDockWidget *dwCurDirLabels;
    FormCurDirLabels *dwcCurDirLabels;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(976, 616);
        actionSearchFilesbyLabels = new QAction(MainWindow);
        actionSearchFilesbyLabels->setObjectName(QString::fromUtf8("actionSearchFilesbyLabels"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/Resources/search.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionSearchFilesbyLabels->setIcon(icon);
        actionAddLabels = new QAction(MainWindow);
        actionAddLabels->setObjectName(QString::fromUtf8("actionAddLabels"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/Resources/204-link.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionAddLabels->setIcon(icon1);
        actionTraverseSelDirs = new QAction(MainWindow);
        actionTraverseSelDirs->setObjectName(QString::fromUtf8("actionTraverseSelDirs"));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/Resources/database.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionTraverseSelDirs->setIcon(icon2);
        actionHelp = new QAction(MainWindow);
        actionHelp->setObjectName(QString::fromUtf8("actionHelp"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/Resources/question.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionHelp->setIcon(icon3);
        actionDelLabels = new QAction(MainWindow);
        actionDelLabels->setObjectName(QString::fromUtf8("actionDelLabels"));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/Resources/breaklink.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionDelLabels->setIcon(icon4);
        actionAuthor = new QAction(MainWindow);
        actionAuthor->setObjectName(QString::fromUtf8("actionAuthor"));
        actionSearchConfig = new QAction(MainWindow);
        actionSearchConfig->setObjectName(QString::fromUtf8("actionSearchConfig"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/Resources/cog.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionSearchConfig->setIcon(icon5);
        actionAddLabelbyAI = new QAction(MainWindow);
        actionAddLabelbyAI->setObjectName(QString::fromUtf8("actionAddLabelbyAI"));
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/Resources/ai.png"), QSize(), QIcon::Normal, QIcon::Off);
        actionAddLabelbyAI->setIcon(icon6);
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        gridLayout = new QGridLayout(centralWidget);
        gridLayout->setSpacing(1);
        gridLayout->setContentsMargins(11, 11, 11, 11);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout->setContentsMargins(1, 1, 1, 1);
        twFileBrowser = new QWidget(centralWidget);
        twFileBrowser->setObjectName(QString::fromUtf8("twFileBrowser"));
        gridLayout_2 = new QGridLayout(twFileBrowser);
        gridLayout_2->setSpacing(0);
        gridLayout_2->setContentsMargins(11, 11, 11, 11);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        gridLayout_2->setContentsMargins(0, 0, 0, 0);
        wFileBrowser = new FormFileBrowser(twFileBrowser);
        wFileBrowser->setObjectName(QString::fromUtf8("wFileBrowser"));

        gridLayout_2->addWidget(wFileBrowser, 0, 0, 1, 1);


        gridLayout->addWidget(twFileBrowser, 0, 0, 1, 1);

        MainWindow->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(MainWindow);
        menuBar->setObjectName(QString::fromUtf8("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 976, 22));
        menuFileManage = new QMenu(menuBar);
        menuFileManage->setObjectName(QString::fromUtf8("menuFileManage"));
        menuLabelManage = new QMenu(menuBar);
        menuLabelManage->setObjectName(QString::fromUtf8("menuLabelManage"));
        menuHelp = new QMenu(menuBar);
        menuHelp->setObjectName(QString::fromUtf8("menuHelp"));
        menuFileSearch = new QMenu(menuBar);
        menuFileSearch->setObjectName(QString::fromUtf8("menuFileSearch"));
        MainWindow->setMenuBar(menuBar);
        mainToolBar = new QToolBar(MainWindow);
        mainToolBar->setObjectName(QString::fromUtf8("mainToolBar"));
        mainToolBar->setMovable(true);
        mainToolBar->setFloatable(true);
        MainWindow->addToolBar(Qt::TopToolBarArea, mainToolBar);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        MainWindow->setStatusBar(statusBar);
        dwLabels = new QDockWidget(MainWindow);
        dwLabels->setObjectName(QString::fromUtf8("dwLabels"));
        dwcLabels = new FormLabels();
        dwcLabels->setObjectName(QString::fromUtf8("dwcLabels"));
        dwLabels->setWidget(dwcLabels);
        MainWindow->addDockWidget(Qt::RightDockWidgetArea, dwLabels);
        dwCurDirLabels = new QDockWidget(MainWindow);
        dwCurDirLabels->setObjectName(QString::fromUtf8("dwCurDirLabels"));
        dwcCurDirLabels = new FormCurDirLabels();
        dwcCurDirLabels->setObjectName(QString::fromUtf8("dwcCurDirLabels"));
        dwCurDirLabels->setWidget(dwcCurDirLabels);
        MainWindow->addDockWidget(Qt::RightDockWidgetArea, dwCurDirLabels);

        menuBar->addAction(menuFileManage->menuAction());
        menuBar->addAction(menuLabelManage->menuAction());
        menuBar->addAction(menuFileSearch->menuAction());
        menuBar->addAction(menuHelp->menuAction());
        menuFileManage->addAction(actionAddLabelbyAI);
        menuFileManage->addAction(actionAddLabels);
        menuFileManage->addAction(actionDelLabels);
        menuHelp->addAction(actionHelp);
        menuHelp->addAction(actionAuthor);
        menuFileSearch->addAction(actionSearchFilesbyLabels);
        menuFileSearch->addAction(actionTraverseSelDirs);
        menuFileSearch->addAction(actionSearchConfig);
        mainToolBar->addAction(actionAddLabelbyAI);
        mainToolBar->addAction(actionAddLabels);
        mainToolBar->addAction(actionDelLabels);
        mainToolBar->addAction(actionSearchFilesbyLabels);
        mainToolBar->addAction(actionTraverseSelDirs);
        mainToolBar->addAction(actionHelp);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\346\226\207\344\273\266\346\240\207\347\255\276\345\214\226\347\256\241\347\220\206\345\267\245\345\205\267", nullptr));
        actionSearchFilesbyLabels->setText(QCoreApplication::translate("MainWindow", "\346\237\245\346\211\276\346\226\207\344\273\266", nullptr));
#if QT_CONFIG(tooltip)
        actionSearchFilesbyLabels->setToolTip(QCoreApplication::translate("MainWindow", "\346\240\271\346\215\256\346\240\207\347\255\276\346\237\245\346\211\276\346\226\207\344\273\266", nullptr));
#endif // QT_CONFIG(tooltip)
        actionAddLabels->setText(QCoreApplication::translate("MainWindow", "\346\267\273\345\212\240\346\240\207\347\255\276", nullptr));
#if QT_CONFIG(tooltip)
        actionAddLabels->setToolTip(QCoreApplication::translate("MainWindow", "\344\270\272\346\226\207\344\273\266\345\205\263\350\201\224\346\240\207\347\255\276", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        actionAddLabels->setShortcut(QCoreApplication::translate("MainWindow", "A", nullptr));
#endif // QT_CONFIG(shortcut)
        actionTraverseSelDirs->setText(QCoreApplication::translate("MainWindow", "\351\201\215\345\216\206\346\226\207\344\273\266", nullptr));
#if QT_CONFIG(tooltip)
        actionTraverseSelDirs->setToolTip(QCoreApplication::translate("MainWindow", "\351\201\215\345\216\206\351\200\211\344\270\255\347\232\204\346\226\207\344\273\266\345\244\271", nullptr));
#endif // QT_CONFIG(tooltip)
        actionHelp->setText(QCoreApplication::translate("MainWindow", "\347\225\214\351\235\242\344\273\213\347\273\215", nullptr));
#if QT_CONFIG(tooltip)
        actionHelp->setToolTip(QCoreApplication::translate("MainWindow", "\345\270\256\345\212\251", nullptr));
#endif // QT_CONFIG(tooltip)
        actionDelLabels->setText(QCoreApplication::translate("MainWindow", "\345\210\240\351\231\244\346\240\207\347\255\276", nullptr));
#if QT_CONFIG(shortcut)
        actionDelLabels->setShortcut(QCoreApplication::translate("MainWindow", "D", nullptr));
#endif // QT_CONFIG(shortcut)
        actionAuthor->setText(QCoreApplication::translate("MainWindow", "\344\275\234\350\200\205\344\277\241\346\201\257", nullptr));
        actionSearchConfig->setText(QCoreApplication::translate("MainWindow", "\350\256\276\347\275\256\346\237\245\346\211\276\346\235\241\344\273\266", nullptr));
        actionAddLabelbyAI->setText(QCoreApplication::translate("MainWindow", "AI\350\207\252\345\212\250\347\224\237\346\210\220\346\240\207\347\255\276", nullptr));
        menuFileManage->setTitle(QCoreApplication::translate("MainWindow", "\346\226\207\344\273\266\347\256\241\347\220\206", nullptr));
        menuLabelManage->setTitle(QCoreApplication::translate("MainWindow", "\346\240\207\347\255\276\347\256\241\347\220\206", nullptr));
        menuHelp->setTitle(QCoreApplication::translate("MainWindow", "\345\270\256\345\212\251", nullptr));
        menuFileSearch->setTitle(QCoreApplication::translate("MainWindow", "\346\237\245\346\211\276\346\226\207\344\273\266", nullptr));
        mainToolBar->setWindowTitle(QCoreApplication::translate("MainWindow", "\345\267\245\345\205\267\346\240\217", nullptr));
        dwLabels->setWindowTitle(QCoreApplication::translate("MainWindow", "\346\240\207\347\255\276\345\272\223", nullptr));
        dwCurDirLabels->setWindowTitle(QCoreApplication::translate("MainWindow", "\345\275\223\345\211\215\347\233\256\345\275\225\344\270\255\346\226\207\344\273\266\347\232\204\346\240\207\347\255\276", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
