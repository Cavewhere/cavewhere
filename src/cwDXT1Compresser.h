#ifndef CWDXT1COMPRESSER_H
#define CWDXT1COMPRESSER_H

//Qt includes
#include <QObject>
#include <QtConcurrent>
#include <QOpenGLFunctions_2_1>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwDXT1Compresser
{

public:
    class CompressedImage {
    public:
        CompressedImage() {}
        CompressedImage(const QByteArray& data, QSize size) :
            data(data),
            size(size)
        {}
        QByteArray data;
        QSize size;
    };

    cwDXT1Compresser();
    ~cwDXT1Compresser();

    QFuture<CompressedImage> compress(const QList<QImage>& images);
    QFuture<CompressedImage> openglCompression(const QList<QImage>& images, bool threaded = true);
    QFuture<CompressedImage> squishCompression(const QList<QImage>& images, bool threaded = true);

    QList<CompressedImage> results(QFuture<CompressedImage> future) const;

private:


    static QFuture<CompressedImage> squishCompressImageThreaded(QImage image,
                                                                int flags,
                                                                float* metric = 0);

    class OpenGLCompresser : public QOpenGLFunctions_2_1 {
    public:
        CompressedImage openglDxt1Compression(QImage image);
    };

};

#endif // CWDXT1COMPRESSER_H
