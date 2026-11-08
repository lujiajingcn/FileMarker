#include "adsoperation.h"
#include <windows.h>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPair>
#include <QRegularExpression>
#include <algorithm>
#include <string>
#include "common.h"
#include "utility.h"

ADSOperation::ADSOperation()
{
}

// FindFirstStreamW / DeleteFile 这些 Win32 API 不认长路径，超过 MAX_PATH 需要 \\?\ 前缀；
// Qt 的 QFile 写入会自动处理长路径，所以只有直接调用 Win32 的地方需要转换。
static std::wstring win32APIPath(const QString &filePath)
{
    QString sNative = QDir::toNativeSeparators(filePath);
    if (sNative.startsWith(QStringLiteral("\\\\?\\")))
        return sNative.toStdWString();

    // 保守阈值：接近 MAX_PATH(260) 时就加前缀，避免长标签导致删除/枚举失败
    if (sNative.size() >= 200) {
        if (sNative.startsWith(QStringLiteral("\\\\")))
            sNative = QStringLiteral("\\\\?\\UNC\\") + sNative.mid(2);
        else
            sNative = QStringLiteral("\\\\?\\") + sNative;
    }
    return sNative.toStdWString();
}

// 标签操作只应改变 ADS，不应改变文件的“修改日期”。
// Windows 在写入/删除流时会更新文件时间，这里在操作前保存、操作后恢复。
namespace {
class FileTimeGuard
{
public:
    explicit FileTimeGuard(const QString &filePath)
        : m_hFile(INVALID_HANDLE_VALUE)
        , m_bTimeSaved(false)
    {
        m_hFile = ::CreateFileW(win32APIPath(filePath).c_str(),
                                FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (m_hFile != INVALID_HANDLE_VALUE)
            m_bTimeSaved = ::GetFileTime(m_hFile, &m_ftCreation, &m_ftAccess, &m_ftWrite) != FALSE;
    }

    ~FileTimeGuard()
    {
        if (m_hFile == INVALID_HANDLE_VALUE)
            return;
        if (m_bTimeSaved)
            ::SetFileTime(m_hFile, &m_ftCreation, &m_ftAccess, &m_ftWrite);
        ::CloseHandle(m_hFile);
    }

private:
    HANDLE m_hFile;
    FILETIME m_ftCreation;
    FILETIME m_ftAccess;
    FILETIME m_ftWrite;
    bool m_bTimeSaved;
};
} // namespace

// NTFS 流名不允许包含 : / \ * ? " < > |，且不能为空。
// 标签作为流名使用时需做净化，否则 QFile::open 会静默失败导致标签“看似加上实则没写”。
QString ADSOperation::sanitizeTagName(const QString &name)
{
    QString s = name;

    // 先去掉未配对的代理项（无效 UTF-16）。
    // 必须在正则替换之前做：QRegularExpression 遇到含孤立代理项的字符串会匹配失败，
    // 导致后面的非法字符替换全部失效，写流时才失败。
    {
        QString cleaned;
        cleaned.reserve(s.size());
        for (int i = 0; i < s.size(); ++i) {
            const QChar c = s.at(i);
            if (c.isHighSurrogate()) {
                if (i + 1 < s.size() && s.at(i + 1).isLowSurrogate()) {
                    cleaned += c;
                    cleaned += s.at(i + 1);
                    ++i;
                }
                // 孤立的高代理项直接丢弃
            } else if (!c.isLowSurrogate()) {
                cleaned += c;
            }
            // 孤立的低代理项直接丢弃
        }
        s = cleaned;
    }

    // 同时禁止逗号、顿号和分号：标签以分隔符 join/split 在缓存入库、缓存查找、AI 上报、
    // 标签导入以及用户手工编辑中传递（见 P1-1）。五个分隔符（, ， 、 ; ；）都算边界，
    // 标签名里若含它们会被错误拆成多个幻影标签，故统一替换为下划线。
    static const QRegularExpression re("[/\\\\:*?\"<>|,，、;；]");
    s.replace(re, "_");

    // 换行、制表符等控制字符在单行显示时会撑乱布局，统一替换为下划线
    static const QRegularExpression reControl("[\\x00-\\x1F\\x7F]");
    s.replace(reControl, "_");

    s = s.trimmed();
    // 空名字保持为空，由 writeADSFile 直接跳过。
    // 以前这里兜底成 "tag"，导致"把标签清空"反而凭空写入一个 tag 标签。
    if (s.isEmpty())
        return s;

    // NTFS 流名最长 255 个 UTF-16 字符，超长会导致写入失败，这里截断并提示
    if (s.size() > 255) {
        s.truncate(255);
        if (s.at(s.size() - 1).isHighSurrogate())
            s.chop(1);
        qWarning() << "标签过长，已截断为 255 个字符:" << name.left(32) << "...";
    }
    return s;
}

void ADSOperation::writeADSFile(const QString &filePath, const QString &streamName, const QString &text, bool isDeleteOldAds)
{
    if(isDeleteOldAds)
        deleteADSFiles(filePath);

    const QString sStreamName = sanitizeTagName(streamName);
    if (sStreamName.isEmpty()) {
        // 空标签（含只有空格、或净化后为空）不写流：清空标签就应该是"没有标签"。
        qWarning() << "忽略空标签，未写入 ADS:" << filePath;
        return ;
    }

    QString sFullFileName = filePath + ":" + sStreamName;
    QFile f(sFullFileName);
    // 刚删除同名流后立即重建，Windows 上偶尔会出现短暂的共享冲突；
    // 这里做几次短重试，避免标签偶发丢失。
    bool bOpened = false;
    for (int nAttempt = 0; nAttempt < 3 && !bOpened; ++nAttempt) {
        bOpened = f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        if (!bOpened) {
            f.close();
            ::Sleep(10);
        }
    }
    if (!bOpened) {
        qWarning() << "写入 ADS 失败:" << sFullFileName << f.errorString();
        return ;
    }

    f.write(text.toUtf8());
}

// 删除指定文件的所有关联的ads文件
void ADSOperation::deleteADSFiles(const QString &filePath)
{
    // 删除流会更新文件的“修改日期”，这里保存/恢复文件时间（对外只应表现为标签变化）。
    FileTimeGuard timeGuard(filePath);

    QStringList adsFileNames = listADSFileName(filePath);
    foreach(QString adsName, adsFileNames)
    {
        QString adsFilePath = filePath + ":" + adsName;
        // P2-23：检查 DeleteFile 返回值，失败时记录警告。
        if (!DeleteFileW(win32APIPath(adsFilePath).c_str())) {
            qWarning() << "删除 ADS 失败:" << adsFilePath << "GetLastError:" << GetLastError();
        }
    }
}

// 根据文件路径，找出该文件所关联的ads文件的文件名
QStringList ADSOperation::listADSFileName(const QString &filePath)
{
    QStringList fileNames;
    WIN32_FIND_STREAM_DATA findData;

    HANDLE hFind = FindFirstStreamW(win32APIPath(filePath).c_str(), FindStreamInfoStandard, &findData, 0);
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

// 按用户录入的顺序列出标签。
// NTFS 枚举流时按流名排序（FindFirstStreamW 返回的就是流名字母序），无法反映录入顺序，
// 所以 replaceTags()/appendTags() 把顺序号写进每个标签流的内容里，这里读出来还原。
// 没有顺序号的历史标签（旧数据内容为空）排在末尾，彼此之间保持流名顺序。
QStringList ADSOperation::listADSTagsInOrder(const QString &filePath)
{
    const QStringList names = listADSFileName(filePath);
    if (names.size() < 2)
        return names;

    QVector<QPair<int, QString>> orderedTags;
    QStringList tagsWithoutOrder;
    foreach (const QString &sName, names)
    {
        int nOrder = -1;
        QFile f(filePath + ":" + sName);
        if (f.open(QIODevice::ReadOnly)) {
            bool bOk = false;
            nOrder = QString::fromUtf8(f.readAll()).trimmed().toInt(&bOk);
            if (!bOk)
                nOrder = -1;
            f.close();
        }

        if (nOrder >= 0)
            orderedTags.append(qMakePair(nOrder, sName));
        else
            tagsWithoutOrder << sName;
    }

    std::stable_sort(orderedTags.begin(), orderedTags.end(),
                     [](const QPair<int, QString> &a, const QPair<int, QString> &b) {
                         if (a.first != b.first)
                             return a.first < b.first;
                         return a.second < b.second;
                     });

    QStringList result;
    for (const QPair<int, QString> &item : orderedTags)
        result << item.second;
    result << tagsWithoutOrder;
    return result;
}

// 用给定顺序重写文件的全部标签：先清空，再按顺序写入。
// 顺序号写进流内容（标签本身不需要内容），供 listADSTagsInOrder() 还原顺序。
void ADSOperation::replaceTags(const QString &filePath, const QStringList &tags)
{
    // 最外层保护：内部 deleteADSFiles 也会保护一次，但写流同样会改时间，
    // 这里在整个“先删后写”结束后统一恢复到操作前的时间。
    FileTimeGuard timeGuard(filePath);

    deleteADSFiles(filePath);

    for (int i = 0; i < tags.count(); ++i)
        writeADSFile(filePath, tags.at(i), QString::number(i));
}

// 在已有标签之后追加：保留已有标签的顺序，新标签按传入顺序追加，重复的跳过。
void ADSOperation::appendTags(const QString &filePath, const QStringList &tags)
{
    QStringList merged = listADSTagsInOrder(filePath);
    foreach (const QString &sTag, tags)
    {
        const QString sTrim = sTag.trimmed();
        if (sTrim.isEmpty() || merged.contains(sTrim))
            continue;
        merged << sTrim;
    }
    replaceTags(filePath, merged);
}

// 根据标签和标签的逻辑关系，判断给定的文件是否是目标文件
bool ADSOperation::isHostFile(const QString &sFilePath, const QStringList &tags, labelLogic logic)
{
    // 没有指定标签时不应匹配任何文件：
    // 空集合是任意集合的子集，会让下面的 AND 判断误判为命中。
    if (tags.isEmpty())
        return false;

    WIN32_FIND_STREAM_DATA findStreamData;
    HANDLE hFind = FindFirstStreamW(win32APIPath(sFilePath).c_str(), FindStreamInfoStandard, &findStreamData, 0);
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
