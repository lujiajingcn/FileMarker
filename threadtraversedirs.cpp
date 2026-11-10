#include "threadtraversedirs.h"

#include "adsoperation.h"
#include "utility.h"
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

void ThreadTraverseDirs::setSelDirs(const QStringList &selDirs)
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
    QStringList qLCompletedDirs;      // 只收录"完整扫完"的目录

    TraverseDirectory::resetStop();   // 每个遍历任务开头复位一次停止标志（多目录遍历也能正确响应停止）
    for (const QString &selDir : m_selDirs) {
        // 用户已点停止：后面的目录一个都不要进，避免又跑一遍
        if (TraverseDirectory::isStopped())
            break;

        const bool bCompleted = TraverseDirectory::traverseDirectory(selDir, func, true);
        if (bCompleted) {
            qLCompletedDirs << selDir;
            dirAndFileTags[selDir] = m_fileTags;
            if (m_tags.count() != 0)
                dirAndTags[selDir] = m_tags;
        }
        // 未扫完的目录：本地结果直接丢弃，绝不进入缓存（否则缓存里只剩残缺数据）

        m_tags.clear();
        m_fileTags.clear();
    }

    emit sendResult(dirAndFileTags, dirAndTags, qLCompletedDirs, TraverseDirectory::isStopped());
}

bool ThreadTraverseDirs::traverseDir(const QFileInfo& fileInfo)
{
    QString filePath = fileInfo.absoluteFilePath();

    QStringList adsFileNames = ADSOperation::listADSFileName(filePath);
    if (adsFileNames.isEmpty())
        return true;

    m_fileTags[filePath] = adsFileNames;

    QSet<QString> setAdsNames = Utility::toSet(adsFileNames);
    m_tags.unite(setAdsNames);

    emit sendProcessInfo(filePath);

// QThread::msleep(1);//让出时间片，使得ui有时间响应

    return true;
}
