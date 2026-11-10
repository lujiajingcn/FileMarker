/**
** 配置文件操作类
**
** 读写配置文件
**/

#ifndef CONFIGOPERATION_H
#define CONFIGOPERATION_H

#include "common.h"

struct SearchConfig
{
    bool bFromCache;
    labelLogic logic;
    SearchConfig()
    {
        bFromCache = true;
        logic = AND;
    }
};

class ConfigOperation
{
public:
    ConfigOperation();

    static SearchConfig readSearchConfig(const QString &sConfigFilePath);
    static void writeSearchConfig(const QString &sConfigFilePath, SearchConfig sc);
};

#endif // CONFIGOPERATION_H
