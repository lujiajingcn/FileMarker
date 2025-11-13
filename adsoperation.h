#ifndef ADSOPERATION_H
#define ADSOPERATION_H

#include <QObject>
class ADSOperation : public QObject
{
    Q_OBJECT
public:
    ADSOperation();

public :
    void setStop(bool bIsStop);
    static void readADS(const QString &sFilePath, const QString& sStreamName);
    static void writeADS(const QString &sFilePath, const QString& sStreamName, const QString& sData, bool isDeleteOldAds = false);
    static void deleteADS(const QString &sFilePath);
    static QStringList listADS(const QString &sFilePath);
    static QStringList listADSFileName(const QString &sFilePath);
};

#endif // ADSOPERATION_H
