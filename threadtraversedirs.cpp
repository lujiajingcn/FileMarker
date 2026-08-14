#include "threadtraversedirs.h"

#include "adsoperation.h"
#include "traversedirectory.h"

ThreadTraverseDirs::ThreadTraverseDirs(QObject *parent) : QThread(parent)
{
    qRegisterMetaType<QMap<QString, QSet<QString>>>();


    connect(this, &ThreadTraverseDirs::finished, this, &QObject::deleteLater);
}

void ThreadTraverseDirs::stopThread()
{
    TraverseDirectory::stop();
}

void ThreadTraverseDirs::setSelDirs(QStringList selDirs)
{
    m_selDirs = selDirs;
}

void ThreadTraverseDirs::run()
{
    auto func = [this](const QFileInfo& info) -> bool {
        return this->traverseDir(info);
    };

    QMap<QString, FILE_TAGS> dirAndFileTags;
    QMap<QString, QSet<QString>> dirAndTags;

    TraverseDirectory::resetStop();   // 每个遍历任务开头复位一次停止标志（多目录遍历也能正确响应停止，P1-3）
    foreach(QString selDir, m_selDirs){
        TraverseDirectory::traverseDirectory(selDir, func, true);

        dirAndFileTags[selDir] = m_fileTags;
        if(m_tags.count() != 0)
            dirAndTags[selDir] = m_tags;

        m_tags.clear();
        m_fileTags.clear();
    }

    emit sendResult(dirAndFileTags, dirAndTags);
}

bool ThreadTraverseDirs::traverseDir(const QFileInfo& fileInfo)
{
    QString filePath = fileInfo.absoluteFilePath();

    QStringList adsFileNames = ADSOperation::listADSFileName(filePath);
    if(adsFileNames.isEmpty())
        return true;

    m_fileTags[filePath] = adsFileNames;

    QSet<QString> setAdsNames = adsFileNames.toSet();
    m_tags.unite(setAdsNames);

    emit sendProcessInfo(filePath);

//    QThread::msleep(1);//让出时间片，使得ui有时间响应

    return true;
}
