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

// 缩略图模式的尺寸档位（图标边长，逻辑像素）
#define ICON_SIZE_SMALL 96
#define ICON_SIZE_MEDIUM 128
#define ICON_SIZE_LARGE 192

// 格子的横向余量只留一点点：文件名和标签都在图标下方排版，横向不需要留太多，
// 否则一格比图标宽出一大截，右侧会空出一条大尾巴（资源管理器的格子就贴着图标）。
#define THUMB_GRID_HMARGIN 36
// 纵向要留出文件名（最多两行）+ 标签 chip（最多两行）的高度
#define THUMB_GRID_VMARGIN 88

#define ICON_SIZE ICON_SIZE_MEDIUM  // 以缩略图方式显示文件时的默认图标大小
#define GRID_SIZE (ICON_SIZE + THUMB_GRID_HMARGIN)  // 缩略图模式的默认格子宽度（高度见 THUMB_GRID_VMARGIN）

typedef QMap<QString, QStringList> FILE_TAGS;   // 文件和该文件的标签

#endif // COMMON_H
