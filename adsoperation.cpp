#include "adsoperation.h"
#include <windows.h>
#include <QDebug>
#include <QDir>
#include <QRegularExpression>
#include "common.h"
#include "utility.h"

ADSOperation::ADSOperation()
{
}

// NTFS 流名不允许包含 : / \ * ? " < > |，且不能为空。
// 标签作为流名使用时需做净化，否则 QFile::open 会静默失败导致标签“看似加上实则没写”。
static QString sanitizeStreamName(const QString &name)
{
    QString s = name;
    // 同时禁止逗号：标签以逗号 join/split 在缓存入库、缓存查找、AI 上报、标签导入中传递，
    // 含逗号的标签名会被错误拆成多个幻影标签（见 P1-1）。
    static const QRegularExpression re("[/\\\\:*?\"<>|,]");
    s.replace(re, "_");
    s = s.trimmed();
    if (s.isEmpty())
        s = "tag";
    return s;
}

void ADSOperation::writeADSFile(const QString &filePath, const QString &streamName, const QString &text, bool isDeleteOldAds)
{
    if(isDeleteOldAds)
        deleteADSFiles(filePath);

    QString sFullFileName = filePath + ":" + sanitizeStreamName(streamName);
    QFile f(sFullFileName);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "写入 ADS 失败:" << sFullFileName;
        return ;
    }

    f.write(text.toUtf8());
}

// 删除指定文件的所有关联的ads文件
void ADSOperation::deleteADSFiles(const QString &filePath)
{
    QStringList adsFileNames = listADSFileName(filePath);
    foreach(QString adsName, adsFileNames)
    {
        QString adsFilePath = filePath + ":" + adsName;
        // P2-23：检查 DeleteFile 返回值，失败时记录警告。
        if (!DeleteFile(adsFilePath.toStdWString().c_str())) {
            qWarning() << "删除 ADS 失败:" << adsFilePath << "GetLastError:" << GetLastError();
        }
    }
}

// 根据文件路径，找出该文件所关联的ads文件的文件名
QStringList ADSOperation::listADSFileName(const QString &filePath)
{
    QStringList fileNames;
    WIN32_FIND_STREAM_DATA findData;

    HANDLE hFind = FindFirstStreamW(filePath.toStdWString().c_str(), FindStreamInfoStandard, &findData, 0);
    if (hFind == INVALID_HANDLE_VALUE)
        return fileNames;

    do {
        QString streamName = QString::fromWCharArray(findData.cStreamName);
        streamName = streamName.mid(1); // Remove the leading ":"
        if(Utility::isADSNameValid(streamName))
            fileNames << streamName;
    } while (FindNextStreamW(hFind, &findData));

    FindClose(hFind);
    return fileNames;
}

// 根据标签和标签的逻辑关系，判断给定的文件是否是目标文件
bool ADSOperation::isHostFile(const QString &sFilePath, const QStringList &tags, labelLogic logic)
{
    WIN32_FIND_STREAM_DATA findStreamData;
    HANDLE hFind = FindFirstStreamW(sFilePath.toStdWString().c_str(), FindStreamInfoStandard, &findStreamData, 0);
    if (hFind == INVALID_HANDLE_VALUE)
        return false;

    bool bResult = false;
    QStringList adsNames;
    do {
        QString adsName = QString::fromWCharArray(findStreamData.cStreamName + 1);
        if(!Utility::isADSNameValid(adsName))
            continue;

        if(logic == labelLogic::OR) {
            if(tags.contains(adsName)) {
                bResult = true;
                break;
            }
        } else if(logic == labelLogic::AND) {
            adsNames << adsName;
        }
    } while (FindNextStreamW(hFind, &findStreamData));

    // 无论 OR 提前命中还是正常遍历结束，都必须释放流枚举句柄，否则每次命中都会泄漏一个句柄（P1-2）。
    FindClose(hFind);

    if (!bResult && logic == labelLogic::AND && adsNames.count() > 0 && Utility::isContains(adsNames, tags))
        bResult = true;

    return bResult;
}
