#include "adsoperation.h"
#include <windows.h>
#include <QDebug>
#include <QDir>
#include "common.h"
#include "utility.h"

ADSOperation::ADSOperation()
{
}

void ADSOperation::readADS(const QString &sFilePath, const QString &sStreamName)
{
    QString sFullFileName = sFilePath + ":" + sStreamName;
    HANDLE hFile = CreateFile((LPCWSTR)sFullFileName.utf16(),
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesRead;
        char buffer[1024]; // adjust buffer size as needed
        if (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL))
        {
            QString sErrorMsg = QString("读取文件%1附属的ads失败").arg(sFullFileName);
            qCritical("%s", sErrorMsg.toStdString().c_str());
        }
        CloseHandle(hFile);
    }
    else
    {
        QString sErrorMsg = QString("打开文件%1失败").arg(sFullFileName);
        qCritical("%s", sErrorMsg.toStdString().c_str());
    }
}

void ADSOperation::writeADS(const QString &sFilePath, const QString &sStreamName, const QString &sData, bool isDeleteOldAds)
{
    if(isDeleteOldAds)
    {
        deleteADS(sFilePath);
    }
    QString sFullFileName = sFilePath + ":" + sStreamName;
    HANDLE hFile = CreateFile((LPCWSTR)sFullFileName.utf16(),
                              GENERIC_WRITE,
                              0,
                              NULL,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD bytesWritten;
        if (WriteFile(hFile, sData.toUtf8().constData(), sData.toUtf8().length(), &bytesWritten, NULL))
        {
            QString sErrorMsg = QString("写入文件%1附属的ads失败").arg(sFullFileName);
            qCritical("%s", sErrorMsg.toStdString().c_str());
        }
        CloseHandle(hFile);
    }
    else
    {
        QString sErrorMsg = QString("打开文件%1失败").arg(sFullFileName);
        qCritical("%s", sErrorMsg.toStdString().c_str());
    }
}

void ADSOperation::deleteADS(const QString &sFilePath)
{
    QStringList qLAdsNames = listADS(sFilePath);
    foreach(QString sAdsName, qLAdsNames)
    {
        QString sFullName = sFilePath + ":" + sAdsName;
        DeleteFile((LPCWSTR)sFullName.utf16());
    }
}

QStringList ADSOperation::listADS(const QString &sFilePath)
{
    QStringList qLLabes;
    LPCWSTR lpFileName = reinterpret_cast<LPCWSTR>(sFilePath.utf16());
    WIN32_FIND_STREAM_DATA findData;

    // Open a handle to the file
    HANDLE hFind = FindFirstStreamW(lpFileName, FindStreamInfoStandard, &findData, 0);
    if (hFind == INVALID_HANDLE_VALUE) {
        return qLLabes;
    }

    do {
        QString streamName = QString::fromWCharArray(findData.cStreamName);
        if (streamName != "::$DATA" && streamName.startsWith(L":")) {
            streamName = streamName.mid(1); // Remove the leading ":"
            qLLabes << streamName;
        }
    } while (FindNextStreamW(hFind, &findData));

    FindClose(hFind);

    return qLLabes;
}

QStringList ADSOperation::listADSFileName(const QString &sFilePath)
{
    QStringList qLLabes;
    LPCWSTR lpFileName = reinterpret_cast<LPCWSTR>(sFilePath.utf16());
    WIN32_FIND_STREAM_DATA findData;

    HANDLE hFind = FindFirstStreamW(lpFileName, FindStreamInfoStandard, &findData, 0);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return qLLabes;
    }

    do {
        QString streamName = QString::fromWCharArray(findData.cStreamName);
        streamName = streamName.mid(1); // Remove the leading ":"
        if(Utility::isADSNameValue(streamName))
        {
            qLLabes << streamName;
        }

    } while (FindNextStreamW(hFind, &findData));

    FindClose(hFind);

    return qLLabes;
}
