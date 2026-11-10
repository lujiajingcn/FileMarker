#include "utility.h"

#include <QUuid>
#include <QDateTime>
#include <QIODevice>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>
#include <QFile>

Utility::Utility()
{

}

QTextStream Utility::tsLogInfo;
QMutex Utility::mutex;

QString Utility::settingsFilePath()
{
    const QString sAppIni = g_sAppDir + QStringLiteral("/FileMarker.ini");

    // 先探测程序目录是否可写；Program Files 等只读目录回退到用户 AppData
    QFile testFile(g_sAppDir + QStringLiteral("/.fm_write_test"));
    if (testFile.open(QIODevice::WriteOnly)) {
        testFile.close();
        testFile.remove();
        return sAppIni;
    }

    const QString sDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!sDir.isEmpty()) {
        QDir().mkpath(sDir);
        return sDir + QStringLiteral("/FileMarker.ini");
    }
    return sAppIni;
}

void Utility::shutdownLog()
{
    QMutexLocker locker(&mutex);
    tsLogInfo.flush();
    if (tsLogInfo.device() != nullptr)
        tsLogInfo.setDevice(nullptr);
}

void Utility::myMessageHandler(QtMsgType mType, const QMessageLogContext&, const QString& sMsg)
{
    // libpng 对第三方图片 ICC 描述的告警（如 "iCCP: known incorrect sRGB profile"）
    // 只影响颜色配置解读，不影响显示，写进日志只会淹没真正的问题，直接丢弃。
    if (sMsg.startsWith(QLatin1String("libpng warning:")))
        return;

    QMutexLocker locker(&mutex);

    // 程序退出收尾阶段日志设备可能已经断开/关闭，此时直接丢弃，
    // 否则会触发 "QIODevice::write (QObject): device not open"。
    QIODevice *paintDevice = tsLogInfo.device();
    if (paintDevice == nullptr || !paintDevice->isOpen())
        return;

    QString sFormatCurTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    switch (mType) {
    case QtDebugMsg:
        tsLogInfo << QString("%1    Debug       %2").arg(sFormatCurTime).arg(sMsg) << '\n';
        tsLogInfo.flush();
        break;
    case QtInfoMsg:
        tsLogInfo << QString("%1    Info        %2").arg(sFormatCurTime).arg(sMsg) << '\n';
        tsLogInfo.flush();
        break;
    case QtWarningMsg:
        tsLogInfo << QString("%1    Warning     %2").arg(sFormatCurTime).arg(sMsg) << '\n';
        tsLogInfo.flush();
        break;
    case QtCriticalMsg:
        tsLogInfo << QString("%1    Critical    %2").arg(sFormatCurTime).arg(sMsg) << '\n';
        tsLogInfo.flush();
        break;
    case QtFatalMsg:
        tsLogInfo << QString("%1    Fatal       %2").arg(sFormatCurTime).arg(sMsg) << '\n';
        tsLogInfo.flush();
        abort();
    }
}

QString Utility::createUuid()
{
    return QUuid::createUuid().toString().remove("{").remove("}").remove("-");
}

QSet<QString> Utility::toSet(const QStringList &list)
{
    QSet<QString> result;
    for (const QString &sValue : list)
        result.insert(sValue);
    return result;
}

bool Utility::hasEqualElement(const QStringList &list1, const QStringList &list2)
{
    QSet<QString> set1 = toSet(list1);
    QSet<QString> set2 = toSet(list2);
    return !set1.intersect(set2).isEmpty();
}

bool Utility::isContains(const QStringList &list1, const QStringList &list2)
{
    QSet<QString> set1 = toSet(list1);
    QSet<QString> set2 = toSet(list2);
    return set1.contains(set2);
}

// 去掉 FindFirstStreamW 返回的流名后缀 ":$DATA"，得到真正的标签名。
// 例：FindFirstStreamW 返回 "::tag:$DATA"，调用方去掉前导冒号后为 "tag:$DATA"，
// 本函数返回 "tag"。缺少这一步会让标签在界面上显示成 "tag:$DATA"，
// 并且用 "tag" 查找文件时永远匹配不到。
QString Utility::adsNameWithoutType(QString sADSName)
{
    if (sADSName.endsWith(ADS_TAIL))
    {
        sADSName.chop(strlen(ADS_TAIL));
    }
    return sADSName;
}

// 拆分标签字符串：英文逗号、中文逗号、顿号、英文分号、中文分号都算，
// 方便中文输入法下直接敲"，"、"、"或"；"。这里逐字符扫描而不是用正则：
// 剪贴板等来源可能含未配对代理项（非法 UTF-16），QRegularExpression 遇到这种
// 主题会匹配失败，导致整串一个标签都拆不出来。
QStringList Utility::splitLabels(const QString &sLabels)
{
    QStringList labels;
    QString current;

    auto flushCurrent = [&]() {
        const QString sLabel = current.trimmed();
        if (!sLabel.isEmpty() && !labels.contains(sLabel))
            labels << sLabel;
        current.clear();
    };

    for (int i = 0; i < sLabels.size(); ++i) {
        const QChar c = sLabels.at(i);
        const bool isSeparator = (c == QLatin1Char(','))
                                 || (c == QLatin1Char(';'))
                                 || (c == QChar(0xFF0C))   // ，
                                 || (c == QChar(0xFF1B))   // ；
                                 || (c == QChar(0x3001));  // 、
        if (isSeparator)
            flushCurrent();
        else
            current += c;
    }
    flushCurrent();

    return labels;
}

QStringList Utility::splitLabelsPreservingExisting(const QString &sLabels,
                                                  const QStringList &existingTags)
{
    if (existingTags.isEmpty())
        return splitLabels(sLabels);

    // 完全相同（未修改）直接返回原列表，避免拆分历史标签
    if (sLabels == existingTags.join(QLatin1Char(',')))
        return existingTags;

    // 长的优先，避免 "a" 抢先匹配 "a，b" 的一部分
    QStringList ordered = existingTags;
    std::sort(ordered.begin(), ordered.end(), [](const QString &a, const QString &b) {
        if (a.size() != b.size())
            return a.size() > b.size();
        return a < b;
    });

    auto isSeparator = [](QChar c) {
        return c == QLatin1Char(',') || c == QLatin1Char(';')
               || c == QChar(0xFF0C) || c == QChar(0xFF1B) || c == QChar(0x3001);
    };

    QStringList labels;
    int i = 0;
    while (i < sLabels.size()) {
        while (i < sLabels.size() && (isSeparator(sLabels.at(i)) || sLabels.at(i).isSpace()))
            ++i;
        if (i >= sLabels.size())
            break;

        bool matched = false;
        for (const QString &sExisting : ordered) {
            if (sExisting.isEmpty() || sLabels.mid(i, sExisting.size()) != sExisting)
                continue;
            const int nAfter = i + sExisting.size();
            if (nAfter == sLabels.size()
                    || isSeparator(sLabels.at(nAfter))
                    || sLabels.at(nAfter).isSpace()) {
                if (!labels.contains(sExisting))
                    labels << sExisting;
                i = nAfter;
                matched = true;
                break;
            }
        }
        if (matched)
            continue;

        const int nStart = i;
        while (i < sLabels.size() && !isSeparator(sLabels.at(i)))
            ++i;
        const QString sToken = sLabels.mid(nStart, i - nStart).trimmed();
        if (!sToken.isEmpty() && !labels.contains(sToken))
            labels << sToken;
    }
    return labels;
}

// 参数改为按值传递。原签名是非 const 引用，函数内部调用 chop() 会修改调用者的字符串，
// 调用者可能不期望参数被修改。按值传递后函数内部可以自由修改副本。
bool Utility::isADSNameValid(QString sADSName)
{
    sADSName = adsNameWithoutType(sADSName);
    if (sADSName.isEmpty())
    {
        return false;
    }
    // Windows 流名大小写不敏感，系统标记可能是 zone.identifier 等变体，必须忽略大小写。
    if (QString::compare(sADSName, FILTER_TAG, Qt::CaseInsensitive) == 0)
    {
        return false;
    }
    // 归属清单本身不是标签
    if (QString::compare(sADSName, FM_TAG_MARKER, Qt::CaseInsensitive) == 0)
    {
        return false;
    }
    return true;
}
