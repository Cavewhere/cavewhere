//Our includes
#include "cwMappedQImage.h"

//Qt includes
#include <QDir>
#include <QUuid>

//Std includes
#include <stdexcept>

QImage cwMappedQImage::createDiskImage(const QString &path, const QImage &imageToCopy)
{
    if(imageToCopy.isNull()) {
        return imageToCopy;
    }

    return createDiskImage(path,
                           imageToCopy.size(),
                           imageToCopy.colorSpace(),
                           imageToCopy.format(),
                           [imageToCopy](QFile* file)
    {
        file->write(reinterpret_cast<const char*>(imageToCopy.bits()), imageToCopy.sizeInBytes());
        checkForFileErrors(file);
        Q_ASSERT(file->size() == imageToCopy.sizeInBytes());
    });
}

QImage cwMappedQImage::createDiskImage(const QString &path, const QSize &size)
{
    if(!size.isValid()) {
        return QImage();
    }

    return createDiskImage(path,
                           size,
                           QColorSpace(),
                           QImage::Format_ARGB32,
                           [size](QFile* file)
    {
        qint64 imageSizeBytes = requiredSizeInBytes(size, QImage::Format_ARGB32);
        file->resize(imageSizeBytes);
        checkForFileErrors(file);
    });

}

QImage cwMappedQImage::createDiskImageWithTempFile(const QString &templateStr, const QImage &imageToCopy)
{
    return createDiskImage(tempFilename(templateStr), imageToCopy);
}

QImage cwMappedQImage::createDiskImageWithTempFile(const QString &templateStr, const QSize &size)
{
    return createDiskImage(tempFilename(templateStr), size);
}

qint64 cwMappedQImage::requiredSizeInBytes(QSize size, QImage::Format format)
{
    qint64 bytesPerPixel;

    switch(format) {
    case QImage::Format_ARGB32:
        bytesPerPixel = 4;
        break;
    default:
        return -1;
        break;
    }

    if(!size.isValid()) {
        return 0;
    }

    qint64 imageSizeBytes = static_cast<qint64>(size.width())
            * static_cast<qint64>(size.height())
            * bytesPerPixel;
    return imageSizeBytes;
}

QString cwMappedQImage::tempFilename(const QString &templateStr)
{
    auto randomString = []() {
        auto id = QUuid::createUuid();
        return id.toString(QUuid::Id128).left(12);
    };

    //Find a unique filename
    QString filename;
    do {
        filename = QString("%1/%2-%3.qimage").arg(QDir::tempPath()).arg(templateStr).arg(randomString());
    } while(QFile::exists(filename));
    return filename;
}

void cwMappedQImage::checkForFileErrors(QFile *file)
{
    if(file->error() != QFileDevice::NoError) {
        throw(std::runtime_error(QString("Image mapping had the following error: \"%1\"").arg(file->errorString()).toStdString()));
    }
}

