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

//Qt includes
#include <QByteArray>
#include <QBuffer>
#include <QImageWriter>
#include <QList>

//TODO: REMOVE for testing only
#include <QFile>

cwCropImageTask::cwCropImageTask(QObject* parent) :
    cwProjectIOTask(parent) {
    AddImageTask = new cwAddImageTask(this);
    AddImageTask->setParentTask(this);
    AddImageTask->setUsingThreadPool(false);

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

/**
 * @brief cwCropImageTask::setMipmapOnly
 * @param mipmapOnly - This make the crop image task only create images with mipmaps
 */
void cwCropImageTask::setMipmapOnly(bool mipmapOnly)
{
    AddImageTask->setMipmapsOnly(mipmapOnly);
}

/**
  This gets the finished cropped image.  This should be called when the task has completed
  */
cwImage cwCropImageTask::croppedImage() const {
    return CroppedImage;
}

/**
  \brief This does the the cropping
  */
void cwCropImageTask::runTask() {
    ImageProvider.setProjectPath(databaseFilename());
//    cwImageData imageData = ImageProvider.data(Original.original());
//    QImage image = QImage::fromData(imageData.data(), imageData.format());
//    QImage croppedImage = image.copy(nearestDXT1Rect(mapNormalizedToIndex(CropRect, Original.originalSize())));

//    if(image.isNull()) {
//        done();
//        return;
//    }

//    qDebug() << "Image:" << image.size() << croppedImage.size() << Original.original();

//    auto cropMipmap = [croppedImage](const cwImageData& imageData, const QRect& cropArea, double scaleFactor) {
//        constexpr int dxt1BlockPixelSize = 4;

//        //Returns true if the
//        auto canDirectCopy = [dxt1BlockPixelSize](const QRect& cropArea) {
//            return cropArea.x() % dxt1BlockPixelSize  == 0
//                    && cropArea.y() % dxt1BlockPixelSize == 0
//                    && cropArea.width() % dxt1BlockPixelSize == 0
//                    && cropArea.height() % dxt1BlockPixelSize == 0;
//        };

//        //This uses the original mipmap, to crop the data.
//        auto directMipmapCrop = [dxt1BlockPixelSize](const cwImageData& imageData, const QRect& cropArea) {
//            //Crop area must be on 4 pixel boundary
//            Q_ASSERT(cropArea.x() % 4 == 0);
//            Q_ASSERT(cropArea.y() % 4 == 0);
//            Q_ASSERT(cropArea.width() % 4 == 0);
//            Q_ASSERT(cropArea.height() % 4 == 0);

//            QRect scaledCropArea(cropArea.x() / dxt1BlockPixelSize,
//                                 cropArea.y() / dxt1BlockPixelSize,
//                                 std::max(1, cropArea.width() / dxt1BlockPixelSize),
//                                 std::max(1, cropArea.height() / dxt1BlockPixelSize));

//            qDebug() << "ScaledCropArea:" << scaledCropArea;

//            //ImageData should be made out of 8 byte blocks
//            QByteArray dxt1Data = imageData.data();
//            Q_ASSERT(dxt1Data.size() % 8 == 0);

//            QByteArray cropData;
//            cropData.fill(0, static_cast<int>(scaledCropArea.width() * scaledCropArea.height()) * static_cast<int>(sizeof(qint64)));
//            qint64* cropDataPtr = reinterpret_cast<qint64*>(cropData.data());

//            int dxt1Width = static_cast<int>(std::ceil(imageData.size().width() / static_cast<double>(dxt1BlockPixelSize)));
//            int lastDataIndex = dxt1Data.size() / sizeof(qint64) - 1;

//            //        int extra = static_cast<int>(std::ceil((cropArea.size().width() - realSize.width()) / dxt1BlockPixelSize));
//            //        qDebug() << "Diff:" << cropArea.size().width() - realSize.width();

//            for(int r = 0; r < scaledCropArea.height(); r++) {
//                for(int c = 0; c < scaledCropArea.width(); c++) {
//                    int index = std::min(lastDataIndex, dxt1Width * (scaledCropArea.y() + r) + (scaledCropArea.x() + c));
//                    Q_ASSERT(index < static_cast<int>(dxt1Data.size() / static_cast<int>(sizeof(qint64))));
//                    auto valuePtr = &reinterpret_cast<const qint64*>(dxt1Data.constData())[index];

//                    int cropIndex = r * scaledCropArea.width() + c;
//                    //                int cropIndex = r * realSize.width() / dxt1BlockPixelSize + c;
//                    Q_ASSERT(cropIndex < static_cast<int>(cropData.size() / static_cast<int>(sizeof(qint64))));
//                    cropDataPtr[cropIndex] = *valuePtr;
//                }
//            }

//            //        return cwImageProvider::createDxt1(QSizeF(scaledCropArea.size() * dxt1BlockPixelSize).toSize(), cropData);
//            return cwImageProvider::createDxt1(cropArea.size(), cropData);
//        };

//        //Crop and scale the orginal to generate the mipmap
//        auto regenerationCrop = [croppedImage](const QRect& cropArea, double scaleFactor) {

//            cwDXT1Compresser compresser;
////            QSize size = QSize(croppedImage.size() * scaleFactor;
//            qDebug() << "CroppedImage:" << croppedImage.size();
//            QImage scaled = croppedImage.scaled(cropArea.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
//            auto future = compresser.compress({scaled});
//            auto compressedImage = compresser.results(future).first();
//            Q_ASSERT(cropArea.size() == compressedImage.size);
//            return cwImageProvider::createDxt1(cropArea.size(), compressedImage.data);
//        };

//        if(canDirectCopy(cropArea)) {
//            qDebug() << "Direct crop:" << cropArea;
//            return directMipmapCrop(imageData, cropArea);
//        }

//        qDebug() << "Regeneration:" << log2(1 / scaleFactor);
//        return regenerationCrop(cropArea, scaleFactor);





////        auto validDxt1Rect = [dxt1BlockPixelSize](const QRect& rect) {
////            return rect.width() >= dxt1BlockPixelSize && rect.height() >= dxt1BlockPixelSize;
////        };

////        if(!validDxt1Rect(cropArea)) {
////            return cwImageData();
////        }


//    };

//    //Scales the rect using x and y scale
//    auto scaleRect = [](const QRect& rect, double xScale, double yScale) {
//        //        Q_ASSERT(xScale <= 1.0);
//        Q_ASSERT(xScale >= 0.0);
//        //        Q_ASSERT(yScale <= 1.0);
//        Q_ASSERT(yScale >= 0.0);
//        return QRect(static_cast<int>(rect.x() * xScale),
//                     static_cast<int>(rect.y() * yScale),
//                     static_cast<int>(rect.width() * xScale),
//                     static_cast<int>(rect.height() * yScale));
//    };

//    QRectF newCropRect(QPointF(CropRect.left(), 1.0 - CropRect.bottom()),
//                       QPointF(CropRect.right(), 1.0 - CropRect.top()));
//    QRect cropArea = nearestDXT1Rect(mapNormalizedToIndex(newCropRect, Original.originalSize()));
//    qDebug() << "Sauce:" << (1.0 - CropRect.top()) << (1.0 - CropRect.bottom());
//    qDebug() << "Crop:" << CropRect << newCropRect << cropArea;
//    auto mipmaps = Original.mipmaps();
//    int numLevels = cwAddImageTask::numberOfMipmapLevels(cropArea.size());

//    struct MipmapLevel {
//        int id;
//        int level;
//        QRect cropArea;
//    };

//    QVector<MipmapLevel> levels;
//    levels.reserve(numLevels);

//    levels.append({mipmaps.at(0), 0, cropArea});
//    for(int i = 1; i < numLevels; i++) {
//        auto previousMipmapArea = levels.at(i - 1).cropArea;
//        levels.append({mipmaps.at(i), i, cwAddImageTask::half(previousMipmapArea)});
//    }

//    //    auto mipmaps = QList<int>({Original.mipmaps().first()});
//    QList<cwImageData> cropMimapData;
//    cropMimapData.reserve(mipmaps.size());
//    std::transform(levels.begin(), levels.end(), std::back_inserter(cropMimapData),
//                   [this, cropMipmap, scaleRect, cropArea](MipmapLevel mipmap)
//    {
//        qDebug() << "Level:" << mipmap.id << mipmap.level << mipmap.cropArea;
//        auto mipmapData = ImageProvider.data(mipmap.id);

//        double scale = 1 / static_cast<double>(1 << mipmap.level);
////        double xScale = mipmapData.size().width() / static_cast<double>(Original.originalSize().width());
////        double yScale = mipmapData.size().height() / static_cast<double>(Original.originalSize().height());

////        qDebug() << scaleRect(cropArea, xScale, yScale);

////        QRect scaledCropArea = nearestDXT1Rect(mipmap.cropArea); //scaleRect(cropArea, xScale, yScale));
////        QSize realSize = mipmap.cropArea.size();
//        return cropMipmap(mipmapData, mipmap.cropArea, scale); //, mipmap.cropArea.size());
//    });

//    //Filter out only the valid mipmaps
//    auto lastGood = std::find_if(cropMimapData.begin(), cropMimapData.end(), [](const cwImageData& data) { return data.size().isEmpty();});
//    QList<cwImageData> goodMipmaps;
//    goodMipmaps.reserve(static_cast<int>(std::distance(cropMimapData.begin(), lastGood)));
//    std::copy(cropMimapData.begin(), lastGood, std::back_inserter(goodMipmaps));

//    cwImage outputImage;

//    //Add good images to the database
//    if(!goodMipmaps.isEmpty()) {
//        //Connect to the database
//        bool connected = connectToDatabase("CropImagesTask");

//        if(connected) {

//            QList<int> mipmapIds;
//            mipmapIds.reserve(goodMipmaps.size());
//            std::transform(goodMipmaps.begin(), goodMipmaps.end(), std::back_inserter(mipmapIds),
//                           [this](const cwImageData& image) {
//                qDebug() << "Writing image:" << image.size() << image.data().size();
//                return cwProject::addImage(database(), image);
//            });

//            outputImage.setMipmaps(mipmapIds);

//            cwImageData originalImageData(goodMipmaps.first().size(),
//                                          Original.originalDotsPerMeter(),
//                                          "no-data-mipmap-only",
//                                          QByteArray());
//            int cropOriginalId = cwProject::addImage(database(), originalImageData);
//            outputImage.setOriginalSize(goodMipmaps.first().size());
//            outputImage.setOriginalDotsPerMeter(Original.originalDotsPerMeter());
//            outputImage.setOriginal(cropOriginalId);

//            //Close the database
//            disconnectToDatabase();
//        } else {
//            qDebug() << "Couldn't connect to the database!" << LOCATION;
//        }
//    }
//    //    cwImageData imageData = ImageProvider.data(Original.original());
//    //    QImage image = QImage::fromData(imageData.data(), imageData.format());
//    //    QImage croppedImage = image.copy(cropArea);

//    CroppedImage = outputImage;



    cwImageData imageData = ImageProvider.data(Original.original());
    QImage image = QImage::fromData(imageData.data(), imageData.format());
    QImage croppedImage = image.copy(mapNormalizedToIndex(CropRect, Original.originalSize()));

    if(image.size().isEmpty()) {
        qDebug() << "Can't crop an image with no size";
        stop();
        done();
        return;
    }



//    qDebug() << "image:" << Original.original() << image << imageData.format() << croppedImage.size() << imageData.size() << CropRect << cropArea;

    QList<QImage> images;
    images.append(croppedImage);

    AddImageTask->setDatabaseFilename(databaseFilename());
    AddImageTask->setNewImages(images);
    AddImageTask->start();

    Q_ASSERT(AddImageTask->isReady());

    //We need to make sure we're still running, because the AddImageTask
    //May have stopped, if the user stopped this task.  If the user stopped
    //this task, the addImageTask may return no images.
    if(isRunning()) {
        QList<cwImage> croppedImages = AddImageTask->images();
        Q_ASSERT(croppedImages.size() == 1);

        CroppedImage = croppedImages.first();
    }

    done();
}

/**
  This converts normalized rect into a index rect in terms of pixels.

  This size is the size of the original image.  This is useful for converting normalize rectangle
  into a rectangle. This flips the coordinate system to opengl
  */
QRect cwCropImageTask::mapNormalizedToIndex(QRectF normalized, QSize size) {
    int left = std::max(0, static_cast<int>(normalized.left() * size.width()));
    int right = std::min(size.width() - 1, static_cast<int>(normalized.right() * size.width() - 1));
    int top = std::max(0, static_cast<int>((1.0 - normalized.bottom()) * size.height()));
    int bottom = std::min(size.height() - 1, static_cast<int>((1.0 - normalized.top()) * size.height() - 1));
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

