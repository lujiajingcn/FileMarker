#include "threadtraversedirs.h"
#include "adsoperation.h"
#include <QDir>
#include <QCoreApplication>

ThreadTraverseDirs::ThreadTraverseDirs(QObject *parent) : QThread(parent)
{
    qRegisterMetaType<QMap<QString, QSet<QString>>>();

    m_isStop = false;

    connect(this, &ThreadTraverseDirs::finished, this, &QObject::deleteLater);
}

void ThreadTraverseDirs::stopThread()
{
    m_isStop = true;
}

void ThreadTraverseDirs::setSelDirs(QStringList selDirs)
{
    m_selDirs = selDirs;
}

void ThreadTraverseDirs::run()
{
    QMap<QString, FILE_TAGS> dirAndFileTags;
    QMap<QString, QSet<QString>> mapDirAndLabel;

    foreach(QString selDir, m_selDirs){
        FILE_TAGS fileTags;
        QSet<QString> setLabels;

        searchDirectory(selDir, fileTags, setLabels);

        dirAndFileTags[selDir] = fileTags;
        if(setLabels.count() != 0)
            mapDirAndLabel[selDir] = setLabels;
    }

    emit sendResult(dirAndFileTags, mapDirAndLabel);
}

void ThreadTraverseDirs::searchDirectory(const QString& sDirPath, FILE_TAGS &fileTags, QSet<QString> &setTags)
{
    if(m_isStop)
        return;

    QDir dir(sDirPath);
    if (!dir.exists())
        return;

    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : fileInfoList){
        if(m_isStop)
            return;

        QString filePath = fileInfo.absoluteFilePath();

        QStringList adsFileNames = ADSOperation::listADSFileName(filePath);
        fileTags[filePath] = adsFileNames;

        QSet<QString> setAdsNames = adsFileNames.toSet();
        setTags.unite(setAdsNames);

        emit sendProcessInfo(filePath);
        QCoreApplication::processEvents();

        QThread::msleep(1);//让出时间片，是ui有时间响应

        if (fileInfo.isDir())
            searchDirectory(filePath, fileTags, setTags);
    }
}

