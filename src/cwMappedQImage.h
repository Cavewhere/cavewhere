#ifndef CWMAPPEDQIMAGE_H
#define CWMAPPEDQIMAGE_H

//Qt includes
#include <QString>
#include <QImage>
#include <QSize>
#include <QFile>
#include <QDebug>

//Our includes
#include "cwGlobals.h"

class CAVEWHERE_LIB_EXPORT cwMappedQImage
{
public:
    cwMappedQImage() = delete;

    static QImage createDiskImage(const QString& path, const QImage& imageToCopy);
    static QImage createDiskImage(const QString& path, const QSize& size);
    static QImage createDiskImageWithTempFile(const QString& templateStr, const QImage& imageToCopy);
    static QImage createDiskImageWithTempFile(const QString& templateStr, const QSize& size);

private:
    template <typename FilePopulateFunc>
    static QImage createDiskImage(const QString& path, const QSize& size, FilePopulateFunc populate) {
        Q_ASSERT(!QFile::exists(path));

        auto file = new QFile(path);
        bool success = file->open(QIODevice::ReadWrite);
        Q_ASSERT(success);

        populate(file);

        auto fileMapPtr = file->map(0, file->size());

        //Create a QImage that uses memory off of disk rather than in memory
        QImage image(fileMapPtr, size.width(), size.height(), QImage::Format_ARGB32,
                     [](void* cleanup)
        {
            auto file = reinterpret_cast<QFile*>(cleanup);
            auto filename = file->fileName();
            delete file;
            bool success = QFile::remove(filename);
            Q_ASSERT(success);
        },
        file);

        return image;
    }

    static QString tempFilename(const QString& templateStr);
};

#endif // CWMAPPEDQIMAGE_H
