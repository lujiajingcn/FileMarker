#-------------------------------------------------
#
# Project created by QtCreator 2023-01-13T18:42:21
#
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = FileMarker
TEMPLATE = app
RC_ICONS = attachment.ico

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        adsoperation.cpp \
        common.cpp \
        configoperation.cpp \
        dlgauthor.cpp \
        dlglabel.cpp \
        dlgsearchconfig.cpp \
        formcurdirlabels.cpp \
        formfilebrowser.cpp \
        formfilenavigation.cpp \
        formlabels.cpp \
        formpagefiles.cpp \
        formpagefilterdfiles.cpp \
        formpageprogressinfo.cpp \
        introductionwidget.cpp \
        main.cpp \
        mainwindow.cpp \
        myqfilesystemmodel.cpp \
        sqliteoperation.cpp \
        threadaddlabelbyai.cpp \
        threadsearch.cpp \
        threadtraversedirs.cpp \
        thumbnailiconprovider.cpp \
        traversedirectory.cpp \
        utility.cpp \
        xmloperation.cpp

HEADERS += \
        adsoperation.h \
        common.h \
        configoperation.h \
        dlgauthor.h \
        dlglabel.h \
        dlgsearchconfig.h \
        formcurdirlabels.h \
        formfilebrowser.h \
        formfilenavigation.h \
        formlabels.h \
        formpagefiles.h \
        formpagefilterdfiles.h \
        formpageprogressinfo.h \
        introductionwidget.h \
        mainwindow.h \
        myqfilesystemmodel.h \
        sqliteoperation.h \
        threadaddlabelbyai.h \
        threadsearch.h \
        threadtraversedirs.h \
        thumbnailiconprovider.h \
        traversedirectory.h \
        utility.h \
        welcometr.h \
        xmloperation.h

FORMS += \
        dlgauthor.ui \
        dlglabel.ui \
        dlgsearchconfig.ui \
        formcurdirlabels.ui \
        formfilebrowser.ui \
        formfilenavigation.ui \
        formlabels.ui \
        formpagefiles.ui \
        formpagefilterdfiles.ui \
        formpageprogressinfo.ui \
        mainwindow.ui

INCLUDEPATH += libxml

LIBS += -L$$PWD/lib/ -lsqlite3
LIBS += -L$$PWD/lib/ -llibxml2

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc
