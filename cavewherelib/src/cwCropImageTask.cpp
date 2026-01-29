/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwCropImageTask.h"
#include "cwConcurrent.h"
#include "cwDiskCacher.h"
#include "cwDebug.h"

//Async future
#include <asyncfuture.h>

//Qt includes
#include <QColorSpace>
#include <cmath>

//Std includes
#include <algorithm>

cwCropImageTask::cwCropImageTask(QObject* parent) :
    QObject(parent) {

}

/**
  Sets the original image that'll be cropped
  */
void cwCropImageTask::setOriginal(cwImage image) {
    Original = image;
}

/**
  Sets the cropping region.  The region should be in normalized image coordinates, ie from
  [0.0 to 1.0]. If the coordinates are out of bounds, this will clamp them. to 0.0 to 1.0
  */
void cwCropImageTask::setRectF(QRectF cropTo) {
    CropRect = cropTo;
}

void cwCropImageTask::setFormatType(cwTextureUploadTask::Format format)
{
    Format = format;
}

void cwCropImageTask::setDataRootDir(const QDir& dataRootDir)
{
    DataRootDir = dataRootDir;
}

QFuture<cwTrackedImagePtr> cwCropImageTask::crop()
{
    auto originalImage = Original;
    auto cropRect = CropRect;
    auto format = Format;
    auto dataRootDir = DataRootDir;

    struct Image {
        cwDiskCacher::Key key;
        QImage croppedImage;
        int dotsPerMeter;
    };

    auto hash = [](const QImage& image)->quint64 {
        return cwImageProvider::imageHash(image);
    };



    auto addCropToDatabase = [dataRootDir](
                                 QImage image,
                                 QString pathToImage,
                                 const QRectF& crop,
                                 quint64 parentHash) {
        // QFileInfo info(pathToImage);

        auto toString = [](const QRectF crop) {
            return
                QString::number(crop.x())
                + "-" +
                QString::number(crop.y())
                + "_" +
                QString::number(crop.width())
                + "x" +
                QString::number(crop.height());
        };

        // cwDiskCacher::Key key {
        //                       toString(crop) + QStringLiteral("crop-") + info.fileName() + QStringLiteral(".") + cwImageProvider::imageCacheExtension(),
        //                       info.dir(),
        //                       QString::number(parentHash, 16)
        // };

        // QByteArray imageData;
        // QBuffer buffer(&imageData);
        // buffer.open(QIODevice::WriteOnly);
        // image.save(&buffer, "PNG");
        // buffer.close();

        // //Save image data into the disk cacher
        // cwDiskCacher cacher(dir);
        // cacher.insert(key, imageData);
        // return key;

        return cwImageProvider::addToImageCache(dataRootDir.path(),
                                                image,
                                                cwImageProvider::imageCacheKey(
                                                    pathToImage,
                                                    toString(crop) + QStringLiteral("crop"),
                                                    parentHash)
                                                );
    };

    auto cropImage = [dataRootDir, originalImage, cropRect, addCropToDatabase, hash]()->Image {
            const QString originalPath = originalImage.path();
            cwImageProvider provider;
            provider.setDataRootDir(dataRootDir);
            const QString requestPath = cwImageProvider::imagePath(originalImage, originalPath);

            QSize imageSize;
            QImage image = provider.requestImage(requestPath, &imageSize, QSize());
            image.setColorSpace(QColorSpace());
            QRect cropArea = nearestDXT1Rect(mapNormalizedToIndex(cropRect,
                                                                  image.size()));

            if(!image.isNull()) {
                QImage croppedImage = image.copy(cropArea);
                auto key = addCropToDatabase(croppedImage, originalImage.path(), cropArea, hash(image));
                return Image({key, image.copy(cropArea), originalImage.originalDotsPerMeter()});
            }

            QImage badImage(cropArea.size(), QImage::Format_ARGB32);
            badImage.fill(QColor("red"));
            // qDebug() << "Original image is bad id:" << originalImage.original() << imageData.data().size() << imageData.size() << imageData.format() << LOCATION;
            return Image({{}, badImage, 0});
    };

    auto cropFuture = cwConcurrent::run(cropImage);

    auto finishedFuture =
        AsyncFuture::observe(cropFuture)
            .subscribe([cropFuture, dataRootDir]() {
            auto dir = dataRootDir;
            auto cropRGBImage = cropFuture.result();

            if(!cropRGBImage.key.id.isEmpty()) {
                cwDiskCacher cacher(dir);
                    auto filePath = cacher.filePath(cropRGBImage.key);
                    cwImage image;
                    image.setOriginalSize(cropRGBImage.croppedImage.size());
                    image.setPath(filePath);

                    return cwTrackedImage::createShared(image,
                                                        image.path(),
                                                        cwTrackedImage::NoOwnership);
                }

                return cwTrackedImagePtr();
            }).future();

    return finishedFuture;
}

/**
  \brief This does the the cropping
  */
void cwCropImageTask::runTask() {
    Q_ASSERT_X(false, "Use cwCropImageTask::crop() instead", LOCATION_STR);
}

/**
  This converts normalized rect into a index rect in terms of pixels.

  This size is the size of the original image.  This is useful for converting normalize rectangle
  into a rectangle. This flips the coordinate system to opengl
  */
QRect cwCropImageTask::mapNormalizedToIndex(QRectF normalized, QSize size) {

     auto clamp = []( const double v, const double lo, const double hi )
    {
        assert( !(hi < lo) );
        return (v < lo) ? lo : (hi < v) ? hi : v;
    };

    double nLeft = clamp(normalized.left(), 0.0, 1.0);
    double nRight = clamp(normalized.right(), 0.0, 1.0);
    double nTop = clamp(normalized.top(), 0.0, 1.0);
    double nBottom = clamp(normalized.bottom(), 0.0, 1.0);

    int left = std::max(0, static_cast<int>(nLeft * size.width()));
    int right = std::min(size.width(), static_cast<int>(nRight * size.width()) - 1);
    int top = std::max(0, static_cast<int>((1.0 - nBottom) * size.height()));
    int bottom = std::min(size.height(), static_cast<int>((1.0 - nTop) * size.height() - 1));
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

/**
 * Rounds rect, either up or down to the nearest dxt1 block
 */
QRect cwCropImageTask::nearestDXT1Rect(QRect rect)
{

    auto nearestFloor = [](int value)->int {
        return 4 * static_cast<int>(std::floor(value / 4.0));
    };

    auto nearestCeiling = [](int value)->int {
        return 4 * static_cast<int>(std::ceil(value / 4.0));
    };


    return QRect(QPoint(nearestFloor(rect.left()), nearestFloor(rect.top())),
                 QSize(nearestCeiling(rect.width()), nearestCeiling(rect.height())));
}
