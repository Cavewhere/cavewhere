/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPROJECTIMAGEPROVIDER_H
#define CWPROJECTIMAGEPROVIDER_H

//Qt includes
#include <QObject>
#include <QQuickImageProvider>
#include <QMutex>
#include <QDebug>
#include <QVector2D>
#include <QStringLiteral>

//Our includes
#include "cwImage.h"
#include "cwImageData.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwImageProvider : public QQuickImageProvider
{
    Q_OBJECT

public:
    cwImageProvider();
    virtual QImage requestImage(const QString &path, QSize *size, const QSize &requestedSize);

    // cwImageData originalMetadata(const cwImage& image) const;
    // QImage image(int id) const;
    QImage image(const QString& path) const;
    QImage image(const cwImageData& data) const;
    QVector2D scaleTexCoords(const cwImage &image) const;

    //data(id) is an old function used to load old image data, and shouldn't be used
    cwImageData data(int id, bool metaDataOnly = false) const;

    //New image data with full path
    cwImageData data(QString filename) const;

    static cwImageData createDxt1(QSize size, const QByteArray& uncompressData);

    static QString name() { return QLatin1String("sqlimagequery"); }
    static QByteArray dxt1GzExtension() { return QByteArrayLiteral("dxt1.gz"); }
    static QByteArray croppedReferenceExtension() { return QByteArrayLiteral("croppedReference"); }

    static QByteArray cropXKey() { return QByteArrayLiteral("x"); }
    static QByteArray cropYKey() { return QByteArrayLiteral("y"); }
    static QByteArray cropWidthKey() { return QByteArrayLiteral("width"); }
    static QByteArray cropHeightKey() { return QByteArrayLiteral("height"); }
    static QByteArray cropIdKey() { return QByteArrayLiteral("id"); }

    static QString imageUrl(QString relativePath);

public slots:
    void setProjectPath(QString projectPath);


private:
    static QString requestImageSQL() { return QLatin1String("SELECT type,width,height,dotsPerMeter,imageData from Images where id=?"); }
    static QString requestMetadataSQL() { return QLatin1String("SELECT type,width,height,dotsPerMeter from Images where id=?"); }
    QString ProjectPath;
    mutable QMutex ProjectPathMutex;

    static QAtomicInt ConnectionCounter;

    QString projectPath() const;
    QString imagePath(const QString& relativeImagePath) const;
};

#endif // CWPROJECTIMAGEPROVIDER_H

