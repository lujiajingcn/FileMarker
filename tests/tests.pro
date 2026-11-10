#-------------------------------------------------------------------------------
# FileMarker 单元测试
#
# 覆盖三个"改一次就要手工回归一次"的模块：
#   - tst_adsoperation : NTFS 标签流读写（顺序、去重、改名、删除、第三方流保留）
#   - tst_xmloperation : 标签库 XML 往返 + 旧格式（包装层）+ 损坏文件兜底
#   - tst_searchmatch  : 搜索匹配（组内 OR、组间 AND/OR、空条件）
#
# 用法：
#   mkdir tests\build && cd tests\build
#   qmake ..\tests.pro CONFIG+=debug
#   mingw32-make -f Makefile.Debug
#   .\debug\tst_filemarker.exe
#
# 注意：ADS 相关用例需要 NTFS（系统盘或工程盘都满足）；它们在临时目录里
# 建测试文件，跑完自动删除，不会碰用户的真实文件。
#-------------------------------------------------------------------------------

QT += core testlib
QT -= gui

CONFIG += console c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_filemarker

INCLUDEPATH += $$PWD/..

SOURCES += \
    main.cpp \
    tst_adsoperation.cpp \
    tst_xmloperation.cpp \
    tst_searchmatch.cpp \
    ../adsoperation.cpp \
    ../xmloperation.cpp \
    ../utility.cpp \
    ../common.cpp

HEADERS += \
    tst_adsoperation.h \
    tst_xmloperation.h \
    tst_searchmatch.h \
    ../adsoperation.h
