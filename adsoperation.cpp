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
    // 同时禁止逗号、顿号和分号：标签以分隔符 join/split 在缓存入库、缓存查找、AI 上报、
    // 标签导入以及用户手工编辑中传递（见 P1-1）。五个分隔符（, ， 、 ; ；）都算边界，
    // 标签名里若含它们会被错误拆成多个幻影标签，故统一替换为下划线。
    static const QRegularExpression re("[/\\\\:*?\"<>|,，、;；]");
    s.replace(re, "_");
    s = s.trimmed();
    // 空名字保持为空，由 writeADSFile 直接跳过。
    // 以前这里兜底成 "tag"，导致"把标签清空"反而凭空写入一个 tag 标签。
    return s;
}

void ADSOperation::writeADSFile(const QString &filePath, const QString &streamName, const QString &text, bool isDeleteOldAds)
{
    if(isDeleteOldAds)
        deleteADSFiles(filePath);

    const QString sStreamName = sanitizeStreamName(streamName);
    if (sStreamName.isEmpty()) {
        // 空标签（含只有空格、或净化后为空）不写流：清空标签就应该是"没有标签"。
        qWarning() << "忽略空标签，未写入 ADS:" << filePath;
        return ;
    }

    QString sFullFileName = filePath + ":" + sStreamName;
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
        // FindFirstStreamW 返回形如 "::tag:$DATA"，去掉前导冒号后是 "tag:$DATA"，
        // 必须再剥掉 ":$DATA" 才是标签名，否则标签列会显示成 "tag:$DATA"。
        if(Utility::isADSNameValid(streamName))
            fileNames << Utility::adsNameWithoutType(streamName);
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

        // 去掉 ":$DATA" 后缀后再与标签名比较：流名是 "tag:$DATA"，标签名是 "tag"，
        // 不剥离则 OR/AND 两种逻辑都永远匹配不到（搜索会返回空结果）。
        adsName = Utility::adsNameWithoutType(adsName);

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
