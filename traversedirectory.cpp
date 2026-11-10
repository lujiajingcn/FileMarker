#include "traversedirectory.h"

std::atomic<bool> TraverseDirectory::m_isStop{false};

TraverseDirectory::TraverseDirectory()
{
}

bool TraverseDirectory::traverseDirectory(const QString& path, FileAction action, bool recursive)
{
    QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, flags);
    while (it.hasNext())
    {
        it.next();
        bool shouldContinue = action(it.fileInfo());
        if (!shouldContinue || m_isStop)
            return false;   // 中断 -> 结果不完整，调用方不得写缓存
    }
    return true;            // 完整遍历
}

void TraverseDirectory::stop()
{
    m_isStop = true;
}

void TraverseDirectory::resetStop()
{
    // 由调用方（各遍历/搜索线程）在 run() 开头调用一次，
    // 不要在 traverseDirectory 内部复位，否则选中多个目录时“停止”只会停当前目录、下一个目录又会继续跑。
    m_isStop = false;
}

bool TraverseDirectory::isStopped()
{
    return m_isStop.load();
}
