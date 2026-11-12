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
    m_progressTimer.start();
    for (QString selDir : m_selDirs)TraverseDirectory::traverseDirectory(selDir, func, true);
    emit sendProcessInfo(QString("扫描结束：共扫描 %1 个文件，命中 %2 个")
                             .arg(m_scanned).arg(m_filePaths.size()));

    qInfo() << "磁盘查找结束: scanned=" << m_scanned << "matched=" << m_filePaths.size()
            << "logic=" << int(m_tagLogic) << "groups=" << m_labelGroups;
    emit sigResult(m_filePaths);
}

bool ThreadSearch::searchFile(const QFileInfo& fileInfo)
{
    QString sFilePath = fileInfo.absoluteFilePath();
    ++m_scanned;
    if (ADSOperation::isHostFile(sFilePath, m_labelGroups, m_tagLogic))
        m_filePaths << sFilePath;

    // 进度节流：以前每个文件都 emit 一次，几十万个文件会把 UI 线程的事件队列灌满，
    // 界面表现就是"一直卡着"。现在每 150ms 才报一次，并带上已扫描文件数。
    ++m_progressFiles;
    if (!m_progressTimer.isValid() || m_progressTimer.elapsed() >= 150) {
        m_progressTimer.restart();
        emit sendProcessInfo(QString("%1    （已扫描 %2 个文件）").arg(sFilePath).arg(m_scanned));
        m_progressFiles = 0;
    }

    return true;
}

