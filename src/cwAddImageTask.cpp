/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwAddImageTask.h"
#include "cwProject.h"
#include "cwImageData.h"
#include "cwImageProvider.h"
#include "cwDebug.h"
#include "cwDXT1Compresser.h"
#include "cwAsyncFuture.h"
#include "cwOpenGLSettings.h"
#include "cwImageDatabase.h"

//For creating compressed DXT texture maps
#include <squish.h>

//Std includes
#include "cwMath.h"

//Qt includes
#include <QString>
#include <QImage>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QTimer>
#include <QImageReader>
#include <QImageWriter>
#include <QBuffer>
#include <QtConcurrentMap>
#include <QOpenGLContext>
#include <QPainter>
#include <QColorSpace>

//TODO: REMOVE for testing only
#include <QFile>
#include <QTime>

//Sqlite lite includes
#include "sqlite3.h"

//Zlib includes
#include <zlib.h>

//Async includes
#include <asyncfuture.h>

cwAddImageTask::cwAddImageTask(QObject* parent) : cwProjectIOTask(parent)
{

}

cwAddImageTask::~cwAddImageTask()
{
}

void cwAddImageTask::setRegenerateMipmapsOn(cwImage image)
{
    RegenerateMipmap = image;
}

void cwAddImageTask::setFormatType(cwTextureUploadTask::Format format)
{
    FormatType = format;
}

QFuture<cwTrackedImagePtr> cwAddImageTask::images() const
{
    QString filename = databaseFilename();
    auto context = this->context();
    auto format = FormatType;

    std::function<PrivateImageData (const QString&)> loadImagesFromPath
            = [filename](const QString& imagePath) {
        //Where the database image ideas are stored
        cwImage image;

        //Copy the original image to the database
        QImage originalImage = copyOriginalImage(imagePath, &image, filename);

        if(!originalImage.isNull()) {
            return PrivateImageData(cwTrackedImage::createShared(image, filename),
                                    originalImage,
                                    imagePath);
        }
        qDebug() << "Image is null!" << LOCATION;
        return PrivateImageData();
    };

    std::function<PrivateImageData (const QImage&)> loadFromImages
            = [filename](const QImage& image) {
        if(!image.isNull()) {
            //Where the database image ideas are stored
            cwImage imageId;
            copyOriginalImage(image, &imageId, filename);

            return PrivateImageData(cwTrackedImage::createShared(imageId, filename),
                                    image);
        }
        qDebug() << "Image is null!" << LOCATION;
        return PrivateImageData();
    };

    auto loadFromDatabaseImage = [filename](const cwImage& image) {
        cwImageProvider provider;
        provider.setProjectPath(filename);
        QImage imageData = provider.image(image.original());

        if(imageData.isNull()) {
            qDebug() << "Database image is null!" << LOCATION;
            return PrivateImageData();
        }

        return PrivateImageData(cwTrackedImage::createShared(image,
                                                             filename,
                                                             cwTrackedImage::Mipmaps),
                                imageData);
    };

    //For creating icon from private image data
    std::function<cwTrackedImagePtr (const PrivateImageData&)> createIcon
            = [filename](const PrivateImageData& imageData) {
        Q_ASSERT(!imageData.OriginalImage.isNull());

        if(imageData.Id->isIconValid()) {
            return cwTrackedImage::createShared(imageData.Id->icon(),
                                              filename,
                                              cwTrackedImage::NoOwnership);
        }
        const QImage& originalImage = imageData.OriginalImage;
        QSize scaledSize = QSize(512, 512);

        if(originalImage.size().height() <= scaledSize.height() &&
                originalImage.size().width() <= scaledSize.width()) {
            //Make the original the icon
            return cwTrackedImage::createShared(imageData.Id->original(),
                                                filename,
                                                cwTrackedImage::NoOwnership);
        }

        QImage scaledImage = originalImage.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        //FIXME: QImage doesn't support QColorSpaces correctly
        //https://bugreports.qt.io/browse/QTBUG-82803
        scaledImage.setColorSpace(QColorSpace());

        //Convert the image into a jpg
        QByteArray format = "jpg";
        QByteArray jpgData;
        QBuffer buffer(&jpgData);
        QImageWriter writer(&buffer, format);
        writer.setCompression(85);
        writer.write(scaledImage);

        int dotMeter = imageData.Id->originalDotsPerMeter() > 0 ? scaledImage.dotsPerMeterX() : 0;

        //Write the data to database
        cwImageData iconImageData(scaledSize, dotMeter, format, jpgData);
        int imageId = cwImageDatabase(filename).addImage(iconImageData);
        return cwTrackedImage::createShared(imageId, filename);
    };

    struct Mipmap {
        QImage image;
        int id;
    };

    //Scaling images for mipmaps
    std::function<QList<Mipmap> (const PrivateImageData&)> scaleImage
            = [](const PrivateImageData& imageData)->QList<Mipmap> {
        const QImage& originalImage = imageData.OriginalImage;

        QSizeF clipArea;
        QImage scaledImage = ensureImageDivisibleBy4(originalImage, &clipArea);
        QList<int> mipmapIds = imageData.Id->mipmaps();

        auto mipmapId = [mipmapIds](int index) {
            if(index < mipmapIds.size()) {
                return mipmapIds.at(index);
            }
            return -1;
        };

        int numberOfLevels = numberOfMipmapLevels(scaledImage.size());

        QSize scaledImageSize = scaledImage.size();

        QList<Mipmap> scaledImages;

        for(int i = 0; i < numberOfLevels; i++) {
            //Rescaled the image
            scaledImage = scaledImage.scaled(scaledImageSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            scaledImages.append({scaledImage, mipmapId(i)});

            //Create the new width and height, by halfing them
            scaledImageSize = half(scaledImageSize);
        }

        return scaledImages;
    };

    //For creating mipmaps
    std::function<QFuture<cwTrackedImagePtr> (const QList<Mipmap>& image)> compressAndUpload
            = [filename, context](const QList<Mipmap>& mipmaps)->QFuture<cwTrackedImagePtr> {

        QList<QImage> mipmapImages = cw::transform(mipmaps,
                                               [](const Mipmap& mipmap)
        {
            return mipmap.image;
        });

        cwDXT1Compresser compresser;
        compresser.setContext(context);
        auto compressFuture = compresser.compress(mipmapImages);

        return AsyncFuture::observe(compressFuture)
                .context(context,
                         [compressFuture, mipmaps, filename]()
        {
            struct CompressedMipmap {
                cwDXT1Compresser::CompressedImage image;
                int id;
            };

            auto compressionResults = compressFuture.results();
            Q_ASSERT(mipmaps.size() == compressionResults.size());

            auto mipmapIter = mipmaps.begin();
            QList<CompressedMipmap> mipmaps = cw::transform(compressionResults,
                                                        [&mipmapIter](const cwDXT1Compresser::CompressedImage& compressedImage)
            {
                CompressedMipmap mipmap = {compressedImage, mipmapIter->id};
                mipmapIter++;
                return mipmap;
            });

            std::function<cwTrackedImagePtr (const CompressedMipmap&)> updateDatabase
                    = [filename](const CompressedMipmap& mipmap) {
                cwImageData imageData = cwImageProvider::createDxt1(mipmap.image.size, mipmap.image.data);
                int imageId = cwImageDatabase(filename).addOrUpdateImage(imageData, mipmap.id);
                return cwTrackedImage::createShared(imageId, filename);
            };

            return QtConcurrent::mapped(mipmaps, updateDatabase);
        }).future();
    };

    auto pathImagesFuture = NewImagePaths.isEmpty() ? QFuture<PrivateImageData>() : QtConcurrent::mapped(NewImagePaths, loadImagesFromPath);
    auto imagesFuture = NewImages.isEmpty() ? QFuture<PrivateImageData>() : QtConcurrent::mapped(NewImages, loadFromImages);
    auto regeneratedFuture = RegenerateMipmap.isOriginalValid() ? QtConcurrent::run(std::bind(loadFromDatabaseImage, RegenerateMipmap)) : QFuture<PrivateImageData>();
    auto imageCombine = AsyncFuture::combine();

    auto addToImageCombine = [&imageCombine](QFuture<PrivateImageData> future) {
        if(!future.isCanceled()) {
            imageCombine << future;
        }
    };

    addToImageCombine(pathImagesFuture);
    addToImageCombine(imagesFuture);
    addToImageCombine(regeneratedFuture);

    return AsyncFuture::observe(imageCombine.future())
            .context(context,
                     [pathImagesFuture,
                     imagesFuture,
                     regeneratedFuture,
                     format,
                     scaleImage,
                     context,
                     compressAndUpload,
                     createIcon
                     ]()
    {

        QList<PrivateImageData> imageData =
                pathImagesFuture.results()
                + imagesFuture.results()
                + regeneratedFuture.results();

        QList<PrivateImageData> filterData;
        filterData.reserve(imageData.size());
        
        //Remove all invalid images
        std::copy_if(imageData.begin(), imageData.end(), std::back_inserter(filterData),
                     [](const PrivateImageData& data)
        {
            return !data.Id.isNull() && data.Id->original() > 0 && !data.OriginalImage.isNull();
        });

        QFuture<QVector<QFuture<cwTrackedImagePtr>>> compressAndUploadFuture = AsyncFuture::completed(QVector<QFuture<cwTrackedImagePtr>>());

        if(format == cwTextureUploadTask::DXT1Mipmaps) {
            auto scaleFuture = QtConcurrent::mapped(filterData, scaleImage);

            compressAndUploadFuture = AsyncFuture::observe(scaleFuture)
                    .context(context, [compressAndUpload, scaleFuture]()
            {
                QList<QList<Mipmap>> allImages = scaleFuture.results();
                QVector<QFuture<cwTrackedImagePtr>> ids;
                ids.reserve(allImages.size());

                std::transform(allImages.begin(),
                               allImages.end(),
                               std::back_inserter(ids),
                               compressAndUpload);

                return ids;
            }
            ).future();
        }

        auto iconFuture = QtConcurrent::mapped(filterData, createIcon);
        auto idCombine = AsyncFuture::combine() << iconFuture << compressAndUploadFuture;

        return AsyncFuture::observe(idCombine.future())
                .context(context, [=]()
        {
            auto icons = iconFuture.results();
            auto allMipmaps = compressAndUploadFuture.result();

            auto completedImages = [=]() {
                Q_ASSERT(icons.size() == filterData.size());
                Q_ASSERT(icons.size() == allMipmaps.size() || allMipmaps.isEmpty());

                QList<cwTrackedImagePtr> images;

                auto toMipmap = [](const QList<cwTrackedImagePtr>& mipmapPtrs)
                {
                    QList<int> mipmaps;
                    mipmaps.reserve(mipmapPtrs.size());
                    std::transform(mipmapPtrs.begin(), mipmapPtrs.end(), std::back_inserter(mipmaps),
                                   [](const cwTrackedImagePtr& ptr)
                    {
                        return ptr->take().original(); //The original for this image is a mipmapId
                    });
                    return mipmaps;
                };

                for(int i = 0; i < icons.size(); i++) {
                    auto icon = icons.at(i);
                    auto image = filterData.at(i);

                    auto newImage = image.Id;
                    newImage->setIcon(icon->take().original()); //The original for this image is a iconId

                    if(i < allMipmaps.size()) {
                        auto mipmapFutures = allMipmaps.at(i);
                        Q_ASSERT(mipmapFutures.isFinished());
                        newImage->setMipmaps(toMipmap(mipmapFutures.results()));
                    }

                    images.append(newImage);
                }

                return AsyncFuture::completed(images);
            };

            if(format == cwTextureUploadTask::OpenGL_RGBA) {
                //RGBA, so don't use mipmaps
                Q_ASSERT(allMipmaps.isEmpty());
                return completedImages();
            }

            Q_ASSERT(format == cwTextureUploadTask::DXT1Mipmaps);

            //Combine all the futures together
            auto mipmapCombine = AsyncFuture::combine();
            for(const auto& mipmapFuture : allMipmaps) {
                mipmapCombine << mipmapFuture;
            }

            return AsyncFuture::observe(mipmapCombine.future())
                    .context(context, completedImages)
                    .future();
        }).future();
    }).future();
}

/**
  \brief This loads images from disk into the database

  This will mipmap the images as well, create a icon image.  The original is also stored
  */
void cwAddImageTask::runTask() {
    Q_ASSERT(false);
}

/**
  \brief This copies the original image to the new place
  */
QImage cwAddImageTask::copyOriginalImage(QString imagePath,
                                         cwImage* imageIdContainer,
                                         const QString &databaseFilename) {

    //Copy original directly into the database
    QFile originalFile;
    originalFile.setFileName(imagePath);
    bool successful = originalFile.open(QFile::ReadOnly);

    if(!successful) {
        qDebug() << "Couldn't load image: " << imagePath << LOCATION;
        return QImage();
    }

    //Read the whole file
    QByteArray originalImageByteData = originalFile.readAll();

    //The the original file's format
    QByteArray format = QImageReader::imageFormat(imagePath);

    if(!supportedImageFormats().contains(format)) {
        qDebug() << "Not a valid file format:" << imagePath << LOCATION;
        return QImage();
    }

    if(format.isEmpty()) {
        qDebug() << "This file is not an image:" << imagePath << LOCATION;
        return QImage();
    }

    //Load the image
    QImage image;
    image.loadFromData(originalImageByteData, format.constData());

    *imageIdContainer = addImageToDatabase(image,
                                           format,
                                           originalImageByteData,
                                           databaseFilename);

    return image;
}

/**
    \brief this adds the image to the database
  */
void cwAddImageTask::copyOriginalImage(const QImage &image,
                                       cwImage *imageIds,
                                       const QString &databaseFilename)
{
    QByteArray format = "png"; //Alternative is to use "webp", but it seems to be pretty memory leaky
    QByteArray imageData;

    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, format);
    writer.setQuality(100);

    //FIXME: QImage doesn't support QColorSpaces correctly
    //https://bugreports.qt.io/browse/QTBUG-82803
    QImage colorSpaceImage = image;
    colorSpaceImage.setColorSpace(QColorSpace());

    bool writeSuccessful = writer.write(colorSpaceImage);
    Q_ASSERT(writeSuccessful);

    *imageIds = addImageToDatabase(image,
                                   format,
                                   imageData,
                                   databaseFilename);
}

/**
  \brief Adds the image to the database
  */
cwImage cwAddImageTask::addImageToDatabase(const QImage &image,
                                           const QByteArray &format,
                                           const QByteArray &imageData,
                                           const QString &databaseFilename)
{
    cwImage imageIdContainer = originalMetaData(image);

    //Write the image to the database
    cwImageData originalImageData(image.size(),
                                  imageIdContainer.originalDotsPerMeter(),
                                  format,
                                  imageData);

    int imageId = cwImageDatabase(databaseFilename).addImage(originalImageData);

    imageIdContainer.setOriginal(imageId);

    return imageIdContainer;
}

/**
  \brief This calculates the number of mipmap levels that the originalImage will
  make up.

  This takes the largest dimension and takes the log2 of it.

  OPENGL IS EXTREMELY SENSITVE WITH THE NUMBER OF MIPMAPS NEEDED, don't modify
  */
int cwAddImageTask::numberOfMipmapLevels(QSize imageSize) {
    double largestDimension = static_cast<double>(std::max(imageSize.width(), imageSize.height()));
    return std::max(1, static_cast<int>(log2(largestDimension)) + 1);
}

QStringList cwAddImageTask::supportedImageFormats()
{
    return QStringList({"bmp", "gif", "jpg", "jpeg", "png", "tif", "tiff", "svg", "webp"});
}

/**
 * @brief cwAddImageTask::ensureImageDivisibleBy4
 * @param originalImage
 * @param renderTextureSize
 * @return The image that's divisible by 4 in both the height and width
 * The image will be place an 0, 0 (upper left corner).  Extra padding
 * will be added to the right and bottom of the image.  RenderSize will
 * return with normalize area where the image is valid. RenderSize should
 * be close to 1.0 or exactly 1.0 if the originalImage's dimensions are
 * divisible by 4.
 *
 *  The mipmaps should be
 * compressed with DXT1 compression, and for ANGLE (for windows)
 * the mipmaps dimension have to be divisible by 4.  This is
 * a D3D requirement (ANGLE converts opengl to D3D on windows).
 * If the mipmap isn't divisible by 4 it will cause a crash (an abort).
 *
 * renderTextureSize hold the area of the image that should be
 * used for rendering.  This value should be in normalized cooridanates
 * frome 0.0 to 1.0.  Usually the renderTextureSize should be close to
 * 1.0.  This (width, height) can be multiplied against the texture coordinates
 * to get the true texture cordinates.
 */
QImage cwAddImageTask::ensureImageDivisibleBy4(QImage originalImage, QSizeF *clipArea)
{
    Q_ASSERT(clipArea != nullptr);

    QSize imageSize = originalImage.size();
    int widthRemainder = imageSize.width() % 4;
    int heightRemainder = imageSize.height() % 4;

    if(widthRemainder == 0 && heightRemainder == 0) {
        //Simple case, everything peachy
        *clipArea = QSizeF(1.0, 1.0);
        return originalImage;
    }

    //Need to add padding to the image
    int newWidth;
    if(widthRemainder == 0) {
        newWidth = imageSize.width();
    } else {
        newWidth = ((imageSize.width() / 4) + 1) * 4; //Integer math
    }
    double validWidth = imageSize.width() / (double)newWidth;

    int newHeight;
    if(heightRemainder == 0) {
        newHeight = imageSize.height();
    } else {
        newHeight = ((imageSize.height() / 4) + 1) * 4; //Integer math
    }
    double validHeight = imageSize.height() / (double)newHeight;

    QImage paddedImage(newWidth, newHeight, QImage::Format_ARGB32_Premultiplied);

    int heightDiff = newHeight - imageSize.height();
    int widthDiff = newWidth - imageSize.width();


    QPainter painter(&paddedImage);

    if(heightRemainder != 0) {
        QImage topImage = originalImage.copy(0, 0, imageSize.width(), 1);
        for(int i = 0; i < heightDiff; i++) {
            painter.drawImage(QPoint(0, i), topImage);
        }
    }

    if(widthRemainder != 0) {
        QRect copyArea(imageSize.width() - 1, 0, 1, imageSize.height());
        QImage rightImage = originalImage.copy(copyArea);
        for(int i = 0; i < widthDiff; i++) {
            painter.drawImage(QPoint(imageSize.width() + i, heightDiff), rightImage);
        }
    }

    painter.drawImage(QPoint(0, heightDiff), originalImage);
    painter.end();

    Q_ASSERT(validWidth > 0);
    Q_ASSERT(validHeight > 0);
    Q_ASSERT(validWidth <= 1.0);
    Q_ASSERT(validHeight <= 1.0);

    *clipArea = QSizeF(validWidth, validHeight);
    return paddedImage;
}

cwImage cwAddImageTask::originalMetaData(const QImage &image)
{
    int dotsPerMeter = 0;
    if(image.dotsPerMeterX() == image.dotsPerMeterY()) {
        dotsPerMeter = image.dotsPerMeterX();
    }

    cwImage imageIdContainer;
    imageIdContainer.setOriginalSize(image.size());
    imageIdContainer.setOriginalDotsPerMeter(dotsPerMeter);

    return imageIdContainer;
}


