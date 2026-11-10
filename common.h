#ifndef COMMON_H
#define COMMON_H

#include <QString>

#define ADS_TAIL ":$DATA"

enum labelLogic{
    AND,
    OR
};

// 浏览器下载文件时 Windows 自己写的流，不是标签：既不显示也不删除。
#define FILTER_TAG "Zone.Identifier"

// FileMarker 自己的“标签归属清单”流：列出本程序写入的标签名。
// 有它时只信任清单里的名字，0 字节第三方 ADS 不会被误删/误判。
#define FM_TAG_MARKER "FileMarker.Owned"

#define NEW_LABEL_NAME "新标签"

// 从【标签库】把标签拖到文件列表时用的 MIME 类型（内容是若干行标签名）
#define MIME_FILEMARKER_LABEL "application/x-filemarker-label"

extern QString g_sAppDir;

#define SEARCH_CONFIG_FILE "config.ini"
#define LABEL_FILE "labels.xml"
#define LOG_FILE "FileMarker.log"

#define LABEL_NAME_AI "AI自动生成的标签"

// 缩略图模式的尺寸档位（图标边长，逻辑像素），格子边长 = 图标边长 + THUMB_GRID_MARGIN
#define ICON_SIZE_SMALL 96
#define ICON_SIZE_MEDIUM 128
#define ICON_SIZE_LARGE 192

#define THUMB_GRID_MARGIN 88    // 图标之外留给文件名（最多两行）和标签 chip（最多两行）的空间

#define ICON_SIZE ICON_SIZE_MEDIUM  // 以缩略图方式显示文件时的默认图标大小
#define GRID_SIZE (ICON_SIZE + THUMB_GRID_MARGIN)   // 以缩略图方式显示文件时的默认格子大小

typedef QMap<QString, QStringList> FILE_TAGS;   // 文件和该文件的标签

#endif // COMMON_H
