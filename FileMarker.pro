#-------------------------------------------------
#
# Project created by QtCreator 2023-01-13T18:42:21
#
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT += network

TARGET = FileMarker
TEMPLATE = app
# ---- 版本信息（Windows 下会写进 exe 属性页）----
VERSION = 1.0.0
QMAKE_TARGET_PRODUCT = "FileMarker"
QMAKE_TARGET_DESCRIPTION = "FileMarker - NTFS tag manager"
QMAKE_TARGET_COMPANY = "FileMarker"
QMAKE_TARGET_COPYRIGHT = "Copyright (C) 2026 FileMarker"

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
        editablefiledelegate.cpp \
        formcurdirlabels.cpp \
        formfilebrowser.cpp \
        formfilebrowserexport.cpp \
        formfilebrowsersearch.cpp \
        formfilebrowsertagops.cpp \
        formfilenavigation.cpp \
        formlabels.cpp \
        formlabelsmisc.cpp \
        formlabelstree.cpp \
        formpagefiles.cpp \
        formpagefilesmenu.cpp \
        formpagefilesnav.cpp \
        formpagefilestags.cpp \
        formpagefilterdfiles.cpp \
        formpageprogressinfo.cpp \
        introductionwidget.cpp \
        main.cpp \
        mainwindow.cpp \
        myqfilesystemmodel.cpp \
        searchmatch.cpp \
        sqliteoperation.cpp \
        tagchipdelegate.cpp \
        tagexport.cpp \
        tageditdialog.cpp \
        threadaddtagbyai.cpp \
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
        editablefiledelegate.h \
        formcurdirlabels.h \
        formfilebrowser.h \
        formfilenavigation.h \
        formlabels.h \
        formpagefiles.h \
        formpagefilesutil.h \
        formpagefilterdfiles.h \
        formpageprogressinfo.h \
        introductionwidget.h \
        mainwindow.h \
        myqfilesystemmodel.h \
        searchmatch.h \
        sqliteoperation.h \
        tagchipdelegate.h \
        tagexport.h \
        tageditdialog.h \
        threadaddtagbyai.h \
        threadsearch.h \
        threadtraversedirs.h \
        thumbnailiconprovider.h \
        traversedirectory.h \
        utility.h \
        xmloperation.h

FORMS += \
        dlgauthor.ui \
        formcurdirlabels.ui \
        formfilebrowser.ui \
        formfilenavigation.ui \
        formlabels.ui \
        formpagefiles.ui \
        formpagefilterdfiles.ui \
        formpageprogressinfo.ui \
        mainwindow.ui


LIBS += -L$$PWD/lib/ -lsqlite3

# 缩略图：Shell 缩略图接口(IShellItemImageFactory) 需要 shell32，位图转换需要 gdi32，
# COM 初始化需要 ole32
win32: LIBS += -lshell32 -lgdi32 -lole32

# 程序图标 + 版本信息 + 高 DPI 清单（PerMonitorV2）：清单必须编进 exe，
# 否则换显示器后按 96 DPI 画，字会显得很小
win32: RC_FILE = FileMarker.rc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc
