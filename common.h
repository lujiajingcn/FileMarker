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
#define LOG_FILE "FilemMrker.log"

#endif // COMMON_H
