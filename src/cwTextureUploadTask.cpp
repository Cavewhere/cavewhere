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

    ScaleTexCoords = imageProvidor.scaleTexCoords(Image);

    QSize imageSize;
    //Load all the mipmaps
    foreach(int imageId, Image.mipmaps()) {
        if(!isRunning()) { return; }
        imageProvidor.data(imageId, true);

        QByteArray imageData = imageProvidor.requestImageData(imageId, &imageSize);
        mipmaps.append(QPair< QByteArray, QSize >(imageData, imageSize));
    }

    Mipmaps = mipmaps;

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
