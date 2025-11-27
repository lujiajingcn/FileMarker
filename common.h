#ifndef COMMON_H
#define COMMON_H

#include <QString>

#define LOGIC_AND "与"
#define LOGIC_OR "或"

#define ADS_TAIL ":$DATA"

enum labelLogic{
    AND,
    OR
//    NOT //todo 后续实现逻辑非得功能
};

#define FILTER_TAG "Zone.Identifier"    // 系统自己生成的标签

#define NEW_LABEL_NAME "新标签"

extern QString g_sAppDir;

#define SEARCH_CONFIG_FILE "config.ini"
#define LABEL_FILE "labels.xml"
#define LOG_FILE "FileMarker.log"

#define LABEL_NAME_AI "AI自动生成的标签"

#define ICON_SIZE 128   // 以缩略图方式显示文件时的图标大小
#define GRID_SIZE 200   // 以缩略图方式显示文件时的格子大小

typedef QMap<QString, QStringList> FILE_TAGS;   // 文件和该文件的标签

#endif // COMMON_H
