/********************************************************************************
** Form generated from reading UI file 'formpagefiles.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FORMPAGEFILES_H
#define UI_FORMPAGEFILES_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QListView>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_FormPageFiles
{
public:
    QGridLayout *gridLayout_3;
    QWidget *wFileListToolBar;
    QVBoxLayout *verticalLayout;
    QWidget *widget_2;
    QGridLayout *gridLayout_7;
    QGridLayout *gridLayout_2;
    QToolButton *toParentButton;
    QToolButton *forwardButton;
    QComboBox *lookInCombo;
    QToolButton *btnRefresh;
    QToolButton *backButton;
    QWidget *widget_3;
    QGridLayout *gridLayout_9;
    QGridLayout *gridLayout_8;
    QSpacerItem *horizontalSpacer_2;
    QToolButton *tbThumbnail;
    QToolButton *tbDetail;
    QStackedWidget *stackedWidget;
    QWidget *page;
    QGridLayout *gridLayout;
    QTreeView *tvFiles;
    QWidget *page_2;
    QGridLayout *gridLayout_4;
    QListView *listView;

    void setupUi(QWidget *FormPageFiles)
    {
        if (FormPageFiles->objectName().isEmpty())
            FormPageFiles->setObjectName(QString::fromUtf8("FormPageFiles"));
        FormPageFiles->resize(564, 495);
        gridLayout_3 = new QGridLayout(FormPageFiles);
        gridLayout_3->setSpacing(0);
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        gridLayout_3->setContentsMargins(0, 0, 0, 0);
        wFileListToolBar = new QWidget(FormPageFiles);
        wFileListToolBar->setObjectName(QString::fromUtf8("wFileListToolBar"));
        verticalLayout = new QVBoxLayout(wFileListToolBar);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        widget_2 = new QWidget(wFileListToolBar);
        widget_2->setObjectName(QString::fromUtf8("widget_2"));
        widget_2->setAutoFillBackground(false);
        widget_2->setStyleSheet(QString::fromUtf8(""));
        gridLayout_7 = new QGridLayout(widget_2);
        gridLayout_7->setSpacing(0);
        gridLayout_7->setObjectName(QString::fromUtf8("gridLayout_7"));
        gridLayout_7->setContentsMargins(0, 0, 0, 0);
        gridLayout_2 = new QGridLayout();
        gridLayout_2->setSpacing(0);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        gridLayout_2->setContentsMargins(0, 0, 0, 0);
        toParentButton = new QToolButton(widget_2);
        toParentButton->setObjectName(QString::fromUtf8("toParentButton"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/Resources/arrow-up.png"), QSize(), QIcon::Normal, QIcon::Off);
        toParentButton->setIcon(icon);
        toParentButton->setAutoRaise(true);

        gridLayout_2->addWidget(toParentButton, 0, 3, 1, 1);

        forwardButton = new QToolButton(widget_2);
        forwardButton->setObjectName(QString::fromUtf8("forwardButton"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/Resources/arrow-right.png"), QSize(), QIcon::Normal, QIcon::Off);
        forwardButton->setIcon(icon1);
        forwardButton->setAutoRaise(true);

        gridLayout_2->addWidget(forwardButton, 0, 1, 1, 1);

        lookInCombo = new QComboBox(widget_2);
        lookInCombo->setObjectName(QString::fromUtf8("lookInCombo"));
        QSizePolicy sizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(1);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(lookInCombo->sizePolicy().hasHeightForWidth());
        lookInCombo->setSizePolicy(sizePolicy);
        lookInCombo->setMinimumSize(QSize(30, 0));
        lookInCombo->setEditable(true);

        gridLayout_2->addWidget(lookInCombo, 0, 5, 1, 1);

        btnRefresh = new QToolButton(widget_2);
        btnRefresh->setObjectName(QString::fromUtf8("btnRefresh"));
        btnRefresh->setEnabled(true);
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/Resources/spinner.png"), QSize(), QIcon::Normal, QIcon::Off);
        btnRefresh->setIcon(icon2);
        btnRefresh->setAutoRaise(true);
        btnRefresh->setArrowType(Qt::NoArrow);

        gridLayout_2->addWidget(btnRefresh, 0, 4, 1, 1);

        backButton = new QToolButton(widget_2);
        backButton->setObjectName(QString::fromUtf8("backButton"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/Resources/arrow-left.png"), QSize(), QIcon::Normal, QIcon::Off);
        backButton->setIcon(icon3);
        backButton->setAutoRaise(true);

        gridLayout_2->addWidget(backButton, 0, 0, 1, 1);

        gridLayout_2->setColumnStretch(0, 2);

        gridLayout_7->addLayout(gridLayout_2, 0, 0, 1, 1);


        verticalLayout->addWidget(widget_2);

        widget_3 = new QWidget(wFileListToolBar);
        widget_3->setObjectName(QString::fromUtf8("widget_3"));
        widget_3->setStyleSheet(QString::fromUtf8(""));
        gridLayout_9 = new QGridLayout(widget_3);
        gridLayout_9->setSpacing(0);
        gridLayout_9->setObjectName(QString::fromUtf8("gridLayout_9"));
        gridLayout_9->setContentsMargins(0, 0, 0, 0);
        gridLayout_8 = new QGridLayout();
        gridLayout_8->setSpacing(0);
        gridLayout_8->setObjectName(QString::fromUtf8("gridLayout_8"));
        gridLayout_8->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_8->addItem(horizontalSpacer_2, 0, 2, 1, 1);

        tbThumbnail = new QToolButton(widget_3);
        tbThumbnail->setObjectName(QString::fromUtf8("tbThumbnail"));

        gridLayout_8->addWidget(tbThumbnail, 0, 4, 1, 1);

        tbDetail = new QToolButton(widget_3);
        tbDetail->setObjectName(QString::fromUtf8("tbDetail"));

        gridLayout_8->addWidget(tbDetail, 0, 3, 1, 1);


        gridLayout_9->addLayout(gridLayout_8, 0, 0, 1, 1);


        verticalLayout->addWidget(widget_3);


        gridLayout_3->addWidget(wFileListToolBar, 0, 0, 1, 1);

        stackedWidget = new QStackedWidget(FormPageFiles);
        stackedWidget->setObjectName(QString::fromUtf8("stackedWidget"));
        page = new QWidget();
        page->setObjectName(QString::fromUtf8("page"));
        gridLayout = new QGridLayout(page);
        gridLayout->setSpacing(0);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout->setContentsMargins(0, 0, 0, 0);
        tvFiles = new QTreeView(page);
        tvFiles->setObjectName(QString::fromUtf8("tvFiles"));
        tvFiles->setContextMenuPolicy(Qt::CustomContextMenu);
        tvFiles->setProperty("showDropIndicator", QVariant(true));
        tvFiles->setSelectionMode(QAbstractItemView::SingleSelection);
        tvFiles->setRootIsDecorated(false);
        tvFiles->setItemsExpandable(false);
        tvFiles->setWordWrap(false);
        tvFiles->setHeaderHidden(false);

        gridLayout->addWidget(tvFiles, 0, 0, 1, 1);

        stackedWidget->addWidget(page);
        page_2 = new QWidget();
        page_2->setObjectName(QString::fromUtf8("page_2"));
        gridLayout_4 = new QGridLayout(page_2);
        gridLayout_4->setSpacing(0);
        gridLayout_4->setObjectName(QString::fromUtf8("gridLayout_4"));
        gridLayout_4->setContentsMargins(0, 0, 0, 0);
        listView = new QListView(page_2);
        listView->setObjectName(QString::fromUtf8("listView"));
        listView->setDefaultDropAction(Qt::IgnoreAction);
        listView->setSelectionMode(QAbstractItemView::SingleSelection);

        gridLayout_4->addWidget(listView, 0, 0, 1, 1);

        stackedWidget->addWidget(page_2);

        gridLayout_3->addWidget(stackedWidget, 1, 0, 1, 1);


        retranslateUi(FormPageFiles);

        stackedWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(FormPageFiles);
    } // setupUi

    void retranslateUi(QWidget *FormPageFiles)
    {
        FormPageFiles->setWindowTitle(QCoreApplication::translate("FormPageFiles", "Form", nullptr));
#if QT_CONFIG(tooltip)
        toParentButton->setToolTip(QCoreApplication::translate("FormPageFiles", "Parent Directory", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(accessibility)
        toParentButton->setAccessibleName(QCoreApplication::translate("FormPageFiles", "Parent Directory", nullptr));
#endif // QT_CONFIG(accessibility)
#if QT_CONFIG(accessibility)
        toParentButton->setAccessibleDescription(QCoreApplication::translate("FormPageFiles", "Go to the parent directory", nullptr));
#endif // QT_CONFIG(accessibility)
#if QT_CONFIG(shortcut)
        toParentButton->setShortcut(QCoreApplication::translate("FormPageFiles", "Alt+Up", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        forwardButton->setToolTip(QCoreApplication::translate("FormPageFiles", "Forward", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(accessibility)
        forwardButton->setAccessibleName(QCoreApplication::translate("FormPageFiles", "Forward", nullptr));
#endif // QT_CONFIG(accessibility)
#if QT_CONFIG(accessibility)
        forwardButton->setAccessibleDescription(QCoreApplication::translate("FormPageFiles", "Go forward", nullptr));
#endif // QT_CONFIG(accessibility)
#if QT_CONFIG(shortcut)
        forwardButton->setShortcut(QCoreApplication::translate("FormPageFiles", "Alt+Right", nullptr));
#endif // QT_CONFIG(shortcut)
        btnRefresh->setText(QCoreApplication::translate("FormPageFiles", "...", nullptr));
#if QT_CONFIG(tooltip)
        backButton->setToolTip(QCoreApplication::translate("FormPageFiles", "Back", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(accessibility)
        backButton->setAccessibleName(QCoreApplication::translate("FormPageFiles", "Back", nullptr));
#endif // QT_CONFIG(accessibility)
#if QT_CONFIG(accessibility)
        backButton->setAccessibleDescription(QCoreApplication::translate("FormPageFiles", "Go back", nullptr));
#endif // QT_CONFIG(accessibility)
#if QT_CONFIG(shortcut)
        backButton->setShortcut(QCoreApplication::translate("FormPageFiles", "Alt+Left", nullptr));
#endif // QT_CONFIG(shortcut)
        tbThumbnail->setText(QCoreApplication::translate("FormPageFiles", "\347\274\251\347\225\245\345\233\276\346\230\276\347\244\272", nullptr));
        tbDetail->setText(QCoreApplication::translate("FormPageFiles", "\350\257\246\346\203\205\346\230\276\347\244\272", nullptr));
    } // retranslateUi

};

namespace Ui {
    class FormPageFiles: public Ui_FormPageFiles {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FORMPAGEFILES_H
