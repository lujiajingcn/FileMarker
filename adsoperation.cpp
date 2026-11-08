#include "adsoperation.h"
#include <windows.h>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QPair>
#include <QRegularExpression>
#include <QSet>
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

// 读取命名流开头的少量文本，用于判断它是不是本程序的标签流（内容为顺序号或旧版空内容）。
// 只读前 maxBytes+1 字节：第三方 ADS（SummaryInformation、缩略图缓存等）可能很大，
// 绝不能整块 readAll，更不能把它们当标签删除/重写。
static bool readStreamHeader(const QString &filePath, const QString &streamName,
                             QString &sContent, int nMaxBytes = 16)
{
    QFile f(filePath + ":" + streamName);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    const QByteArray data = f.read(nMaxBytes + 1);
    f.close();
    if (data.size() > nMaxBytes)
        return false;

    sContent = QString::fromUtf8(data).trimmed();
    return true;
}

// 只把“内容为空（旧版标签）或纯数字（新版顺序号）”的命名流当作标签。
// 这样文件上的第三方 ADS 会被保留，不会在编辑标签时被误删或改写成数字。
static bool isFileMarkerTagStream(const QString &filePath, const QString &streamName)
{
    if (!Utility::isADSNameValid(streamName))
        return false;

    QString sContent;
    if (!readStreamHeader(filePath, streamName, sContent, 16))
        return false;

    if (sContent.isEmpty())
        return true;   // 兼容旧数据：顺序号功能之前写入的标签内容为空

    static const QRegularExpression reOrder(QStringLiteral("^\\d{1,9}$"));
    return reOrder.match(sContent).hasMatch();
}

// 写入 ADS 的实际实现，返回是否成功，供 replaceTags 做失败回滚。
static bool writeADSFileInternal(const QString &filePath, const QString &streamName,
                                 const QString &text)
{
    const QString sStreamName = ADSOperation::sanitizeTagName(streamName);
    if (sStreamName.isEmpty())
        return false;

    QString sFullFileName = filePath + ":" + sStreamName;
    QFile f(sFullFileName);
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
        return false;
    }

    const QByteArray data = text.toUtf8();
    const qint64 nWritten = f.write(data);
    f.close();
    if (nWritten != data.size()) {
        qWarning() << "写入 ADS 不完整:" << sFullFileName;
        return false;
    }
    return true;
}

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

    // Windows 会静默去掉流名末尾的点（"a." 实际会变成 "a"），导致界面上的标签
    // 和磁盘上的名字不一致；这里主动去掉，避免“看起来写成功、名字却变了”。
    while (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    s = s.trimmed();

    // 空名字保持为空，由 writeADSFile 直接跳过。
    // 以前这里兜底成 "tag"，导致"把标签清空"反而凭空写入一个 tag 标签。
    if (s.isEmpty())
        return s;

    // Zone.Identifier 是 Windows 的 Mark-of-the-Web，不能当标签写：写了会被枚举过滤、
    // 看不到也删不掉，还会破坏下载来源标记。
    if (QString::compare(s, FILTER_TAG, Qt::CaseInsensitive) == 0) {
        qWarning() << "拒绝把系统流名当标签:" << name;
        return QString();
    }

    // 纯分隔符/非法字符（例如 "," "???"）净化后会变成 "_"，不应凭空造出一个 "_" 标签。
    {
        static const QRegularExpression reBad(QStringLiteral("[/\\\\:*?\"<>|,，、;；\\x00-\\x1F\\x7F]"));
        if (s.count(QLatin1Char('_')) == s.size() && reBad.match(name).hasMatch())
            return QString();
    }

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
        deleteADSFiles(filePath);   // 只删除标签流，保留第三方 ADS

    if (!writeADSFileInternal(filePath, streamName, text)) {
        if (sanitizeTagName(streamName).isEmpty())
            qWarning() << "忽略空/保留标签，未写入 ADS:" << filePath;
    }
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

// 列出该文件上属于 FileMarker 的标签流（不包含第三方 ADS、Zone.Identifier）。
// 判定规则见 isFileMarkerTagStream：内容为空或纯数字顺序号。
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
        if(!Utility::isADSNameValid(streamName))
            continue;

        const QString sTagName = Utility::adsNameWithoutType(streamName);
        // 只接受本程序写入的标签流；第三方命名流保持不变，也不显示成标签。
        if (!isFileMarkerTagStream(filePath, sTagName))
            continue;

        fileNames << sTagName;
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
        QString sContent;
        // 只读顺序号需要的前 16 字节，避免读到第三方大流
        if (readStreamHeader(filePath, sName, sContent, 16) && !sContent.isEmpty()) {
            bool bOk = false;
            const int nValue = sContent.toInt(&bOk);
            if (bOk && nValue >= 0)
                nOrder = nValue;
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
// 写入失败时尽量把操作前的标签恢复回去，避免“删掉了旧的、新的没写进去”。
void ADSOperation::replaceTags(const QString &filePath, const QStringList &tags)
{
    // 最外层保护：内部 deleteADSFiles 也会保护一次，但写流同样会改时间，
    // 这里在整个“先删后写”结束后统一恢复到操作前的时间。
    FileTimeGuard timeGuard(filePath);

    // 统一净化、去空、大小写不敏感去重（Windows 流名不区分大小写）
    QStringList newTags;
    QSet<QString> setNewLower;
    foreach (const QString &sTag, tags) {
        const QString sSanitized = sanitizeTagName(sTag);
        if (sSanitized.isEmpty())
            continue;
        const QString sLower = sSanitized.toLower();
        if (setNewLower.contains(sLower))
            continue;
        setNewLower.insert(sLower);
        newTags << sSanitized;
    }

    // 备份旧标签及其顺序内容，写入失败时回滚
    const QStringList oldTags = listADSTagsInOrder(filePath);
    QMap<QString, QString> mapOldContents;
    foreach (const QString &sOld, oldTags) {
        QString sContent;
        if (!readStreamHeader(filePath, sOld, sContent, 16))
            sContent.clear();
        mapOldContents.insert(sOld, sContent);
    }

    // 只删除标签流；文件上的第三方 ADS 会保留
    deleteADSFiles(filePath);

    bool bOk = true;
    for (int i = 0; i < newTags.count(); ++i) {
        if (!writeADSFileInternal(filePath, newTags.at(i), QString::number(i))) {
            bOk = false;
            break;
        }
    }

    if (!bOk) {
        qWarning() << "标签写入失败，正在回滚:" << filePath;
        deleteADSFiles(filePath);   // 清掉写了一半的新标签（仍保留第三方 ADS）
        for (auto it = mapOldContents.constBegin(); it != mapOldContents.constEnd(); ++it)
            writeADSFileInternal(filePath, it.key(), it.value());
        qWarning() << "已尝试恢复操作前的标签:" << filePath;
    }
}

// 在已有标签之后追加：保留已有标签的顺序，新标签按传入顺序追加，重复的跳过。
// 追加只写缺失的流，不删除/重写已有标签，也不会碰第三方 ADS；
// 因此中途失败最多是少加几个新标签，不会丢失旧标签。
void ADSOperation::appendTags(const QString &filePath, const QStringList &tags)
{
    FileTimeGuard timeGuard(filePath);

    const QStringList existing = listADSTagsInOrder(filePath);
    QSet<QString> setExistingLower;
    foreach (const QString &sName, existing)
        setExistingLower.insert(sName.toLower());

    QStringList toAdd;
    foreach (const QString &sTag, tags) {
        const QString sTrimmed = sTag.trimmed();
        if (sTrimmed.isEmpty())
            continue;
        // 旧数据里的标签名可能合法包含 "，、；"；如果库里还是原名，先按原名判断是否已存在，
        // 避免净化成下划线后又给同一个文件追加一个“新”标签。
        if (setExistingLower.contains(sTrimmed.toLower()))
            continue;

        const QString sSanitized = sanitizeTagName(sTag);
        if (sSanitized.isEmpty())
            continue;
        const QString sLower = sSanitized.toLower();
        if (setExistingLower.contains(sLower))
            continue;
        setExistingLower.insert(sLower);
        toAdd << sSanitized;
    }

    // 兼容旧标签：它们的内容为空，会排在所有带顺序号的标签之后。
    // 如果直接给新标签写 existing.size()，新标签会排到旧标签前面（看起来不是“追加”）。
    // 这里先给旧标签补上顺序号（只改内容，不删除任何流），再把新标签接在最后。
    QStringList legacyTags;
    int nMaxOrder = -1;
    foreach (const QString &sName, existing) {
        QString sContent;
        int nOrder = -1;
        if (readStreamHeader(filePath, sName, sContent, 16) && !sContent.isEmpty()) {
            bool bOk = false;
            const int nValue = sContent.toInt(&bOk);
            if (bOk && nValue >= 0)
                nOrder = nValue;
        }
        if (nOrder >= 0)
            nMaxOrder = qMax(nMaxOrder, nOrder);
        else
            legacyTags << sName;
    }
    foreach (const QString &sLegacy, legacyTags) {
        // 写入失败则不要占用这个序号（旧标签本身仍在，只是继续排在末尾）
        if (!writeADSFileInternal(filePath, sLegacy, QString::number(++nMaxOrder)))
            --nMaxOrder;
    }

    int nOrder = nMaxOrder + 1;
    foreach (const QString &sTag, toAdd) {
        if (!writeADSFileInternal(filePath, sTag, QString::number(nOrder))) {
            qWarning() << "追加标签失败（已有标签未受影响）:" << filePath << sTag;
            continue;
        }
        ++nOrder;
    }
}

// 根据标签和标签的逻辑关系，判断给定的文件是否是目标文件
bool ADSOperation::isHostFile(const QString &sFilePath, const QStringList &tags, labelLogic logic)
{
    // 没有指定标签时不应匹配任何文件：
    // 空集合是任意集合的子集，会让下面的 AND 判断误判为命中。
    if (tags.isEmpty())
        return false;

    // 复用标签流枚举：只比较 FileMarker 自己的标签，第三方 ADS 不参与搜索。
    const QStringList adsNames = listADSFileName(sFilePath);
    if (adsNames.isEmpty())
        return false;

    if (logic == labelLogic::OR) {
        foreach (const QString &sTag, tags) {
            if (adsNames.contains(sTag))
                return true;
        }
        return false;
    }

    // AND：条件标签必须全部包含在文件的标签集合中
    return Utility::isContains(adsNames, tags);
}
