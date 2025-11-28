#include "threadsearch.h"
#include "adsoperation.h"
#include <QDir>
#include <QCoreApplication>

ThreadSearch::ThreadSearch(QObject *parent) : QThread(parent)
{
    m_isStop = false;
    connect(this, &ThreadSearch::finished, this, &QObject::deleteLater);
}

void ThreadSearch::stopThread()
{
    m_isStop = true;
}

void ThreadSearch::setPara(const QStringList &selDirs, const QStringList &tags, const labelLogic taglLogic)
{
    m_selDirs = selDirs;
    m_adsName = tags;
    m_tagLogic = taglLogic;
}

void ThreadSearch::run()
{
    QStringList hostFiles;
    foreach(QString selDir, m_selDirs)
        searchDirectory(selDir, hostFiles);

    emit sigResult(hostFiles);
}

void ThreadSearch::searchDirectory(const QString& dirPath, QStringList &hostFiles)
{
    if(m_isStop)
    {
        emit sigResult(hostFiles);
        return;
    }
    QDir dir(dirPath);
    if (!dir.exists())
    {
        emit sigResult(hostFiles);
        return;
    }

    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : fileInfoList)
    {
        QString sFilePath = fileInfo.absoluteFilePath();
        if(ADSOperation::isHostFile(sFilePath, m_adsName, m_tagLogic))
            hostFiles << sFilePath;

        emit sendProcessInfo(sFilePath);
        QCoreApplication::processEvents();
        if (fileInfo.isDir())
            searchDirectory(sFilePath, hostFiles);
    }
}


