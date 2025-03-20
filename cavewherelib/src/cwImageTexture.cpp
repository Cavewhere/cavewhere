/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwImageTexture.h"
#include "cwTextureUploadTask.h"
#include "cwDebug.h"

//QT includes
#include <QVector2D>
#include <QFuture>
#include <QWindow>

//Async future
#include <asyncfuture.h>

/**

  */
cwImageTexture::cwImageTexture(QObject *parent) :
    QObject(parent),
    TextureDirty(false),
    DeleteTexture(false),
    TextureId(0)
{
    // auto settings = cwOpenGLSettings::instance();

    setTextureType(cwTextureUploadTask::format());

    auto reloadTexture = [this]() {
        ReloadTexture = true;
        markAsDirty();
    };

    // connect(settings, &cwOpenGLSettings::useDXT1CompressionChanged, this, [this, reloadTexture]() {
    //     setTextureType(cwTextureUploadTask::format());
    //     startLoadingImage();
    //     reloadTexture();
    // });

    // connect(settings, &cwOpenGLSettings::useAnisotropyChanged, this, reloadTexture);
    // connect(settings, &cwOpenGLSettings::useMipmapsChanged, this, reloadTexture);
    // connect(settings, &cwOpenGLSettings::magFilterChanged, this, reloadTexture);
    // connect(settings, &cwOpenGLSettings::minFilterChanged, this, reloadTexture);
}

/**
 * @brief cwImageTexture::~cwImageTexture
 *
 * The deconstructor assumes that the current opengl context has
 * been set, and this object is being destroyed in the correct thread
 */
cwImageTexture::~cwImageTexture()
{
}

/**
  This initilizes the texture map in opengl
  */
void cwImageTexture::initialize()
{
    // initializeOpenGLFunctions();

    // glGenTextures(1, &TextureId);
    // glBindTexture(GL_TEXTURE_2D, TextureId);

    // auto toMinFilter = [](cwOpenGLSettings::MinFilter filter) {
    //     switch (filter) {
    //     case cwOpenGLSettings::MinLinear:
    //         return GL_LINEAR;
    //     case cwOpenGLSettings::MinNearest_Mipmap_Linear:
    //         return GL_NEAREST_MIPMAP_LINEAR;
    //     case cwOpenGLSettings::MinLinear_Mipmap_Linear:
    //         return GL_NEAREST_MIPMAP_LINEAR;
    //     }
    //     return GL_LINEAR;
    // };

    // auto toMagFilter = [](cwOpenGLSettings::MagFilter filter) {
    //     switch (filter) {
    //     case cwOpenGLSettings::MagNearest:
    //         return GL_NEAREST;
    //     case cwOpenGLSettings::MagLinear:
    //         return GL_LINEAR;
    //     }
    //     return GL_NEAREST;
    // };

    // auto settings = cwOpenGLSettings::instance();

    // if(settings->useMipmaps()) {
    //     glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, toMinFilter(settings->minFilter()));
    // } else {
    //     glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // }

    // if(settings->useAnisotropy()) {
    //     GLfloat fLargest;
    //     glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
    //     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargest);
    // }

    // glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, toMagFilter(settings->magFilter()));
    // glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    // glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    // glBindTexture(GL_TEXTURE_2D, 0);

}

void cwImageTexture::releaseResources()
{
    deleteGLTexture();
}

/**
Sets project
*/
void cwImageTexture::setProject(QString project) {
    if(ProjectFilename != project) {
        ProjectFilename = project;
        startLoadingImage();
        emit projectChanged();
    }
}

/**
 * @brief cwImageTexture::scaleTexCoords
 * @return  How the text are should be scaled
 */
QVector2D cwImageTexture::scaleTexCoords() const
{
    return ScaleTexCoords;
}

/**
Sets image
*/
void cwImageTexture::setImage(cwImage image) {
    if(Image != image) {
        Image = image;

        if(isImageValid(image)) {
            startLoadingImage();
        } else {
            DeleteTexture = true;
            markAsDirty();
        }

        emit imageChanged();
    }
}

/**
  This upload the results from texture image to the graphics card
  */
void cwImageTexture::updateData() {
    // if(!isDirty()) { return; }

    // if(DeleteTexture) {
    //     deleteGLTexture();
    //     TextureDirty = false;
    //     emit needsUpdate();
    //     return;
    // }

    // if(ReloadTexture) {
    //     deleteGLTexture();
    //     initialize();
    //     ReloadTexture = false;
    //     emit needsUpdate();
    //     return;
    // }

    // if(UploadedTextureFuture.isRunning()) {
    //     return;
    // }

    // if(UploadedTextureFuture.resultCount() == 0) {
    //     return;
    // }

    // auto results = UploadedTextureFuture.result();

    // QList<QPair<QByteArray, QSize> > mipmaps = results.mipmaps;
    // ScaleTexCoords = results.scaleTexCoords;

    // if(mipmaps.isEmpty()) { return; }

    // QSize firstLevel = mipmaps.first().second;
    // if(!cwTextureUploadTask::isDivisibleBy4(firstLevel) && results.type == cwTextureUploadTask::DXT1Mipmaps) {
    //     qDebug() << "Trying to upload an image that isn't divisible by 4. This will crash ANGLE on windows." << LOCATION;
    //     TextureDirty = false;
    //     return;
    // }

    // if(TextureId == 0) {
    //     initialize();
    // }

    // //Load the data into opengl
    // bind();

    // //Get the max texture size
    // GLint maxTextureSize;
    // glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

    // bool useMipmaps = cwOpenGLSettings::instance()->useMipmaps();

    // int trueMipmapLevel = 0;
    // for(int mipmapLevel = 0; mipmapLevel < mipmaps.size(); mipmapLevel++) {

    //     //Get the mipmap data
    //     QPair<QByteArray, QSize> image = mipmaps.at(mipmapLevel);
    //     QByteArray imageData = image.first;
    //     QSize size = image.second;

    //     Q_ASSERT(mipmaps.at(mipmapLevel - 1 > 0 ? mipmapLevel - 1 : 0).second.width() >= size.width());

    //     if(size.width() < maxTextureSize && size.height() < maxTextureSize) {
    //         switch(results.type) {
    //         case cwTextureUploadTask::DXT1Mipmaps:
    //             glCompressedTexImage2D(GL_TEXTURE_2D, trueMipmapLevel, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
    //                                    size.width(), size.height(), 0,
    //                                    imageData.size(), imageData.data());
    //             break;
    //         case cwTextureUploadTask::OpenGL_RGBA:
    //             Q_ASSERT_X(!imageData.isEmpty(), LOCATION_STR, "Image can't be empty, this will cause crashing");
    //             glTexImage2D(GL_TEXTURE_2D, trueMipmapLevel, GL_RGBA,
    //                          size.width(), size.height(),
    //                          0, GL_RGBA, GL_UNSIGNED_BYTE,
    //                          imageData.data());
    //             Q_ASSERT_X(mipmaps.size() == 1, LOCATION_STR, "There should only be one mipmap for GL_RGBA types");

    //             if(useMipmaps) {
    //                 glGenerateMipmap(GL_TEXTURE_2D);
    //             }
    //             break;
    //         }

    //         trueMipmapLevel++;

    //         if(!useMipmaps) {
    //             break;
    //         }
    //     }
    // }

    // release();

    // emit needsUpdate();
    // TextureDirty = false;
}

/**
 * @brief cwImageTexture::startLoadingImage
 *
 * Loads the image into the graphics card
 */
void cwImageTexture::startLoadingImage()
{
    if(Image.isOriginalValid() && !project().isEmpty()) {

        UploadedTextureFuture.cancel();

        cwTextureUploadTask uploadTask;
        uploadTask.setImage(image());
        uploadTask.setProjectFilename(ProjectFilename);
        uploadTask.setType(TextureType);
        UploadedTextureFuture = uploadTask.mipmaps();
        FutureManagerToken.addJob(cwFuture(QFuture<void>(UploadedTextureFuture), "Updating Texture"));

        AsyncFuture::observe(UploadedTextureFuture).subscribe([this](){
            markAsDirty();
            emit textureUploaded();
            emit needsUpdate();
        });

        DeleteTexture = false;
    }
}

/**
 * @brief cwImageTexture::deleteGLTexture
 *
 * This deletes the image texture
 */
void cwImageTexture::deleteGLTexture()
{
    // if(TextureId > 0) {
    //     if(QOpenGLContext::currentContext()) {
    //         glDeleteTextures(0, &TextureId);
    //     }
    //     TextureId = 0;
    //     DeleteTexture = false;
    //     markAsDirty();
    // }
}

void cwImageTexture::setTextureType(cwTextureUploadTask::Format type)
{
    if(type != TextureType) {
        TextureType = type;
        startLoadingImage();
    }
}

bool cwImageTexture::isImageValid(const cwImage &image) const
{
    // switch(TextureType) {
    // case cwTextureUploadTask::DXT1Mipmaps:
    //     Q_ASSERT(cwOpenGLSettings::instance()->useDXT1Compression());
    //     return image.isMipmapsValid();
    // case cwTextureUploadTask::OpenGL_RGBA:
    //     Q_ASSERT(!cwOpenGLSettings::instance()->useDXT1Compression());
    //     return image.isOriginalValid();
    // }
    // Q_ASSERT(false);
    return false;
}

void cwImageTexture::markAsDirty()
{
    TextureDirty = true;
    emit needsUpdate();
}

/**
  This binds the texture to current texture unit
  */
void cwImageTexture::bind() {
    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, TextureId);
}

/**
    Releases the texture
  */
void cwImageTexture::release() {
    // glBindTexture(GL_TEXTURE_2D, 0);
}

/**
*
*/
void cwImageTexture::setFutureManagerToken(cwFutureManagerToken futureManagerToken) {
    if(FutureManagerToken != futureManagerToken) {
        FutureManagerToken = futureManagerToken;
        emit futureManagerTokenChanged();
    }
}
