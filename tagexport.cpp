#include "tagexport.h"

#include "adsoperation.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace TagExport {

QList<FileTags> collectFromDirs(const QStringList &qLDirs,
                                const std::function<bool(const QString &sFilePath)> &progress,
                                QString *pError)
{
    QList<FileTags> qLResult;
    for (const QString &sDir : qLDirs) {
        if (sDir.isEmpty())
            continue;
        const QDir dir(sDir);
        if (!dir.exists()) {
            if (pError != nullptr)
                *pError = QStringLiteral("目录不存在：%1").arg(sDir);
            continue;
        }

        const QString sRootPath = QDir::cleanPath(dir.absolutePath());
        QDirIterator it(sRootPath, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString sFilePath = QDir::cleanPath(it.next());
            if (progress && !progress(sFilePath))
                return qLResult;   // 用户取消：返回已收集的部分

            const QStringList qLTags = ADSOperation::listADSTagsInOrder(sFilePath);
            if (qLTags.isEmpty())
                continue;          // 没有标签的文件没有导出的必要

            FileTags record;
            record.sPath = sFilePath;
            record.sRel = QDir(sRootPath).relativeFilePath(sFilePath);
            record.qLTags = qLTags;
            qLResult.append(record);
        }
    }
    return qLResult;
}

bool writeJson(const QString &sFile, const QList<FileTags> &qLRecords,
               const QString &sRoot, QString *pError)
{
    QJsonArray qLFiles;
    for (const FileTags &record : qLRecords) {
        QJsonObject obj;
        obj.insert(QStringLiteral("path"), record.sPath);
        obj.insert(QStringLiteral("rel"), record.sRel);
        obj.insert(QStringLiteral("tags"), QJsonArray::fromStringList(record.qLTags));
        qLFiles.append(obj);
    }

    QJsonObject root;
    root.insert(QStringLiteral("app"), QStringLiteral("FileMarker"));
    root.insert(QStringLiteral("format"), 1);
    root.insert(QStringLiteral("exportedAt"),
                QDateTime::currentDateTime().toString(Qt::ISODate));
    root.insert(QStringLiteral("root"), QDir::cleanPath(sRoot));
    root.insert(QStringLiteral("fileCount"), qLRecords.size());
    root.insert(QStringLiteral("files"), qLFiles);

    QFile file(sFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (pError != nullptr)
            *pError = file.errorString();
        return false;
    }
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);
    const bool bOk = file.write(data) == data.size();
    file.close();
    if (!bOk && pError != nullptr)
        *pError = file.errorString();
    return bOk;
}

bool readJson(const QString &sFile, QList<FileTags> *pRecords,
              QString *pRoot, QString *pError)
{
    QFile file(sFile);
    if (!file.open(QIODevice::ReadOnly)) {
        if (pError != nullptr)
            *pError = file.errorString();
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (pError != nullptr)
            *pError = QStringLiteral("不是有效的 JSON：%1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("app")).toString() != QStringLiteral("FileMarker")) {
        if (pError != nullptr)
            *pError = QStringLiteral("不是 FileMarker 导出的标签清单");
        return false;
    }

    if (pRoot != nullptr)
        *pRoot = root.value(QStringLiteral("root")).toString();

    pRecords->clear();
    const QJsonArray qLFiles = root.value(QStringLiteral("files")).toArray();
    for (const QJsonValue &value : qLFiles) {
        const QJsonObject obj = value.toObject();
        FileTags record;
        record.sPath = obj.value(QStringLiteral("path")).toString();
        record.sRel = obj.value(QStringLiteral("rel")).toString();
        for (const QJsonValue &tag : obj.value(QStringLiteral("tags")).toArray())
            record.qLTags << tag.toString();
        if (record.sPath.isEmpty() || record.qLTags.isEmpty())
            continue;
        pRecords->append(record);
    }
    return true;
}

ApplyResult applyRecords(const QList<FileTags> &qLRecords, bool bOverwrite,
                         const QString &sFallbackRoot,
                         const std::function<bool(const QString &sFilePath)> &progress)
{
    ApplyResult result;
    for (const FileTags &record : qLRecords) {
        if (progress && !progress(record.sPath))
            break;

        // 优先按绝对路径；找不到就用"新的根目录 + 相对路径"再试一次（整个目录被搬走的情况）
        QString sTarget = record.sPath;
        if (!QFileInfo::exists(sTarget) && !record.sRel.isEmpty() && !sFallbackRoot.isEmpty()) {
            const QString sAlternative = QDir::cleanPath(QDir(sFallbackRoot).filePath(record.sRel));
            if (QFileInfo::exists(sAlternative))
                sTarget = sAlternative;
        }
        if (!QFileInfo::exists(sTarget)) {
            ++result.nSkipped;
            result.qLMissingPaths << record.sPath;
            continue;
        }

        const bool bOk = bOverwrite
                             ? ADSOperation::replaceTags(sTarget, record.qLTags)
                             : ADSOperation::appendTags(sTarget, record.qLTags);
        if (bOk) {
            ++result.nApplied;
        } else {
            ++result.nFailed;
            result.qLFailedPaths << record.sPath;
        }
    }
    return result;
}

}   // namespace TagExport
