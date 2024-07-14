#include "threadtraversedirs.h"
#include "adsoperation.h"
#include <QDir>
#include <Windows.h>
#include "common.h"
#include <QDebug>
#include <QCoreApplication>
#include "utility.h"
//#include <QDateTime>

ThreadTraverseDirs::ThreadTraverseDirs(QObject *parent) : QThread(parent)
{
    m_bIsStop = false;
    connect(this, &ThreadTraverseDirs::finished, this, &QObject::deleteLater);
}

void ThreadTraverseDirs::recvStop()
{
    qDebug()<<"recvStop";
    m_bIsStop = true;
}

void ThreadTraverseDirs::setSelDirs(QStringList qLSelDirs)
{
    m_qLSelDirs = qLSelDirs;
}

void ThreadTraverseDirs::run()
{
    QMap<QString, QMap<QString, QStringList>> mapDirAndmapHostFilesAndLabel;
    foreach(QString sSelDir, m_qLSelDirs)
    {
        QMap<QString, QStringList> mapFilePathAndLabel;
        searchDirectory(sSelDir, mapFilePathAndLabel);
        mapDirAndmapHostFilesAndLabel[sSelDir] = mapFilePathAndLabel;
    }
    emit sigResult(mapDirAndmapHostFilesAndLabel);
}

void ThreadTraverseDirs::searchDirectory(const QString& sDirPath, QMap<QString, QStringList> &mapFilePathAndLabel)
{
    if(m_bIsStop)
    {
        return;
    }
    QDir dir(sDirPath);
    if (!dir.exists())
    {
        return;
    }

    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : fileInfoList)
    {
        if(m_bIsStop)
        {
            return;
        }
        QString sFilePath = fileInfo.absoluteFilePath();
        findADSName(sFilePath, mapFilePathAndLabel);
        emit sendProcessInfo(sFilePath);
        QCoreApplication::processEvents();
        if (fileInfo.isDir())
        {
            searchDirectory(sFilePath, mapFilePathAndLabel);
        }
    }
}

void ThreadTraverseDirs::findADSName(const QString &sFilePath, QMap<QString, QStringList> &mapFilePathAndLabel)
{
    HANDLE hFile = CreateFile(sFilePath.toStdWString().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        WIN32_FIND_STREAM_DATA findStreamData;
        HANDLE hFind = FindFirstStreamW(sFilePath.toStdWString().c_str(), FindStreamInfoStandard, &findStreamData, 0);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            QStringList qLAdsNames;
            do {
                QString sADSName = QString::fromWCharArray(findStreamData.cStreamName + 1);
                if(!Utility::isADSNameValue(sADSName))
                {
                    continue;
                }
                qLAdsNames << sADSName;
            } while (FindNextStreamW(hFind, &findStreamData));
            if(qLAdsNames.count() > 0)
            {
                mapFilePathAndLabel[sFilePath] = qLAdsNames;
            }
            FindClose(hFind);
        }
        CloseHandle(hFile);
    }
}

