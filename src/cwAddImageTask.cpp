//Our includes
#include "cwAddImageTask.h"
#include "cwProject.h"
#include "cwImageData.h"
#include "cwProjectImageProvider.h"
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

//TODO: REMOVE for testing only
#include <QFile>
#include <QTime>

//Sqlite lite includes
#include "sqlite3.h"

//Zlib includes
#include <zlib.h>

cwAddImageTask::cwAddImageTask(QObject* parent) : cwProjectIOTask(parent)
{
    CompressionContext = new QOpenGLContext(this);
    Window = new QWindow();
    Window->setSurfaceType(QSurface::OpenGLSurface);
    Window->create();
    Texture = 0;

    MipmapOnly = false;
//    ComperssionContext->create();

//    qDebug() << "Context created: " << GLWindow->context();
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

    bool couldCreate = CompressionContext->create();
//    qDebug() << "Could create context:" << couldCreate;

    bool couldMakeCurrent = CompressionContext->makeCurrent(Window);
//    qDebug() << "Could make curernt: " << couldMakeCurrent;

    glEnable(GL_TEXTURE_2D);

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
    }

    CompressionContext->doneCurrent();

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
        QSize imageSize = reader.size();
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
    bool good = beginTransation(SLOT(tryAddingImagesToDatabase()));
    if(!good) { return; }

    //Database image, original image
    QList< PrivateImageData > images;

    //Go through all the images strings
    for(int i = 0; i < NewImagePaths.size() && isRunning(); i++) {
        QString imagePath = NewImagePaths[i];

        //Where the database image ideas are stored
        cwImage imageIds;

        //Copy the original image to the database
        QImage originalImage = copyOriginalImage(imagePath, &imageIds);

        images.append(PrivateImageData(imageIds, originalImage, imagePath));
    }

    //Go through all the images
    for(int i = 0; i < NewImages.size() && isRunning(); i++) {
        QImage originalImage = NewImages[i];

        //Where the database image ideas are stored
        cwImage imageIds;

        copyOriginalImage(originalImage, &imageIds);

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

    endTransation();
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
void cwAddImageTask::createMipmaps(QImage originalImage, QString imageFilename, cwImage* imageIds) {
    int numberOfLevels = numberOfMipmapLevels(originalImage.size());

    QImage scaledImage = originalImage;
    QList<int> mipmapIds;

    QSize scaledImageSize = scaledImage.size();

    for(int i = 0; i < numberOfLevels && isRunning(); i++) {
        emit statusMessage(QString("Compressing %1 of %2 bold flavors of %3").arg(i + 1).arg(numberOfLevels).arg(QFileInfo(imageFilename).fileName()));

        //Rescaled the image
        scaledImage = scaledImage.scaled(scaledImageSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        //Export the image to DXT1 format
        int mipmapId = saveToDXT1Format(scaledImage);

        //Add the path to the mipmapPath
        mipmapIds.append(mipmapId);

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
  */
int cwAddImageTask::saveToDXT1Format(QImage image) {
    //Convert and compress using dxt1
    //20 times slower on my computer
    //QByteArray outputData1 = squishCompressImageThreaded(image, squish::kDxt1 | squish::kColourIterativeClusterFit);

    QByteArray outputData = openglDxt1Compression(image);

    if(outputData.isEmpty()) {
        return -1;
    }

    //Compress the dxt1FileData using zlib
    outputData = qCompress(outputData, 9);

    //Add the image to the database
    cwImageData iconImageData(image.size(), 0, cwProjectImageProvider::Dxt1_GZ_Extension, outputData);
    int imageId = cwProject::addImage(Database, iconImageData);

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



