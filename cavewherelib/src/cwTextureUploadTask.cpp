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
#include "cwMath.h"
#include "cwOpenGLUtils.h"
#include "cwImageDatabase.h"
#include "cwConcurrent.h"
#include "cwTrackedImage.h"

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
    auto image = Image;
    auto currentFormat = type;
    QDir dataRootDir = DataRootDir;

    //loads valid mipmap
    auto loadValidMipmap = [currentFormat, dataRootDir](const cwTrackedImagePtr& image) {
        UploadResult results;

        cwImageProvider imageProvidor;
        imageProvidor.setDataRootDir(dataRootDir);

        auto loadRGB = [&imageProvidor, image]()->QImage {
            auto imageData = imageProvidor.data(image->path());

            if(imageData.data().isEmpty()) {
                qWarning() << "Image data is empty:" << image->path() << LOCATION;
                return {};
            }

            QImage qimage = cwOpenGLUtils::toGLTexture(imageProvidor.image(imageData));

            //We might want convert the image here to prevent Rhi from having to covert it
            return qimage;
        };

        switch(currentFormat) {
        case OpenGL_RGBA:
            results.scaleTexCoords = QVector2D(1.0, 1.0); //No extra padding
            results.image = loadRGB();
            break;
        default:
            Q_ASSERT(false);
        }

        results.type = currentFormat;
        return results;
    };

    return cwConcurrent::run(std::bind(loadValidMipmap,
                                       cwTrackedImage::createShared(image,
                                                                    image.path(),
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
    return cwTextureUploadTask::OpenGL_RGBA;
}

void cwTextureUploadTask::setDataRootDir(const QDir& dataRootDir)
{
    DataRootDir = dataRootDir;
}
