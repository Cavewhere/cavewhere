//Our includes
#include "cwMappedQImage.h"

//Qt includes
#include <QDir>
#include <QUuid>

QImage cwMappedQImage::createDiskImage(const QString &path, const QImage &imageToCopy)
{
    if(imageToCopy.isNull()) {
        return imageToCopy;
    }

    return createDiskImage(path, imageToCopy.size(), [imageToCopy](QFile* file)
    {
        bool success = file->write(reinterpret_cast<const char*>(imageToCopy.bits()), imageToCopy.sizeInBytes());
        Q_ASSERT(success);
        Q_ASSERT(file->size() == imageToCopy.sizeInBytes());
    });
}

QImage cwMappedQImage::createDiskImage(const QString &path, const QSize &size)
{
    if(!size.isValid()) {
        return QImage();
    }

    return createDiskImage(path, size, [size](QFile* file)
    {
        const qint64 bytesPerPixel = 4;
        qint64 imageSizeBytes = static_cast<qint64>(size.width())
                * static_cast<qint64>(size.height())
                * bytesPerPixel;

        bool success = file->resize(imageSizeBytes);
        Q_ASSERT(success);
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

