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
#include <functional>

//Our includes
#include "cwImage.h"
#include "cwImageData.h"
#include "cwGlobals.h"
#include "cwDiskCacher.h"

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

    //image data with full path
    cwImageData data(QString filename) const;

    QString absoluteImagePath(const cwImage& image) const;

    static cwImageData createDxt1(QSize size, const QByteArray& uncompressData);

    static QString name() { return QLatin1String("sqlimagequery"); }
    static QByteArray dxt1GzExtension() { return QByteArrayLiteral("dxt1.gz"); }
    static QByteArray croppedReferenceExtension() { return QByteArrayLiteral("croppedReference"); }
    static QString imageCacheExtension() { return QStringLiteral("cw_img_cache"); }

    static QByteArray cropXKey() { return QByteArrayLiteral("x"); }
    static QByteArray cropYKey() { return QByteArrayLiteral("y"); }
    static QByteArray cropWidthKey() { return QByteArrayLiteral("width"); }
    static QByteArray cropHeightKey() { return QByteArrayLiteral("height"); }
    static QByteArray cropIdKey() { return QByteArrayLiteral("id"); }

    static QString imageUrl(QString relativePath);

    static quint64 imageHash(const QImage& image);
    static quint64 fileHash(const QString& path);
    static quint64 toHash(const QByteArray& data);

    static cwDiskCacher::Key addToImageCache(
        const QString& projectPath,
        const QImage& image,
        const cwDiskCacher::Key& key);
    static cwDiskCacher::Key addToImageCache(
        const QDir& projectDir,
        const QImage& image,
        const cwDiskCacher::Key& key);

    static cwDiskCacher::Key imageCacheKey(const QString& pathToImage,
                                           const QString& keyPrefix,
                                           quint64 parentImageHash);

    static QImage requestScaledImageFromCache(
        const QSize& requestedSize,
        QSize* size,
        const std::function<QImage()>& loadOriginalImage,
        const cwDiskCacher::Key& cacheKey,
        const QDir& cacheDir);

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
