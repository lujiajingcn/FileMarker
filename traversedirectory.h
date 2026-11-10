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

    /**
     * 遍历指定目录并执行回调。
     * @return true  = 目录被完整遍历（结果可以安全地写入缓存）
     *         false = 中途被 stop() 或回调要求中断（结果不完整，不能写缓存）
     * 以前返回 void，调用方无法区分"扫完了"和"被停止了"，
     * 于是中断时仍把残缺结果写进缓存，之后按缓存搜索会永久漏文件。
     */
    static bool traverseDirectory(const QString& path, FileAction action, bool recursive = false);
    static void stop();
    static void resetStop();
    static bool isStopped();

private:
    // 使用原子变量，避免多线程并发读写导致未定义行为。
    static std::atomic<bool>    m_isStop;
};

#endif // TRAVERSEDIRECTORY_H
