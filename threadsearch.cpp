#include "threadsearch.h"
#include "adsoperation.h"
#include <QDir>
#include <Windows.h>
#include "common.h"
#include <QDebug>
#include <QCoreApplication>
#include "utility.h"

ThreadSearch::ThreadSearch(QObject *parent) : QThread(parent)
{
    m_bIsStop = false;
    connect(this, &ThreadSearch::finished, this, &QObject::deleteLater);
}

void ThreadSearch::stopThread()
{
    m_bIsStop = true;
}

void ThreadSearch::setPara(const QStringList &sSelDirs, const QStringList &adsName, const int nLabelLogic)
{
    m_qLSelDirs = sSelDirs;
    m_adsName = adsName;
    m_nLabelLogic = nLabelLogic;
}

void ThreadSearch::run()
{
    QStringList qLHostFiles;
    foreach(QString sSelDir, m_qLSelDirs)
    {
        searchDirectory(sSelDir, qLHostFiles);
    }
    emit sigResult(qLHostFiles);
}

void ThreadSearch::searchDirectory(const QString& dirPath, QStringList &qLHostFiles)
{
    if(m_bIsStop)
    {
        emit sigResult(qLHostFiles);
        return;
    }
    QDir dir(dirPath);
    if (!dir.exists())
    {
        emit sigResult(qLHostFiles);
        return;
    }

    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : fileInfoList)
    {
        QString sFilePath = fileInfo.absoluteFilePath();
        if(isHostFile(sFilePath))
        {
            qLHostFiles << sFilePath;
        }
        emit sendProcessInfo(sFilePath);
        QCoreApplication::processEvents();
        if (fileInfo.isDir())
        {
            searchDirectory(sFilePath, qLHostFiles);
        }
    }
}

bool ThreadSearch::isHostFile(const QString &sFilePath)
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
                if(m_nLabelLogic == labelLogic::OR)
                {
                    if(m_adsName.contains(sADSName))
                    {
                        return true;
                    }
                }
                else if(m_nLabelLogic == labelLogic::AND)
                {
                    qLAdsNames << sADSName;
                }
            } while (FindNextStreamW(hFind, &findStreamData));
            if(qLAdsNames.count() > 0 && Utility::isContains(qLAdsNames, m_adsName))
            {
                return true;
            }
            FindClose(hFind);
        }
        CloseHandle(hFile);
    }
    return false;
}
