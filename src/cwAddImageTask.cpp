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
//#include "cwImageDatabase.h"

//For creating compressed DXT texture maps
#include <squish.h>

//Std includes
#include "cwMath.h"

//Qt includes
#include <QString>
#include <QImage>
#include <QGLWidget>
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
    MipmapOnly = false;
}

cwAddImageTask::~cwAddImageTask()
{
}

/**
  \brief This loads images from disk into the database

  This will mipmap the images as well, create a icon image.  The original is also stored
  */
void cwAddImageTask::runTask() {
    //Clear all previous data
    Images.clear();

    //Clear the current progress
    Progress = QAtomicInt(0);

    //Set the number of steps for this task
    calculateNumberOfSteps();

    //Connect to the database
    bool connected = connectToDatabase("AddImagesTask");

    if(connected) {
        //Try to add the ImagePaths to the database
        tryAddingImagesToDatabase();

        //Close the database
        disconnectToDatabase();

        if(isRunning()) {
            emit addedImages(images());
        }
    } else {
        qDebug() << "Couldn't connect to the database!" << LOCATION;
    }

    //Finished
    done();
}

/**
  \brief This calculate the number of steps in this task

  The number of steps are equal = sum of all pixels in each mipmap
  */
void cwAddImageTask::calculateNumberOfSteps() {

    QList<QSize> sizes;
    sizes.reserve(NewImagePaths.size() + Images.size());

    foreach(QString imagePath, NewImagePaths) {
        QImageReader reader(imagePath);
        QSize imageSize;
        if(reader.supportsOption(QImageIOHandler::Size)) {
            //Format support reading the size without reading the whole image
            imageSize = reader.size();
        } else {
            //Read the whole image and get the size
            QImage image = reader.read();
            imageSize = image.size();
        }
        sizes.append(imageSize);
    }

    foreach(QImage image, NewImages) {
        sizes.append(image.size());
    }

    int numberOfSteps = 0;
    foreach(QSize imageSize, sizes) {
        int numberOfMipmapLevel = numberOfMipmapLevels(imageSize);
        for(int i = 0; i < numberOfMipmapLevel; i++) {
            int iterWidth = imageSize.width() / 4 + 1;
            int iterHeight = imageSize.height() / 4 + 1;

            numberOfSteps += iterWidth * iterHeight;
            imageSize = half(imageSize);
        }
    }

    setNumberOfSteps(numberOfSteps);
}

/**
  \brief This tries to add the image to the database
  */
void cwAddImageTask::tryAddingImagesToDatabase() {
//    bool good = beginTransation(SLOT(tryAddingImagesToDatabase()));
//    if(!good) {
//        qDebug() << "Couldn't begin transaction!";
//        return;
//    }

    //Database image, original image
    QList< PrivateImageData > images;

    //Go through all the images strings
    for(int i = 0; i < NewImagePaths.size() && isRunning(); i++) {
        QString imagePath = NewImagePaths[i];

        //Where the database image ideas are stored
        cwImage imageIds;

        //Copy the original image to the database
        QImage originalImage;
        originalImage = copyOriginalImage(imagePath, &imageIds);

        if(!originalImage.isNull()) {
            images.append(PrivateImageData(imageIds, originalImage, imagePath));
        }
    }

    //Go through all the images
    for(int i = 0; i < NewImages.size() && isRunning(); i++) {
        QImage originalImage = NewImages[i];

        //Where the database image ideas are stored
        cwImage imageIds;

        if(!MipmapOnly) {
            copyOriginalImage(originalImage, &imageIds);
        } else {
            imageIds = originalMetaData(originalImage);
        }

        images.append(PrivateImageData(imageIds, originalImage));
    }

    //Go through all the images
    for(int i = 0; i < images.size() && isRunning(); i++) {
        cwImage& imageIds = images[i].Id;
        const QImage& originalImage = images[i].OriginalImage;
        const QString name = images[i].Name;

        //Create a icon image
        if(!MipmapOnly) {
            createIcon(originalImage, name, &imageIds);
        }

        //Create mipmaps
        createMipmaps(originalImage, name, &imageIds);

        //Add image ids to the list of images that are returned
        Images.append(imageIds);

        //  emit progressed(i + 1);
    }

    if(RegenerateImage.isValid()) {
        //Regenerate the mipmaps based on what's aleardy in the database

        cwImageProvider imageProvider;
        imageProvider.setProjectPath(databaseFilename());
        QImage originalImage = imageProvider.image(RegenerateImage.original());

        if(!originalImage.isNull()) {
            createMipmaps(originalImage, "", &RegenerateImage);
        }
    }

//    endTransation();
}


/**
  \brief This copies the original image to the new place
  */
QImage cwAddImageTask::copyOriginalImage(QString imagePath, cwImage* imageIdContainer) {

    emit statusMessage(QString("Copying %1").arg(QFileInfo(imagePath).fileName()));

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

    if(format.isEmpty()) {
        qDebug() << "This file is not an image:" << imagePath << LOCATION;
        return QImage();
    }

    //Load the image
    QImage image;
    image.loadFromData(originalImageByteData, format.constData());

    if(MipmapOnly) {
        originalImageByteData = QByteArray();
    }

    *imageIdContainer = addImageToDatabase(image, format, originalImageByteData);

    return image;
}

/**
    \brief this adds the image to the database
  */
void cwAddImageTask::copyOriginalImage(const QImage &image, cwImage *imageIds)
{
    QByteArray format = "png";
    QByteArray imageData;

    if(!MipmapOnly) {
        QBuffer buffer(&imageData);
        QImageWriter writer(&buffer, format);
        writer.write(image);
    }

    *imageIds = addImageToDatabase(image, format, imageData);
}

/**
  \brief Adds the image to the database
  */
cwImage cwAddImageTask::addImageToDatabase(const QImage &image,
                                           const QByteArray &format,
                                           const QByteArray &imageData)
{
    cwImage imageIdContainer = originalMetaData(image);

    //Write the image to the database
    cwImageData originalImageData(image.size(),
                                  imageIdContainer.originalDotsPerMeter(),
                                  format,
                                  imageData);

    int imageId = cwProject::addImage(database(), originalImageData);

    imageIdContainer.setOriginal(imageId);

    return imageIdContainer;
}

/**
  \brief Creates an icon of the original image

  If the originalImage is less than 512x512, this just save the full image as a icon
  */
void cwAddImageTask::createIcon(QImage originalImage, QString imageFilename, cwImage* imageIds) {
    emit statusMessage(QString("Generating icon for %1").arg(QFileInfo(imageFilename).fileName()));

    QSize scaledSize = QSize(512, 512);

    if(originalImage.size().height() <= scaledSize.height() &&
            originalImage.size().width() <= scaledSize.width()) {
        //Make the original the icon
        imageIds->setIcon(imageIds->original());
        return;
    }

    QImage scaledImage = originalImage.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    //Convert the image into a jpg
    QByteArray format = "jpg";
    QByteArray jpgData;
    QBuffer buffer(&jpgData);
    QImageWriter writer(&buffer, format);
    writer.setCompression(85);
    writer.write(scaledImage);

    int dotMeter = imageIds->originalDotsPerMeter() > 0 ? scaledImage.dotsPerMeterX() : 0;

    //Write the data to database
    cwImageData iconImageData(scaledSize, dotMeter, format, jpgData);
    int imageId = cwProject::addImage(database(), iconImageData);
    imageIds->setIcon(imageId);
}

/**
  \brief This creates compressed mipmaps for the originalImage

  The return stringList is a list of all the mipmaps, starting with level 0 going to level
  size-1 of the list.
  */
void cwAddImageTask::createMipmaps(QImage originalImage,
                                   QString imageFilename,
                                   cwImage* imageIds) {

    QSizeF clipArea;
    QImage scaledImage = ensureImageDivisibleBy4(originalImage, &clipArea);
    QList<int> mipmapIds;

    int numberOfLevels = numberOfMipmapLevels(scaledImage.size());

    QSize scaledImageSize = scaledImage.size();

    bool regeneratingMipmaps = numberOfLevels == imageIds->mipmaps().size();

    QList<int> generatedMipmapIds;
    QList<QImage> scaledImages;

    for(int i = 0; i < numberOfLevels && isRunning(); i++) {
        emit statusMessage(QString("Compressing %1 of %2 bold flavors of %3").arg(i + 1).arg(numberOfLevels).arg(QFileInfo(imageFilename).fileName()));

        //Rescaled the image
        scaledImage = scaledImage.scaled(scaledImageSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        int mipmapId = -1;
        if(regeneratingMipmaps) {
            mipmapId = imageIds->mipmaps().at(i);
        }

        generatedMipmapIds.append(mipmapId);
        scaledImages.append(scaledImage);

        //Create the new width and height, by halfing them
        scaledImageSize = half(scaledImageSize);
    }

    if(!isRunning()) {
        return;
    }

    cwDXT1Compresser compresser;
    auto future = compresser.compress(scaledImages);
    auto images = compresser.results(future);

    Q_ASSERT(images.size() == generatedMipmapIds.size());

    //Export the image to DXT1 format
    for(int i = 0; i < images.size() && isRunning(); i++) {
        int mipmapId = generatedMipmapIds.at(i);
        cwDXT1Compresser::CompressedImage image = images.at(i);
        mipmapId = saveToDXT1Format(image, mipmapId);

        //Add the path to the mipmapPath
        if(!regeneratingMipmaps) {
            mipmapIds.append(mipmapId);
        }
    }

    imageIds->setMipmaps(mipmapIds);
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

/**
  \brief Saves the image to the path using the dxt1 format from squish

  The image will be firsted compress using dxt1 compression 1:4.  Then it'll
  be compressed with zlib to gunzip.  This will compress the image by another
  35% by default.

  \param image - The image that'll be converted
  \param id - The id that'll be overwritten by the task
  */
int cwAddImageTask::saveToDXT1Format(const cwDXT1Compresser::CompressedImage &image, int id) {
    //Add the image to the database
    cwImageData imageData = cwImageProvider::createDxt1(image.size, image.data);

    int imageId;
    if(id == -1) {
        //Add the image
        imageId = cwProject::addImage(database(), imageData);
    } else {
        imageId = id;
        cwProject::updateImage(database(), imageData, id);
    }

    return imageId;
}

/**
  Gets the number of dots per meter of the image

  If the image doesn't have the same dots per meter in both the x and y direction, then
  zero is returned.
  */
int cwAddImageTask::dotsPerMeter(QImage image) const {
    if(image.dotsPerMeterX() != image.dotsPerMeterY()) {
        return 0;
    }
    return image.dotsPerMeterX();
}

/**
 * @brief cwAddImageTask::regenerateMipmaps
 *
 * This regenerates the mipmaps for RegenerateImage.  This might be used
 * to regenerate the correct mipmap compression for moble devices or
 * if the compression was incorrect generate the first time.
 */
void cwAddImageTask::regenerateMipmaps()
{
    if(RegenerateImage.isValid()) {
        //Get the original image from the database
        cwImageProvider imageProvider;
        imageProvider.setProjectPath(databaseFilename());

        QImage original = imageProvider.image(RegenerateImage.original());
    }
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

/**
  \brief This increases the current progress of the task

  This function is thread safe

  This uses an atomic integer that's thread safe
  */
void cwAddImageTask::IncreaseProgress() {
    int originalValue = Progress.fetchAndAddRelaxed(1);

    //Normalize to progress
    double percent = 100.0 * (originalValue / (double)numberOfSteps());
    int wholeProgress = (qRound(percent) / 100.0) * numberOfSteps();

    wholeProgress = qMax(0, qMin(wholeProgress, numberOfSteps()));

    setProgress(wholeProgress);
}

cwImage cwAddImageTask::originalMetaData(const QImage &image) const
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


