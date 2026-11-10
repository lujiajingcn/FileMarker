/**
** FormPageFiles 内部共用的小工具（不属于 UI，也不进 moc）
**
** 拆文件之前这些小工具都堆在 formpagefiles.cpp 的匿名命名空间里，
** 导航、标签、菜单三个实现文件都要用，所以提到这里共享。
**/

#ifndef FORMPAGEFILESUTIL_H
#define FORMPAGEFILESUTIL_H

#include <QDir>
#include <QString>

namespace FmPageUtil {

// 地址栏下拉里保留的历史目录条数
const int RECENT_DIR_LIMIT = 10;

/**
 * 虚拟根节点（"我的电脑"）的显示名。
 * QFileSystemModel::myComputer() 返回的是英文 "My Computer"，和界面上的名称不同，
 * 这里统一用界面名，并且两种写法都认。
 */
inline QString myComputerName()
{
    return QStringLiteral("我的电脑");
}

inline bool isMyComputerName(const QString &sName)
{
    const QString s = sName.trimmed();
    return s.isEmpty()
           || s.compare(myComputerName(), Qt::CaseInsensitive) == 0
           || s.compare(QStringLiteral("My Computer"), Qt::CaseInsensitive) == 0;
}

/** Windows 下同一路径可能以不同分隔符/盘符大小写出现（模型 vs QDir），统一比较 */
inline bool isSameDirPath(const QString &sLeft, const QString &sRight)
{
    if (sLeft.isEmpty() || sRight.isEmpty())
        return sLeft == sRight;
    return QDir::cleanPath(sLeft).compare(QDir::cleanPath(sRight), Qt::CaseInsensitive) == 0;
}

}   // namespace FmPageUtil

#endif // FORMPAGEFILESUTIL_H
