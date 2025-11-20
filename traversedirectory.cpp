#include "traversedirectory.h"

bool TraverseDirectory::m_bIsStop = false;

TraverseDirectory::TraverseDirectory()
{
//    m_bIsStop = false;
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
        {
            break;
        }
    }
}

void TraverseDirectory::stop()
{
    m_bIsStop = true;
}
