/**
** 【遍历文件夹】
**
** 遍历指定的文件夹，并执行传入的回调函数
**/

#ifndef TRAVERSEDIRECTORY_H
#define TRAVERSEDIRECTORY_H

#include <functional>
#include <QDirIterator>
#include <atomic>

using FileAction = std::function<bool(const QFileInfo&)>;

class TraverseDirectory
{
public:
    TraverseDirectory();

    static void traverseDirectory(const QString& path, FileAction action, bool recursive = false);
    static void stop();
    static void resetStop();
    static bool isStopped();

private:
    // 使用原子变量，避免多线程并发读写导致未定义行为。
    static std::atomic<bool>    m_bIsStop;
};

#endif // TRAVERSEDIRECTORY_H
