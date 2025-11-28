#ifndef ADSOPERATION_H
#define ADSOPERATION_H

#include <QObject>
#include "common.h"

class ADSOperation : public QObject
{
    Q_OBJECT
public:
    ADSOperation();

public :
    static void writeADSFile(const QString &filePath, const QString& streamName, const QString& text, bool isDeleteOldAds = false);
    static void deleteADSFiles(const QString &filePath);
    static QStringList listADSFileName(const QString &filePath);
    static bool isHostFile(const QString &sFilePath, const QStringList &tags, labelLogic logic);
};

#endif // ADSOPERATION_H
