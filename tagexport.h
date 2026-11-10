/**
** 【标签备份】把文件上的标签导出成 JSON / 从 JSON 导回
**
** 为什么需要：标签存在 NTFS 数据流里，把文件拷到 U 盘（FAT32）、压成 zip、
** 传网盘/聊天工具时都会丢失；换机器重装也只有文件本身能带走。
** 导出的是一个"路径 → 标签列表"的清单，导入时按路径写回。
**
** 文件格式（UTF-8 JSON）：
** {
**   "app": "FileMarker", "format": 1, "exportedAt": "2026-11-10T12:30:00",
**   "root": "E:/testfiles_copy", "fileCount": 12,
**   "files": [ {"path": "...", "rel": "...", "tags": ["电子书","户外"]} ]
** }
**/

#ifndef TAGEXPORT_H
#define TAGEXPORT_H

#include <QString>
#include <QStringList>
#include <QList>

#include <functional>

namespace TagExport {

struct FileTags
{
    QString     sPath;      // 绝对路径
    QString     sRel;       // 相对导出根目录的路径（根目录移动后仍可尝试找回）
    QStringList qLTags;
};

/**
 * 扫描若干目录（含子目录），收集"有标签的文件"。
 * progress 每个文件回调一次（返回 false 表示用户取消）。被取消时返回已收集的部分。
 */
QList<FileTags> collectFromDirs(const QStringList &qLDirs,
                                const std::function<bool(const QString &sFilePath)> &progress,
                                QString *pError);

/** 写 JSON 清单 */
bool writeJson(const QString &sFile, const QList<FileTags> &qLRecords,
               const QString &sRoot, QString *pError);

/** 读 JSON 清单 */
bool readJson(const QString &sFile, QList<FileTags> *pRecords,
              QString *pRoot, QString *pError);

struct ApplyResult
{
    int         nApplied = 0;    // 成功写入
    int         nSkipped = 0;    // 文件不存在，跳过
    int         nFailed = 0;     // 写盘失败
    QStringList qLFailedPaths;
    QStringList qLMissingPaths;
};

/**
 * 把清单写回文件。
 * bOverwrite = true：用清单里的标签替换文件现有标签；false：追加（去重）。
 * sFallbackRoot 非空时：绝对路径找不到的文件，会尝试用它 + 相对路径定位（目录整体搬走后仍能恢复）。
 * progress 每处理一个文件回调一次（返回 false 表示用户取消）。
 */
ApplyResult applyRecords(const QList<FileTags> &qLRecords, bool bOverwrite,
                         const QString &sFallbackRoot,
                         const std::function<bool(const QString &sFilePath)> &progress);

}   // namespace TagExport

#endif // TAGEXPORT_H
