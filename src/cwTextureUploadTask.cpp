//Our includes
#include "cwImageProvider.h"
#include "cwTextureUploadTask.h"
#include "cwDebug.h"
#include "cwAddImageTask.h"
#include "cwMath.h"

//Qt includes
#include <QDebug>
#include <QWindow>
#include <QThread>

//Std includes
#include <math.h>

cwTextureUploadTask::cwTextureUploadTask(QObject *parent) :
    cwTask(parent)
{
}

/**
 * @brief cwTextureUploadTask::runTask
 *
 * This will fetch the texture's from disk, as well as, upload them
 * to the shared OpenGL context. This can be used in a theaded manner
 * to prevent the rendering thread from blocking
 */
void cwTextureUploadTask::runTask()
{
    //Fetch mimaps from disk
    loadMipmapsFromDisk();

    done();
}

/**
 * @brief cwTextureUploadTask::loadMipmapsFromDisk
 * @return
 */
void cwTextureUploadTask::loadMipmapsFromDisk()
{
    Mipmaps.clear();
    if(Image.mipmaps().empty()) { return; }

    //Fetch mimaps from disk
    cwImageProvider imageProvidor;
    imageProvidor.setProjectPath(ProjectFilename);

    QList< QPair< QByteArray, QSize > > mipmaps;

    int firstLevel = Image.mipmaps().first();
    QSize firstLevelSize = imageProvidor.data(firstLevel, true).size();

    if(!isDivisibleBy4(firstLevelSize)) {
        //Regenerate the mipmaps
        cwAddImageTask addImageTask;
        addImageTask.setDatabaseFilename(ProjectFilename);
        addImageTask.regenerateMipmapsOn(Image);
        addImageTask.start();

        //Double check if it successful
        int firstLevel = Image.mipmaps().first();
        firstLevelSize = imageProvidor.data(firstLevel, true).size();
        if(!isDivisibleBy4(firstLevelSize)) {
            //No original image, can regenerate image
            return;
        }
    }

//    ScaleTexCoords = imageProvidor.scaleTexCoords(Image);

    QSize imageSize;
    //Load all the mipmaps
    foreach(int imageId, Image.mipmaps()) {
        if(!isRunning()) { return; }
        imageProvidor.data(imageId, true);

        QByteArray imageData = imageProvidor.requestImageData(imageId, &imageSize);
        mipmaps.append(QPair< QByteArray, QSize >(imageData, imageSize));
    }

    Mipmaps = mipmaps;

    //Make sure that the mipmaps are powers of two
//    ensurePowerOfTwo();

    updateScaleTexCoords();

    for(int i = 0; i < Mipmaps.size(); i++) {
        qDebug() << "BytemapSize: " << Mipmaps.at(i).first.size() << Mipmaps.at(i).second;
    }
}

/**
 * @brief cwTextureUploadTask::ensurePowerOfTwo
 *
 * This ensures that the texture and mipmaps are of a power of two.  This
 * is nessary for some computers (mostly intel sucky graphics cards)  That don't
 * support power of two textures with dxt1
 */
void cwTextureUploadTask::ensurePowerOfTwo()
{
    for(int i = 0; i < Mipmaps.size(); i++) {
        ensurePowerOfTwo(Mipmaps[i]);
    }
}

/**
 * @brief cwTextureUploadTask::ensurePowerOfTwo
 *
 * This ensures that the texture and mipmaps are of a power of two.  This
 * is nessary for some computers (mostly intel sucky graphics cards)  That don't
 * support power of two textures with dxt1
 *
 * This will modify mipmap in place.  This will not change the mipmap if the mipmap
 * is already a power of two in both directions.
 */
void cwTextureUploadTask::ensurePowerOfTwo(QPair<QByteArray, QSize> &mipmap)
{
//    Q_ASSERT(isDivisibleBy4(mipmap.second));

    static const int blockSize = 8; //8 Bytes for DXT1 compression
    static const int blockPixelSize = 4; //4 pixels in each dimension

    QSize size = mipmap.second;
    QByteArray image = mipmap.first;
    int xBlocks = (int)ceil(size.width() / (double)blockPixelSize);
    int yBlocks = (int)ceil(size.height() / (double)blockPixelSize);
    int numberOfBlocks = xBlocks * yBlocks;

    //Ensure that the mipmap is the correct size
    qDebug() << "BlockSize:" << numberOfBlocks * blockSize << image.size();
    Q_ASSERT(numberOfBlocks * blockSize == image.size());

    int newWidth = pow(2,ceil(log2(size.width())));
    int newHeight = pow(2, ceil(log2(size.height())));

    QSize newSize(newWidth, newHeight);

    qDebug() << "NewSize:" << newSize << size;

    if(size != newSize) {

        int sizeInBytes = (newWidth / blockPixelSize) * (newHeight / blockPixelSize) * blockSize;
        QByteArray newImage;
        newImage.resize(sizeInBytes);

        //Start copying blocks from one array to another
        const char* imageData = image.constData();
        char* newImageData = newImage.data();

        int oldRowBytes = size.width() / blockPixelSize * blockSize;
        int newRowBytes = newSize.width() / blockPixelSize * blockSize;

        for(int row = 0; row < yBlocks; row++) {
             memcpy(&(newImageData[row * newRowBytes]), &(imageData[row * oldRowBytes]), oldRowBytes);
        }

        mipmap.first = newImage;
        mipmap.second = newSize;
    }
}

/**
 * @brief cwTextureUploadTask::updateScaleTexCoords
 */
void cwTextureUploadTask::updateScaleTexCoords()
{
    if(Mipmaps.isEmpty()) {
        ScaleTexCoords = QVector2D(1.0, 1.0);
        return;
    }

    cwImageProvider imageProvider;
    imageProvider.setProjectPath(ProjectFilename);

    QSize originalSize = imageProvider.data(Image.original(), true).size();
    QSize firstMipmapSize = Mipmaps.first().second;

    qDebug() << "OriginalSize: " << Image.original() << originalSize << firstMipmapSize;

    ScaleTexCoords = QVector2D(originalSize.width() / (double)firstMipmapSize.width(),
                     originalSize.height() / (double)firstMipmapSize.height());
}



/**
 * @brief cwImageTexture::isDivisibleBy4
 * @param size
 * @return True if the size is divisible by 4
 *
 * This is to prevent D3D from crashing when using ANGLE
 */
bool cwTextureUploadTask::isDivisibleBy4(QSize size)
{
    int widthRemainder = size.width() % 4;
    int heightRemainder = size.height() % 4;
    return widthRemainder == 0 && heightRemainder == 0;
}

/**
 * @brief cwTextureUploadTask::mipmaps
 * @return
 */
QList<QPair<QByteArray, QSize> > cwTextureUploadTask::mipmaps() const
{
    if(isReady()) {
        return Mipmaps;
    }

    qDebug() << "Mipmaps aren't ready" << status() << LOCATION;
    return QList<QPair<QByteArray, QSize> >();
}

QVector2D cwTextureUploadTask::scaleTexCoords() const
{
    if(isReady()) {
        return ScaleTexCoords;
    }

    qDebug() << "ScaleTexCoords aren't ready" << status() << LOCATION;
    return QVector2D(1.0, 1.0);
}
