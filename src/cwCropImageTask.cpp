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

//Std includes
#include <algorithm>

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
    constexpr int dxt1BlockPixelSize = 4;
    ImageProvider.setProjectPath(databaseFilename());
    cwImageData imageData = ImageProvider.data(Original.original());
    QImage image = QImage::fromData(imageData.data(), imageData.format());

//    for(int y = 0; y < image.size().height(); y++) {
//        for(int x = 0; x < image.size().width(); x++) {
//            qDebug() << "Pixel:" << image.pixelColor(x, y).name();
//        }
//    }

    QRect cropArea = nearestDXT1Rect(mapNormalizedToIndex(CropRect, Original.originalSize()));

//    qDebug() << "Normalize:" << mapNormalizedToIndex(CropRect, Original.originalSize());
//    qDebug() << cropArea;
//    qDebug() << QPoint(cropArea.left(), image.size().height() - cropArea.bottom() + 1);
//    qDebug() << QPoint(cropArea.right(), image.size().height() - cropArea.top());

    QRect cropAreaQImageSpace = QRect(QPoint(cropArea.left(), image.size().height() - 1 - cropArea.bottom()),
                                      QPoint(cropArea.right(), image.size().height() - 1 - cropArea.top()));
    QImage croppedImage = image.copy(cropArea);
//    croppedImage.fill(0);

//    qDebug() << "Original:" << image.pixelColor(0, 0).name();
//    qDebug() << "CroppedImage:" << croppedImage.pixelColor(0,0).name() << croppedImage.size();

//        QImage croppedImage = image.copy(cropAreaQImageSpace);

//    qDebug() << "Crop:" << cropArea << cropAreaQImageSpace;

    if(image.isNull()) {
        done();
        return;
    }

//    qDebug() << "Image:" << image.size() << croppedImage.size() << Original.original();
    //This uses the original mipmap, to crop the data.
//    auto directMipmapCrop = [dxt1BlockPixelSize](const cwImageData& imageData, const QRect& cropArea) {
//        //Crop area must be on 4 pixel boundary
//        Q_ASSERT(cropArea.x() % 4 == 0);
//        Q_ASSERT(cropArea.y() % 4 == 0);
//        Q_ASSERT(cropArea.width() % 4 == 0);
//        Q_ASSERT(cropArea.height() % 4 == 0);

//        QRect scaledCropArea(cropArea.x() / dxt1BlockPixelSize,
//                             cropArea.y() / dxt1BlockPixelSize,
//                             std::max(1, cropArea.width() / dxt1BlockPixelSize),
//                             std::max(1, cropArea.height() / dxt1BlockPixelSize));
////            QRect scaledCropArea1(QPoint(cropArea.left() / dxt1BlockPixelSize,
////                                        (imageData.size().height() - cropArea.bottom()) / dxt1BlockPixelSize),
////                                 QPoint(cropArea.right() / dxt1BlockPixelSize,
////                                 (imageData.size().height() - cropArea.top()) / dxt1BlockPixelSize));

////        qDebug() << "ScaledCropArea:" << scaledCropArea << scaledCropArea1;

//        qDebug() << "Crop Area:" << scaledCropArea;

//        //ImageData should be made out of 8 byte blocks
//        QByteArray dxt1Data = imageData.data();
//        Q_ASSERT(dxt1Data.size() % 8 == 0);

//        QByteArray cropData;
//        cropData.resize(static_cast<int>(scaledCropArea.width() * scaledCropArea.height()) * static_cast<int>(sizeof(qint64)));
//        qint64* cropDataPtr = reinterpret_cast<qint64*>(cropData.data());

//        int dxt1Width = static_cast<int>(std::ceil(imageData.size().width() / static_cast<double>(dxt1BlockPixelSize)));
//        int lastDataIndex = dxt1Data.size() / sizeof(qint64) - 1;

//        //        int extra = static_cast<int>(std::ceil((cropArea.size().width() - realSize.width()) / dxt1BlockPixelSize));
//        //        qDebug() << "Diff:" << cropArea.size().width() - realSize.width();

////            int yOffset = imageData.size().height() - scaledCropArea.y();

//        for(int r = 0; r < scaledCropArea.height(); r++) {
//            for(int c = 0; c < scaledCropArea.width(); c++) {
//                int index = std::min(lastDataIndex, dxt1Width * (scaledCropArea.y() + r) + (scaledCropArea.x() + c));
//                Q_ASSERT(index < static_cast<int>(dxt1Data.size() / static_cast<int>(sizeof(qint64))));
//                auto valuePtr = &reinterpret_cast<const qint64*>(dxt1Data.constData())[index];

//                int cropIndex = r * scaledCropArea.width() + c;
//                //                int cropIndex = r * realSize.width() / dxt1BlockPixelSize + c;
//                Q_ASSERT(cropIndex < static_cast<int>(cropData.size() / static_cast<int>(sizeof(qint64))));
//                cropDataPtr[cropIndex] = *valuePtr;
//            }
//        }

//        //        return cwImageProvider::createDxt1(QSizeF(scaledCropArea.size() * dxt1BlockPixelSize).toSize(), cropData);
//        return cwImageProvider::createDxt1(cropArea.size(), cropData);
//    };


//    auto addImageToDatabase = [this](const cwImageData& imageData) {
//        bool connected = connectToDatabase("CropImagesTask");
//        if(connected) {
//            return cwProject::addImage(database(), imageData);
//        }
//        qDebug() << "Couldn't connect to the database!" << LOCATION;
//        return -1;
//    };

//    auto mipmaps = Original.mipmaps();

//    auto firstLevel = ImageProvider.data(mipmaps.first());
//    cwImageData firstCrop = directMipmapCrop(firstLevel, cropArea);
//    int firstCropId = addImageToDatabase(firstCrop);

//    Q_ASSERT(firstCrop.size() == croppedImage.size());

//    auto scaledHalfImage = croppedImage.scaled(cwAddImageTask::half(croppedImage.size()), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

//    Q_ASSERT(scaledHalfImage.size() == cwAddImageTask::half(firstCrop.size()));

//    //    auto scaledHalfImage = croppedImage;
//    AddImageTask->setDatabaseFilename(databaseFilename());
//    AddImageTask->setNewImages({scaledHalfImage});
//    AddImageTask->setMipmapsOnly(true);
//    AddImageTask->start();

//    if(isRunning()) {
//        QList<cwImage> croppedImages = AddImageTask->images();
//        Q_ASSERT(croppedImages.size() == 1);
//        auto finalCroppedImage = croppedImages.first();
//        finalCroppedImage.setMipmaps(QList<int>({firstCropId}) + finalCroppedImage.mipmaps());
//        finalCroppedImage.setOriginalSize(firstCrop.size());
//        finalCroppedImage.setOriginalDotsPerMeter(Original.originalDotsPerMeter());
////        finalCroppedImage.setMipmaps(finalCroppedImage.mipmaps());
//        qDebug() << "Updating cropped images!";
//        CroppedImage = finalCroppedImage;

//        AddImageTask->setDatabaseFilename(databaseFilename());
//        AddImageTask->setNewImages({croppedImage});
//        AddImageTask->setMipmapsOnly(true);
//        AddImageTask->start();
//        CroppedImage = AddImageTask->images().first();
//    }

    AddImageTask->setDatabaseFilename(databaseFilename());
    AddImageTask->setNewImages({croppedImage});
    AddImageTask->setMipmapsOnly(true);
    AddImageTask->start();
    if(isRunning()) {
        CroppedImage = AddImageTask->images().first();
    }
    done();
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

