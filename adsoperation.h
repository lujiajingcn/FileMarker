#ifndef ADSOPERATION_H
#define ADSOPERATION_H

#include <QObject>
class ADSOperation : public QObject
{
    Q_OBJECT
public:
    ADSOperation();

public :
    static void writeADSFile(const QString &filePath, const QString& streamName, const QString& text, bool isDeleteOldAds = false);
    static void deleteADSFiles(const QString &filePath);
    static QStringList listADSFileName(const QString &filePath);
};

#endif // ADSOPERATION_H
