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
#include "cwOpenGLSettings.h"

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
    auto context = this->context();

    //loads valid mipmap
    auto loadValidMipmap = [projectFile, currentFormat](const cwImage& image) {
        cwImageProvider imageProvidor;
        imageProvidor.setProjectPath(projectFile);

        UploadResult results;

        auto loadDXT1Mipmap = [&imageProvidor, image]() {
            QList< QPair< QByteArray, QSize > > mipmaps;
            //Load all the mipmaps
            foreach(int imageId, image.mipmaps()) {
                imageProvidor.data(imageId, true);

                QSize imageSize;
                QByteArray imageData = imageProvidor.requestImageData(imageId, &imageSize);
                mipmaps.append(QPair< QByteArray, QSize >(imageData, imageSize));
            }
            return mipmaps;
        };

        auto loadRGB = [&imageProvidor, image]()->QList< QPair< QByteArray, QSize > > {
            auto imageData = imageProvidor.data(image.original());

            if(imageData.data().isEmpty()) {
                return {};
            }

            QImage image;
            bool loadedOkay = image.loadFromData(imageData.data());
            if(loadedOkay) {
                image = cwOpenGLUtils::toGLTexture(image);
                QByteArray data(reinterpret_cast<char*>(image.bits()),
                                static_cast<int>(image.sizeInBytes()));
                return {{data, imageData.size()}};
            }
            qDebug() << "Couldn't load from imageData";
            return {};
        };

        switch(currentFormat) {
        case OpenGL_RGBA:
            results.scaleTexCoords = QVector2D(1.0, 1.0); //No extra padding
            results.mipmaps = loadRGB();
            break;
        case DXT1Mipmaps:
            results.scaleTexCoords = imageProvidor.scaleTexCoords(image);
            results.mipmaps = loadDXT1Mipmap();
            break;
        }

        results.type = currentFormat;
        return results;
    };

    //fetchs the mipmaps
    auto fetchImages = [loadValidMipmap, projectFile, image, context, currentFormat]() {
        //Fetch mimaps from disk

        if(currentFormat == DXT1Mipmaps) {
            auto size = [image, projectFile]() {
                if(image.mipmaps().isEmpty()) {
                    return QSize();
                }

                int firstLevel = image.mipmaps().first();
                cwImageProvider imageProvidor;
                imageProvidor.setProjectPath(projectFile);
                return imageProvidor.data(firstLevel, true).size();
            };

            if(!isDivisibleBy4(size())) {
                //Regenerate the mipmaps
                cwAddImageTask addImageTask;
                addImageTask.setDatabaseFilename(projectFile);
                addImageTask.setRegenerateMipmapsOn(image);
                addImageTask.setContext(context);
                addImageTask.setFormatType(currentFormat);
                auto reAddFuture = addImageTask.images();

                return AsyncFuture::observe(reAddFuture)
                        .context(context, [projectFile, reAddFuture, loadValidMipmap]() {
                    //Double check if it successful
                    cwImageProvider imageProvidor;
                    imageProvidor.setProjectPath(projectFile);

                    auto image = reAddFuture.result();
                    int firstLevel = image.mipmaps().first();
                    QSize firstLevelSize = imageProvidor.data(firstLevel, true).size();
                    if(!isDivisibleBy4(firstLevelSize)) {
                        //No original image, can regenerate image
                        return AsyncFuture::completed(UploadResult());
                    }

                    return QtConcurrent::run(std::bind(loadValidMipmap, reAddFuture));
                }).future();
            }
        }

        return QtConcurrent::run(std::bind(loadValidMipmap, image));
    };

    return QtConcurrent::run(fetchImages);
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
    if(cwOpenGLSettings::instance()->useDXT1Compression()) {
        return cwTextureUploadTask::DXT1Mipmaps;
    }
    return cwTextureUploadTask::OpenGL_RGBA;
}
