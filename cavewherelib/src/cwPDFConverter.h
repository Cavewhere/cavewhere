#ifndef CWPDFCONVERTER_H
#define CWPDFCONVERTER_H

//Qt inludes
#include <QObject>
#include <QImage>
#include <QList>
#include <QtConcurrent>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwPDFConverter : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool isSupported READ isSupported CONSTANT)

public:
    explicit cwPDFConverter(QObject *parent = nullptr);

    QString source() const;
    void setSource(QString source);

    int resolution() const;
    void setResolution(int resolution);

    QString error() const;

    Q_INVOKABLE QFuture<QImage> convert();

    static bool isSupported();

signals:
    void sourceChanged();
    void resolutionChanged();
    void errorChanged();

private:
    QString Source; //!<
    int Resolution = 300; //!<
    QString ErrorMessage; //!<

    void setError(const QString& errorMessage);

};

/**
*
*/
inline QString cwPDFConverter::source() const {
    return Source;
}

/**
*
*/
inline int cwPDFConverter::resolution() const {
    return Resolution;
}

/**
*
*/
inline QString cwPDFConverter::error() const {
    return ErrorMessage;
}



#endif // CWPDFCONVERTER_H
