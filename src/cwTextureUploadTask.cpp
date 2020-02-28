/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwImageProvider.h"
#include "cwTextureUploadTask.h"
#include "cwDebug.h"
#include "cwAddImageTask.h"
#include "cwMath.h"

//Qt includes
#include <QDebug>
#include <QWindow>
#include <QGLWidget>

//Std includes
#include <math.h>

cwTextureUploadTask::cwTextureUploadTask()
{
}

cwTextureUploadTask::UploadResult cwTextureUploadTask::operator()() const
{
    if(Image.mipmaps().empty()) { return UploadResult(); }

    //Fetch mimaps from disk
    cwImageProvider imageProvidor;
    imageProvidor.setProjectPath(ProjectFilename);

    int firstLevel = Image.mipmaps().first();
    QSize firstLevelSize = imageProvidor.data(firstLevel, true).size();

    if(!isDivisibleBy4(firstLevelSize)) {
        //Regenerate the mipmaps
        cwAddImageTask addImageTask;
        addImageTask.setUsingThreadPool(false);
        addImageTask.setDatabaseFilename(ProjectFilename);
        addImageTask.regenerateMipmapsOn(Image);
        addImageTask.start();

        //Double check if it successful
        int firstLevel = Image.mipmaps().first();
        firstLevelSize = imageProvidor.data(firstLevel, true).size();
        if(!isDivisibleBy4(firstLevelSize)) {
            //No original image, can regenerate image
            return UploadResult();
        }
    }

    UploadResult results;

    results.scaleTexCoords = imageProvidor.scaleTexCoords(Image);

    auto loadDXT1Mipmap = [this, &imageProvidor]() {
        QList< QPair< QByteArray, QSize > > mipmaps;
        //Load all the mipmaps
        foreach(int imageId, Image.mipmaps()) {
            imageProvidor.data(imageId, true);

            QSize imageSize;
            QByteArray imageData = imageProvidor.requestImageData(imageId, &imageSize);
            mipmaps.append(QPair< QByteArray, QSize >(imageData, imageSize));
        }
        return mipmaps;
    };

    auto loadRGB = [this, &imageProvidor]()->QList< QPair< QByteArray, QSize > > {
        QList< QPair< QByteArray, QSize > > mipmaps;
        auto imageData = imageProvidor.data(Image.original());

        if(imageData.data().isEmpty()) {
            return {};
        }

        QImage image;
        bool loadedOkay = image.loadFromData(imageData.data());
        if(loadedOkay) {
            image = image.convertToFormat(QImage::Format_RGBA8888).mirrored();
            QByteArray data(reinterpret_cast<char*>(image.bits()),
                            static_cast<int>(image.sizeInBytes()));
            return {{data, imageData.size()}};
        }
        return {};
    };

    switch(type) {
    case OpenGL_RGBA:
        results.scaleTexCoords = QVector2D(1.0, 1.0); //No extra padding
        results.mipmaps = loadRGB();
        break;
    case DXT1Mipmaps:
        results.scaleTexCoords = imageProvidor.scaleTexCoords(Image);
        results.mipmaps = loadDXT1Mipmap();
    }

    results.type = type;
    return results;
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
