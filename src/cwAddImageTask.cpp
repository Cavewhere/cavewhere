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

cwAddImageTask::cwAddImageTask(QObject* parent) : cwProjectIOTask(parent)
{


    MipmapOnly = false;
}

/**
  \brief This loads images from disk into the database

  This will mipmap the images as well, create a icon image.  The original is also stored
  */
void cwAddImageTask::runTask() {
    CompressionContext = new QOpenGLContext();
    Window = new QWindow();
    Window->setSurfaceType(QSurface::OpenGLSurface);
    Window->create();
    Texture = 0;

    //Clear all previous data
    Images.clear();

    //Clear the current progress
    Progress = QAtomicInt(0);

    //Set the number of steps for this task
    calculateNumberOfSteps();

    bool couldCreate = CompressionContext->create();
    if(!couldCreate) {
        qDebug() << "Could create context:" << couldCreate << LOCATION;
        done();
        return;
    }

    bool couldMakeCurrent = CompressionContext->makeCurrent(Window);
    if(!couldMakeCurrent) {
        qDebug() << "Could make curernt: " << couldMakeCurrent << LOCATION;
        done();
        return;
    }

    

    if(Texture == 0) {
        glGenTextures(1, &Texture);
    }

    //Connect to the database
    bool connected = connectToDatabase("AddImagesTask");

    if(connected) {
        //Try to add the ImagePaths to the database
        tryAddingImagesToDatabase();

        //Close the database
        Database.close();

        if(isRunning()) {
            emit addedImages(images());
        }
    } else {
        qDebug() << "Couldn't connect to the database!" << LOCATION;
    }

    CompressionContext->doneCurrent();

    delete CompressionContext;
    delete Window;

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
            imageSize = halfSize(imageSize);
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
        //        if(MipmapOnly) {
        //            originalImage = QImage(imagePath);
        //        } else {
        originalImage = copyOriginalImage(imagePath, &imageIds);
        //        }

        if(!originalImage.isNull()) {
            images.append(PrivateImageData(imageIds, originalImage, imagePath));
        }
    }

    //Go through all the images
    for(int i = 0; i < NewImages.size() && isRunning(); i++) {
        QImage originalImage = NewImages[i];

        //Where the database image ideas are stored
        cwImage imageIds;

        //FIXME: This !Mipmap break carpeting, becaus cwImage.isValid() fails???
        //        if(!MipmapOnly) {
        copyOriginalImage(originalImage, &imageIds);
        //        }

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
    QByteArray format = "jpg";
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
    int dotsPerMeter = 0;
    if(image.dotsPerMeterX() == image.dotsPerMeterY()) {
        dotsPerMeter = image.dotsPerMeterX();
    }

    //Write the image to the database
    cwImageData originalImageData(image.size(), dotsPerMeter, format, imageData);
    int imageId = cwProject::addImage(Database, originalImageData);

    cwImage imageIdContainer;
    imageIdContainer.setOriginal(imageId);
    imageIdContainer.setOriginalSize(image.size());
    imageIdContainer.setOriginalDotsPerMeter(dotsPerMeter);

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
    int imageId = cwProject::addImage(Database, iconImageData);
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

    for(int i = 0; i < numberOfLevels && isRunning(); i++) {
        emit statusMessage(QString("Compressing %1 of %2 bold flavors of %3").arg(i + 1).arg(numberOfLevels).arg(QFileInfo(imageFilename).fileName()));

        //Rescaled the image
        scaledImage = scaledImage.scaled(scaledImageSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        int mipmapId = -1;
        if(regeneratingMipmaps) {
            mipmapId = imageIds->mipmaps().at(i);
        }

        //Export the image to DXT1 format
        mipmapId = saveToDXT1Format(scaledImage, mipmapId);

        //Add the path to the mipmapPath
        if(!regeneratingMipmaps) {
            mipmapIds.append(mipmapId);
        }

        //Create the new width and height, by halfing them
        scaledImageSize = halfSize(scaledImageSize);
    }

    imageIds->setMipmaps(mipmapIds);
}

/**
  \brief This calculates the number of mipmap levels that the originalImage will
  make up.

  This takes the largest dimension and takes the log2 of it.
  */
int cwAddImageTask::numberOfMipmapLevels(QSize imageSize) const {
    double largestDimension = (double)qMax(imageSize.width(), imageSize.height());
    return (int)log2(largestDimension) + 1;
}


/**
  \brief Saves the image to the path using the dxt1 format from squish

  The image will be firsted compress using dxt1 compression 1:6.  Then it'll
  be compressed with zlib to gunzip.  This will compress the image by another
  35% by default.

  \param image - The image that'll be converted
  \param id - The id that'll be overwritten by the task
  */
int cwAddImageTask::saveToDXT1Format(QImage image, int id) {
    //Convert and compress using dxt1
    //20 times slower on my computer
#ifdef Q_OS_WIN
    //FIXME: Need to have settings to use opengl dxt1 compression!
    //FIXME: This should be used on gl es 2 implementations only. We should check to see if we have glGetCompressTexture
//    QByteArray outputData = squishCompressImageThreaded(image, squish::kDxt1 | squish::kColourIterativeClusterFit);
#else
    //FIXME: This is commented out because this breaks hard on old intel graphics cards
    QByteArray outputData = openglDxt1Compression(image);
#endif

    if(outputData.isEmpty()) {
        return -1;
    }

    //Compress the dxt1FileData using zlib
    outputData = qCompress(outputData, 9);

    //Add the image to the database
    cwImageData iconImageData(image.size(), 0, cwImageProvider::Dxt1_GZ_Extension, outputData);

    int imageId;
    if(id == -1) {
        //Add the image
        imageId = cwProject::addImage(Database, iconImageData);
    } else {
        imageId = id;
        cwProject::updateImage(Database, iconImageData, id);
    }

    return imageId;
}

using namespace squish;

/**
  This block is create to be compressed by squish in a thread way.

  See sqiush::CompressMask for details
  */
class Block {
public:

    /**
      \param x - The x parameter
      \param y - The y parameter
      \param rgba - The source rgba
      \param blockData - The output blockdata
      */
    Block(int x, int y, u8 const* rgba, void* blockData) {
        Position = QPoint(x, y);
        RGBA = rgba;
        BlockData = blockData;
    }

    QPoint Position;
    u8 const* RGBA;
    void* BlockData;
};

/**
  \brief This class compresses a signle block.  This allow squish library to be
  threaded.
  */
class CompressImageKernal {
public:
    CompressImageKernal(cwAddImageTask* task, QSize imageSize, int flags, float* metric) {
        Task = task;
        ImageSize = imageSize;
        Flags = flags;
        Metric = metric;
    }

    cwAddImageTask* Task;
    QSize ImageSize;
    int Flags;
    float* Metric;


    void operator()(Block block) {

        if(!Task->isRunning()) { return; }

        // build the 4x4 block of pixels
        u8 sourceRgba[16*4];
        u8* targetPixel = sourceRgba;
        int mask = 0;
        for( int py = 0; py < 4; ++py )
        {
            for( int px = 0; px < 4; ++px )
            {
                // get the source pixel in the image
                int sx = block.Position.x() + px;
                int sy = block.Position.y() + py;

                // enable if we're in the image
                if( sx < ImageSize.width() && sy < ImageSize.height() )
                {
                    // copy the rgba value
                    u8 const* sourcePixel = block.RGBA + 4*( ImageSize.width() * sy + sx );
                    for( int i = 0; i < 4; ++i ) {
                        *targetPixel++ = *sourcePixel++;
                    }

                    // enable this pixel
                    mask |= ( 1 << ( 4*py + px ) );
                }
                else
                {
                    // skip this pixel as its outside the image
                    targetPixel += 4;
                }
            }
        }

        CompressMasked(sourceRgba, mask, block.BlockData, Flags);

        emit Task->IncreaseProgress();
    }

};



/**
  Copied directly from squish library
  */

static int FixFlags( int flags )
{
    // grab the flag bits
    int method = flags & ( kDxt1 | kDxt3 | kDxt5 );
    int fit = flags & ( kColourIterativeClusterFit | kColourClusterFit | kColourRangeFit );
    int extra = flags & kWeightColourByAlpha;

    // set defaults
    if( method != kDxt3 && method != kDxt5 )
        method = kDxt1;
    if( fit != kColourRangeFit && fit != kColourIterativeClusterFit )
        fit = kColourClusterFit;

    // done
    return method | fit | extra;
}

/**
  \brief This is a drop in replacement for sqiush::CompressImage

  The only differance is this is threaded.
  */
QByteArray cwAddImageTask::squishCompressImageThreaded( QImage image, int flags, float* metric ) {
    int outputFileSize = squish::GetStorageRequirements(image.width(), image.height(), squish::kDxt1);

    //Allocate the compress data
    QByteArray outputData;
    outputData.resize(outputFileSize);

    //Convert the image to a real format
    QImage convertedFormat = QGLWidget::convertToGLFormat(image);

    // fix any bad flags
    flags = FixFlags( flags );

    // initialise the block output
    u8* targetBlock = reinterpret_cast< u8* >( outputData.data() );
    int bytesPerBlock = ( ( flags & kDxt1 ) != 0 ) ? 8 : 16;

    // loop over pixels and create blocks
    QList<Block> computeBlocks;
    for( int y = 0; y < image.height(); y += 4 )
    {
        for( int x = 0; x < image.width(); x += 4 )
        {
            computeBlocks.append(Block(x, y, convertedFormat.bits(), targetBlock));

            // advance
            targetBlock += bytesPerBlock;
        }
    }

    //This takes all the compute blocks and compresses them using squish
    QtConcurrent::blockingMap(computeBlocks, CompressImageKernal(this, image.size(), flags, metric));

    return outputData;
}

/**
 * @brief cwAddImageTask::graphicsDriverDx1Compression
 * @param image - The image to compress
 * @return Returns the compress image in DXT1 compression
 *
 * This assumes that the opengl context is bound
 */
//#ifndef Q_OS_WIN
QByteArray cwAddImageTask::openglDxt1Compression(QImage image)
{
    glBindTexture(GL_TEXTURE_2D, Texture);

    QImage convertedImage = QGLWidget::convertToGLFormat(image);

    glTexImage2D(GL_TEXTURE_2D,
                 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                 image.width(), image.height(),
                 0, GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 convertedImage.bits());


    GLint compressed;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_ARB, &compressed);
    // if the compression has been successful
    if (compressed == GL_TRUE)
    {
        //        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT,
        //                                 &internalformat);
        GLint compressed_size;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB,
                                 &compressed_size);
        QByteArray compressedByteArray(compressed_size, 0);
        glGetCompressedTexImage(GL_TEXTURE_2D, 0, compressedByteArray.data());

        return compressedByteArray;
    }

    qDebug() << "Error: Couldn't compress image" << LOCATION;
    return QByteArray();
}
//#endif


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


