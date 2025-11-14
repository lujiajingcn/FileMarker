#include "threadtraversedirs.h"
#include "adsoperation.h"
#include <QDir>
#include <windows.h>
#include "common.h"
#include <QDebug>
#include <QCoreApplication>
#include "utility.h"

ThreadTraverseDirs::ThreadTraverseDirs(QObject *parent) : QThread(parent)
{
    qRegisterMetaType<QMap<QString, QSet<QString>>>();

    m_bIsStop = false;
    connect(this, &ThreadTraverseDirs::finished, this, &QObject::deleteLater);
}

void ThreadTraverseDirs::recvStop()
{
    m_bIsStop = true;
}

void ThreadTraverseDirs::setSelDirs(QStringList qLSelDirs)
{
    m_qLSelDirs = qLSelDirs;
}

void ThreadTraverseDirs::run()
{
    QMap<QString, QMap<QString, QStringList>> mapDirAndmapHostFilesAndLabel;
    QMap<QString, QSet<QString>> mapDirAndLabel;
    foreach(QString sSelDir, m_qLSelDirs)
    {
        QMap<QString, QStringList> mapFilePathAndLabel;
        QSet<QString> setLabels;
        searchDirectory(sSelDir, mapFilePathAndLabel, setLabels);
        mapDirAndmapHostFilesAndLabel[sSelDir] = mapFilePathAndLabel;
        mapDirAndLabel[sSelDir] = setLabels;
    }
    emit sigResult(mapDirAndmapHostFilesAndLabel);
    emit sendDirAndLabel(mapDirAndLabel);
}

void ThreadTraverseDirs::searchDirectory(const QString& sDirPath, QMap<QString, QStringList> &mapFilePathAndLabel, QSet<QString> &setLabels)
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
        if(sFilePath == "D:/filemarker_test_files/WorkSpace/Applications_Qt/xpdf_build_win/源码/xpdf-4.04/xpdf-4.04/xpdf/pdftohtml.cc")
        {
            int a = 0;
        }
        findADSName(sFilePath, mapFilePathAndLabel, setLabels);
        emit sendProcessInfo(sFilePath);
        QCoreApplication::processEvents();
        if (fileInfo.isDir())
        {
            searchDirectory(sFilePath, mapFilePathAndLabel, setLabels);
        }
    }
}

void ThreadTraverseDirs::findADSName(const QString &sFilePath, QMap<QString, QStringList> &mapFilePathAndLabel, QSet<QString> &setLabels)
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
                setLabels.insert(sADSName);
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

