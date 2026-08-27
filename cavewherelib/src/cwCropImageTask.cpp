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
#include "cwKtx2Codec.h"
#include "cwOpenGLUtils.h"

//Async future
#include <asyncfuture.h>

//Qt includes
#include <QColorSpace>
#include <cmath>

//Std includes
#include <algorithm>

namespace {
    //Keeps scrap textures below the smallest common driver texture limit
    constexpr int kMaxCropPixelDimension = 4096;

    /**
     * Marks the compressed scrap entries in the disk cache. The trailing number
     * is the encode's generation: bump it whenever the encoded bytes change for
     * inputs that hash the same, so old entries stop being served.
     */
    constexpr QLatin1StringView kCompressedScrapKeySuffix("-uastc1");

    /**
     * The shared part of both cache keys for one crop: the crop rect, plus the
     * suffix that marks a downscaled crop.
     */
    QString cropKeyPrefix(const QRectF& crop, const QString& keySuffix)
    {
        return QString::number(crop.x())
               + QStringLiteral("-")
               + QString::number(crop.y())
               + QStringLiteral("_")
               + QString::number(crop.width())
               + QStringLiteral("x")
               + QString::number(crop.height())
               + QStringLiteral("crop")
               + keySuffix;
    }

    /**
     * Encodes croppedImage as UASTC .ktx2 and stores it beside the PNG crop in
     * the disk cache, keyed off the same content hash so an edited note
     * invalidates both entries together. Returns an empty key when the encode
     * failed and the caller should stay on the PNG crop.
     *
     * The cache lookup comes first: encoding a 4096 pixel crop costs seconds of
     * CPU, and rewarping a note re-crops pixels that are usually unchanged. The
     * lookup reads the entry rather than testing the file's existence, because
     * an edited note lands on the same cache file path with a new checksum and
     * only a read notices that the stored bytes are stale.
     */
    cwDiskCacher::Key addCompressedCropToCache(const QDir& dataRootDir,
                                               const QImage& croppedImage,
                                               const QString& pathToImage,
                                               const QString& keyPrefix,
                                               quint64 parentHash)
    {
        const cwDiskCacher::Key key = cwImageProvider::imageCacheKey(
            pathToImage,
            keyPrefix + kCompressedScrapKeySuffix,
            parentHash);

        cwDiskCacher cacher(dataRootDir);
        if(!cacher.entry(key).isEmpty()) {
            return key;
        }

        //Scrap texcoords use the OpenGL bottom-left origin, so the compressed
        //texture must carry the same flip cwOpenGLUtils::toGLTexture() gives the
        //uncompressed path
        const auto encoded = cw::ktx2::encodeRgba(cwOpenGLUtils::toGLTexture(croppedImage));
        if(encoded.hasError()) {
            qWarning() << "Can't compress scrap texture, using the uncompressed image:"
                       << encoded.errorMessage();
            return {};
        }

        cacher.insert(key, encoded.value());
        return key;
    }
}

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

QFuture<cwCropImageTask::Result> cwCropImageTask::crop()
{
    auto originalImage = Original;
    auto cropRect = CropRect;
    auto dataRootDir = DataRootDir;

    struct Image {
        cwDiskCacher::Key key;
        cwDiskCacher::Key compressedKey;
        QImage croppedImage;
        int dotsPerMeter;
    };

    auto cropImage = [dataRootDir, originalImage, cropRect]()->Image {
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
                int dotsPerMeter = originalImage.originalDotsPerMeter();
                QString keySuffix;

                if(std::max(croppedImage.width(), croppedImage.height()) > kMaxCropPixelDimension) {
                    croppedImage = croppedImage.scaled(kMaxCropPixelDimension,
                                                       kMaxCropPixelDimension,
                                                       Qt::KeepAspectRatio,
                                                       Qt::SmoothTransformation);

                    const double scale = static_cast<double>(croppedImage.width())
                                         / static_cast<double>(cropArea.width());
                    dotsPerMeter = static_cast<int>(std::round(dotsPerMeter * scale));
                    keySuffix = QStringLiteral("-max")
                                + QString::number(kMaxCropPixelDimension);
                }

                const quint64 parentHash = cwImageProvider::imageHash(image);
                const QString keyPrefix = cropKeyPrefix(cropArea, keySuffix);

                const auto key = cwImageProvider::addToImageCache(
                    dataRootDir.path(),
                    croppedImage,
                    cwImageProvider::imageCacheKey(originalPath, keyPrefix, parentHash));
                const auto compressedKey = addCompressedCropToCache(dataRootDir,
                                                                    croppedImage,
                                                                    originalPath,
                                                                    keyPrefix,
                                                                    parentHash);
                return Image({key, compressedKey, croppedImage, dotsPerMeter});
            }

            QImage badImage(cropArea.size(), QImage::Format_ARGB32);
            badImage.fill(QColor("red"));
            // qDebug() << "Original image is bad id:" << originalImage.original() << imageData.data().size() << imageData.size() << imageData.format() << LOCATION;
            return Image({{}, {}, badImage, 0});
    };

    auto cropFuture = cwConcurrent::run(cropImage);

    auto finishedFuture =
        AsyncFuture::observe(cropFuture)
            .subscribe([cropFuture, dataRootDir]() {
                const auto cropRGBImage = cropFuture.result();

                if(cropRGBImage.key.id.isEmpty()) {
                    return Result();
                }

                const cwDiskCacher cacher(dataRootDir);
                cwImage image;
                image.setOriginalSize(cropRGBImage.croppedImage.size());
                image.setOriginalDotsPerMeter(cropRGBImage.dotsPerMeter);
                image.setPath(cacher.filePath(cropRGBImage.key));

                return Result {
                    cwTrackedImage::createShared(image,
                                                 image.path(),
                                                 cwTrackedImage::NoOwnership),
                    cropRGBImage.compressedKey,
                    cropRGBImage.croppedImage.size()
                };
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
