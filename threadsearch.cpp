#include "threadsearch.h"
#include "adsoperation.h"
#include <QCoreApplication>

#include "traversedirectory.h"

ThreadSearch::ThreadSearch(QObject *parent) : QThread(parent)
{
    connect(this, &ThreadSearch::finished, this, &QObject::deleteLater);
}

void ThreadSearch::stopThread()
{
    TraverseDirectory::stop();
}

void ThreadSearch::setPara(const QStringList &selDirs, const QStringList &tags, const labelLogic taglLogic)
{
    m_selDirs = selDirs;
    m_adsName = tags;
    m_tagLogic = taglLogic;
}

void ThreadSearch::run()
{
    auto func = [this](const QFileInfo& info) -> bool {
        return this->searchFile(info);
    };

    QStringList hostFiles;
    foreach(QString selDir, m_selDirs)
        TraverseDirectory::traverseDirectory(selDir, func, true);

    emit sigResult(m_filePaths);
}

bool ThreadSearch::searchFile(const QFileInfo& fileInfo)
{
    QString sFilePath = fileInfo.absoluteFilePath();
    if(ADSOperation::isHostFile(sFilePath, m_adsName, m_tagLogic))
        m_filePaths << sFilePath;

    emit sendProcessInfo(sFilePath);
    QCoreApplication::processEvents();
//    QThread::msleep(1);//睡眠1ms可以让界面上的进度输出顺畅，但是，整个操作耗时太多

    return true;
}


