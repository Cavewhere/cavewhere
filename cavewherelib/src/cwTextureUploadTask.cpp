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
#include "cwOpenGLUtils.h"
#include "cwImageDatabase.h"
#include "cwConcurrent.h"

//Qt includes
#include <QDebug>
#include <QWindow>

//Std includes
#include <math.h>

//Async Future
#include "asyncfuture.h"

cwTextureUploadTask::cwTextureUploadTask()
{
}

QFuture<cwTextureUploadTask::UploadResult> cwTextureUploadTask::mipmaps() const
{
    auto projectFile = ProjectFilename;
    auto image = Image;
    auto currentFormat = type;

    //loads valid mipmap
    auto loadValidMipmap = [projectFile, currentFormat](const cwTrackedImagePtr& image) {
        UploadResult results;

        // if(!cwImageDatabase(projectFile).mipmapsValid(*image, currentFormat == DXT1Mipmaps)) {
        //     return results;
        // }

        cwImageProvider imageProvidor;
        imageProvidor.setProjectPath(projectFile);

        auto loadDXT1Mipmap = [&imageProvidor, image]() {
            QList< QPair< QByteArray, QSize > > mipmaps;
            //Load all the mipmaps
            foreach(int imageId, image->mipmaps()) {
                auto imageData = imageProvidor.data(imageId);
                imageProvidor.data(imageId, true);
                mipmaps.append(QPair< QByteArray, QSize >(imageData.data(), imageData.size()));
            }
            return mipmaps;
        };

        auto loadRGB = [&imageProvidor, image]()->QImage {
            auto imageData = imageProvidor.data(image->path());

            if(imageData.data().isEmpty()) {
                return {};
            }

            QImage qimage = cwOpenGLUtils::toGLTexture(imageProvidor.image(imageData));
            qDebug() << "Loaded:" << image->path() << QFileInfo::exists(image->path()) << qimage.size();

            //We might want convert the image here to prevent Rhi from having to covert it

            return qimage;
            // if(!image.isNull()) {
            //     image = cwOpenGLUtils::toGLTexture(image);
            //     QByteArray data(reinterpret_cast<char*>(image.bits()),
            //                     static_cast<int>(image.sizeInBytes()));
            //     return {{data, imageData.size()}};
            // }
            // qDebug() << "Couldn't load from imageData";
            // return {};
        };

        switch(currentFormat) {
        case OpenGL_RGBA:
            results.scaleTexCoords = QVector2D(1.0, 1.0); //No extra padding
            results.image = loadRGB();
            break;
        // case DXT1Mipmaps:
        //     results.scaleTexCoords = imageProvidor.scaleTexCoords(*image);
        //     results.mipmaps = loadDXT1Mipmap();
        //     break;
        default:
            Q_ASSERT(false);
        }

        results.type = currentFormat;
        return results;
    };

    return cwConcurrent::run(std::bind(loadValidMipmap,
                                       cwTrackedImage::createShared(image,
                                                                    projectFile,
                                                                    cwTrackedImage::NoOwnership)));

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
 * This function is not Thread safe, make sure use correctly
 */
cwTextureUploadTask::Format cwTextureUploadTask::format()
{
    // if(cwOpenGLSettings::instance()->useDXT1Compression()) {
    //     return cwTextureUploadTask::DXT1Mipmaps;
    // }
    return cwTextureUploadTask::OpenGL_RGBA;
}
