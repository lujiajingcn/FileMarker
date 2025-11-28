#include "adsoperation.h"
#include <windows.h>
#include <QDebug>
#include <QDir>
#include "common.h"
#include "utility.h"

ADSOperation::ADSOperation()
{
}

void ADSOperation::writeADSFile(const QString &filePath, const QString &streamName, const QString &text, bool isDeleteOldAds)
{
    if(isDeleteOldAds)
        deleteADSFiles(filePath);

    QString sFullFileName = filePath + ":" + streamName;
    QFile f(sFullFileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return ;

    f.write(text.toUtf8());
}

// 删除指定文件的所有关联的ads文件
void ADSOperation::deleteADSFiles(const QString &filePath)
{
    QStringList adsFileNames = listADSFileName(filePath);
    foreach(QString adsName, adsFileNames)
    {
        QString adsFilePath = filePath + ":" + adsName;
        DeleteFile(adsFilePath.toStdWString().c_str());
    }
}

// 根据文件路径，找出该文件所关联的ads文件的文件名
QStringList ADSOperation::listADSFileName(const QString &filePath)
{
    QStringList fileNames;
    WIN32_FIND_STREAM_DATA findData;

    HANDLE hFind = FindFirstStreamW(filePath.toStdWString().c_str(), FindStreamInfoStandard, &findData, 0);
    if (hFind == INVALID_HANDLE_VALUE)
        return fileNames;

    do {
        QString streamName = QString::fromWCharArray(findData.cStreamName);
        streamName = streamName.mid(1); // Remove the leading ":"
        if(Utility::isADSNameValid(streamName))
            fileNames << streamName;
    } while (FindNextStreamW(hFind, &findData));

    FindClose(hFind);
    return fileNames;
}

// 根据标签和标签的逻辑关系，判断给定的文件是否是目标文件
bool ADSOperation::isHostFile(const QString &sFilePath, const QStringList &tags, labelLogic logic)
{
    WIN32_FIND_STREAM_DATA findStreamData;
    HANDLE hFind = FindFirstStreamW(sFilePath.toStdWString().c_str(), FindStreamInfoStandard, &findStreamData, 0);
    if (hFind != INVALID_HANDLE_VALUE) {
        QStringList adsNames;
        do {
            QString adsName = QString::fromWCharArray(findStreamData.cStreamName + 1);
            if(!Utility::isADSNameValid(adsName))
                continue;

            if(logic == labelLogic::OR && tags.contains(adsName))
                return true;
            else if(logic == labelLogic::AND)
                adsNames << adsName;

        } while (FindNextStreamW(hFind, &findStreamData));
        if(adsNames.count() > 0 && Utility::isContains(adsNames, tags))
            return true;

        FindClose(hFind);
    }

    return false;
}
