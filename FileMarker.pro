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
        dlgauthor.cpp \
        dlglabel.cpp \
        filebrowser.cpp \
        formlabels.cpp \
        formtest.cpp \
        introductionwidget.cpp \
        main.cpp \
        mainwindow.cpp \
        myqfilesystemmodel.cpp \
        sqliteoperation.cpp \
        threadsearch.cpp \
        threadtraversedirs.cpp \
        utility.cpp \
        xmloperation.cpp

HEADERS += \
        adsoperation.h \
        common.h \
        dlgauthor.h \
        dlglabel.h \
        filebrowser.h \
        formlabels.h \
        formtest.h \
        introductionwidget.h \
        mainwindow.h \
        myqfilesystemmodel.h \
        sqliteoperation.h \
        threadsearch.h \
        threadtraversedirs.h \
        utility.h \
        welcometr.h \
        xmloperation.h

FORMS += \
        dlgauthor.ui \
        dlglabel.ui \
        filebrowser.ui \
        formlabels.ui \
        formtest.ui \
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
