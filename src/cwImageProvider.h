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

//Our includes
#include "cwImage.h"
#include "cwImageData.h"
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwImageProvider : public QObject, public QQuickImageProvider
{
    Q_OBJECT

public:
    static const QString Name;
    static const QByteArray Dxt1GzExtension;
    static const QByteArray CroppedreferenceExtension;

    static const QByteArray CropXKey;
    static const QByteArray CropYKey;
    static const QByteArray CropWidthKey;
    static const QByteArray CropHeightKey;
    static const QByteArray CropIdKey;

    cwImageProvider();
    virtual QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);

    cwImageData originalMetadata(const cwImage& image) const;
    cwImageData data(int id, bool metaDataOnly = false) const;
    QImage image(int id) const;
    QImage image(const cwImageData& data) const;
    QVector2D scaleTexCoords(const cwImage &image) const;

    static cwImageData createDxt1(QSize size, const QByteArray& uncompressData);

public slots:
    void setProjectPath(QString projectPath);


private:
    static const QString RequestImageSQL;
    static const QString RequestMetadataSQL;
    QString ProjectPath;
    QMutex ProjectPathMutex;

    static QAtomicInt ConnectionCounter;

    QString projectPath() const;
};

#endif // CWPROJECTIMAGEPROVIDER_H

