#include "threadsearch.h"
#include "adsoperation.h"
#include <QDebug>

#include "traversedirectory.h"

ThreadSearch::ThreadSearch(QObject *parent) : QThread(parent)
{
    connect(this, &ThreadSearch::finished, this, &QObject::deleteLater);
}

void ThreadSearch::stopThread()
{
    TraverseDirectory::stop();
}

void ThreadSearch::setPara(const QStringList &selDirs, const QList<QSet<QString>> &qLLabelGroups, const labelLogic taglLogic)
{
    m_selDirs = selDirs;
    m_labelGroups = qLLabelGroups;
    m_tagLogic = taglLogic;
}

void ThreadSearch::run()
{
    auto func = [this](const QFileInfo& info) -> bool {
        return this->searchFile(info);
    };

    QStringList hostFiles;
    TraverseDirectory::resetStop();   // 每个搜索任务开头复位一次停止标志（多目录搜索也能正确响应停止）
    foreach(QString selDir, m_selDirs)
        TraverseDirectory::traverseDirectory(selDir, func, true);

    qInfo() << "磁盘查找结束: scanned=" << m_nScanned << "matched=" << m_filePaths.size()
            << "logic=" << int(m_tagLogic) << "groups=" << m_labelGroups;
    emit sigResult(m_filePaths);
}

bool ThreadSearch::searchFile(const QFileInfo& fileInfo)
{
    QString sFilePath = fileInfo.absoluteFilePath();
    ++m_nScanned;
    if(ADSOperation::isHostFile(sFilePath, m_labelGroups, m_tagLogic))
        m_filePaths << sFilePath;

    emit sendProcessInfo(sFilePath);
// QThread::msleep(1);//睡眠1ms可以让界面上的进度输出顺畅，但是，整个操作耗时太多

    return true;
}


