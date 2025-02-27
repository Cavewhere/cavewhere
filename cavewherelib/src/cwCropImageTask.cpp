/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


//Our includes
#include "cwCropImageTask.h"
#include "cwImageData.h"
#include "cwDebug.h"
#include "cwProject.h"
#include "cwAddImageTask.h"
#include "cwAsyncFuture.h"
#include "cwImageDatabase.h"
#include "cwConcurrent.h"

//Qt includes
#include <QByteArray>
#include <QBuffer>
#include <QImageWriter>
#include <QList>
#include <QColorSpace>
#include <QJsonDocument>

//TODO: REMOVE for testing only
#include <QFile>

//Std includes
#include <algorithm>

//Async Future
#include <asyncfuture.h>

cwCropImageTask::cwCropImageTask(QObject* parent) :
    cwProjectIOTask(parent) {

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

QFuture<cwTrackedImagePtr> cwCropImageTask::crop()
{
    auto filename = databaseFilename();
    auto originalImage = Original;
    auto cropRect = CropRect;
    auto format = Format;

    struct Image {
        int id;
        QImage croppedImage;
        int dotsPerMeter;
    };

    auto addCropToDatabase = [filename](QRect cropRect, const cwImage& image) {
        QVariantMap map({
                            {cwImageProvider::cropIdKey(), image.original()},
                            {cwImageProvider::cropXKey(), cropRect.x()},
                            {cwImageProvider::cropYKey(), cropRect.y()},
                            {cwImageProvider::cropWidthKey(), cropRect.width()},
                            {cwImageProvider::cropHeightKey(), cropRect.height()}
                        });
        auto document = QJsonDocument::fromVariant(map);
        auto json = document.toJson(QJsonDocument::Compact);

        cwImageData imageData(cropRect.size(),
                              image.originalDotsPerMeter(),
                              cwImageProvider::croppedReferenceExtension(),
                              json);

        cwImageDatabase database(filename);
        return database.addImage(imageData);
    };

    auto cropImage = [filename, originalImage, cropRect, addCropToDatabase]()->Image {
            cwImageProvider provider;
            provider.setProjectPath(filename);
            cwImageData imageData = provider.data(originalImage.original());
            QImage image = provider.image(imageData);
            image.setColorSpace(QColorSpace());
            QRect cropArea = nearestDXT1Rect(mapNormalizedToIndex(cropRect,
                                                                  originalImage.originalSize()));
            if(!image.isNull()) {
                int id = addCropToDatabase(cropArea, originalImage);
                return Image({id, image.copy(cropArea), originalImage.originalDotsPerMeter()});
            }

            QImage badImage(cropArea.size(), QImage::Format_ARGB32);
            badImage.fill(QColor("red"));
            qDebug() << "Original image is bad id:" << originalImage.original() << imageData.data().size() << imageData.size() << imageData.format() << LOCATION;
            return Image({-1, badImage, 0});
    };

    auto cropFuture = cwConcurrent::run(cropImage);

    auto addImageFuture = AsyncFuture::observe(cropFuture)
            .subscribe([cropFuture, filename, format]()
    {
        int imageTypes = cwAddImageTask::None;
        // if(format == cwTextureUploadTask::DXT1Mipmaps) {
        //     imageTypes |= cwAddImageTask::Mipmaps;
        // }

        Image cropRGBImage = cropFuture.result();
        if(cropRGBImage.id < 0) {
            //Bad image, add the red image crop
            imageTypes |= cwAddImageTask::Original;
        }

        cwAddImageTask addImages;
        addImages.setDatabaseFilename(filename);
        addImages.setNewImages({cropRGBImage.croppedImage});
        addImages.setImageTypes(imageTypes);

        return addImages.images();
    }).future();

    auto finishedFuture = AsyncFuture::observe(addImageFuture)
            .subscribe([addImageFuture, cropFuture]()
    {
        auto cropRGBImage = cropFuture.result();
        auto images = addImageFuture.results();
        if(!images.isEmpty()) {
            auto imagePtr = images.first();
            if(cropRGBImage.id > 0) {
                //Update with the ref image
                imagePtr->setOriginalDotsPerMeter(cropRGBImage.dotsPerMeter);
                imagePtr->setOriginalSize(cropRGBImage.croppedImage.size());
                imagePtr->setOriginal(cropRGBImage.id);
            }
            return images.first();
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

