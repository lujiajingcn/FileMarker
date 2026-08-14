#include "traversedirectory.h"

std::atomic<bool> TraverseDirectory::m_bIsStop{false};

TraverseDirectory::TraverseDirectory()
{
}

void TraverseDirectory::traverseDirectory(const QString& path, FileAction action, bool recursive)
{
    QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, flags);
    while (it.hasNext())
    {
        it.next();
        bool shouldContinue = action(it.fileInfo());
        if (!shouldContinue || m_bIsStop)
            break;
    }
}

void TraverseDirectory::stop()
{
    m_bIsStop = true;
}

void TraverseDirectory::resetStop()
{
    // 由调用方（各遍历/搜索线程）在 run() 开头调用一次，
    // 不要在 traverseDirectory 内部复位，否则选中多个目录时“停止”只会停当前目录、下一个目录又会继续跑（P1-3）。
    m_bIsStop = false;
}

bool TraverseDirectory::isStopped()
{
    return m_bIsStop.load();
}
