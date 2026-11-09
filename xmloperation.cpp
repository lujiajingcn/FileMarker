#include "xmloperation.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

const QString sTagRoot = QStringLiteral("Root");
const QString sTagLabel = QStringLiteral("label");
const QString sAttrId = QStringLiteral("id");
const QString sAttrName = QStringLiteral("name");

/**
 * 读"当前容器元素内部"的一层标签。
 * 进入时 xml 已停在一个容器的 start element 上；返回时已消费到该容器的 end element。
 *
 * 没有 id/name 的 <label> 视为旧格式的"容器"，透明展开——
 * 这样旧文件（子标签外多一层包装）和新文件（直接嵌套）都能读成同一棵树。
 */
QList<LabelInfo> readLabelsInContainer(QXmlStreamReader &xml)
{
    QList<LabelInfo> lstResult;
    while (xml.readNextStartElement()) {
        if (xml.name() != sTagLabel) {
            xml.skipCurrentElement();
            continue;
        }

        const QString sUuid = xml.attributes().value(sAttrId).toString();
        const QString sName = xml.attributes().value(sAttrName).toString();
        if (sUuid.isEmpty() && sName.isEmpty()) {
            lstResult += readLabelsInContainer(xml);   // 旧格式的包装层：把里面的标签提到同一层
            continue;
        }

        LabelInfo info;
        info.sUuid = sUuid;
        info.sName = sName;
        info.lstChild = readLabelsInContainer(xml);    // 没有子元素时立即返回
        lstResult << info;
    }
    return lstResult;
}

void writeLabelsTo(QXmlStreamWriter &xml, const QList<LabelInfo> &lstBlockInfo)
{
    for (const LabelInfo &info : lstBlockInfo) {
        xml.writeStartElement(sTagLabel);
        xml.writeAttribute(sAttrId, info.sUuid);
        xml.writeAttribute(sAttrName, info.sName);
        if (!info.lstChild.isEmpty())
            writeLabelsTo(xml, info.lstChild);         // 直接嵌套，不再写包装层
        xml.writeEndElement();
    }
}

}   // namespace

XmlOperation::XmlOperation()
{
}

QList<LabelInfo> XmlOperation::readLabelFromXmlFile(const char* sXml)
{
    QList<LabelInfo> lstLabelInfo;

    // QString::toStdString() 是 UTF-8，调用方传进来的就是 UTF-8 路径
    const QString sFilePath = QString::fromUtf8(sXml);
    QFile file(sFilePath);
    if (!file.exists())
        return lstLabelInfo;    // 首次运行：还没有标签库文件，属正常情况

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "标签库文件无法打开:" << sFilePath << file.errorString();
        return lstLabelInfo;
    }

    QXmlStreamReader xml(&file);
    QList<LabelInfo> lstParsed;
    bool bRootOk = false;
    if (xml.readNextStartElement()) {
        if (xml.name() == sTagRoot) {
            bRootOk = true;
            lstParsed = readLabelsInContainer(xml);
        } else {
            xml.raiseError(QStringLiteral("根元素不是 <Root>"));
        }
    }

    const bool bParseOk = bRootOk && !xml.hasError();
    const QString sError = xml.errorString();
    xml.clear();
    file.close();

    if (!bParseOk) {
        // 不能在无提示的情况下把标签库当成空库：留一份 .bad 备份，并写日志说明
        const QString sBackup = sFilePath + QStringLiteral(".bad-")
                                + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
        const bool bCopied = QFile::copy(sFilePath, sBackup);
        qCritical().noquote()
            << QStringLiteral("标签库 XML 解析失败，已按空库处理（原文件备份：%1）：%2 —— %3")
                   .arg(bCopied ? sBackup : QStringLiteral("备份失败"), sFilePath, sError);
        return lstLabelInfo;
    }

    return lstParsed;
}

bool XmlOperation::saveXml(const char* sXml, const QList<LabelInfo> &lstBlockInfo)
{
    const QString sFilePath = QString::fromUtf8(sXml);

    // QSaveFile：先写临时文件，commit() 时原子替换，写一半失败也不会毁掉旧文件
    QSaveFile file(sFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "标签库文件无法写入:" << sFilePath << file.errorString();
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    xml.writeStartDocument(QStringLiteral("1.0"));
    xml.writeStartElement(sTagRoot);
    writeLabelsTo(xml, lstBlockInfo);
    xml.writeEndElement();      // Root
    xml.writeEndDocument();

    if (xml.hasError()) {
        qWarning() << "标签库序列化失败:" << sFilePath;
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        qWarning() << "标签库保存失败:" << sFilePath << file.errorString();
        return false;
    }
    return true;
}
