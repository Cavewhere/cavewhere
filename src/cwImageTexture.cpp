/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwImageTexture.h"
#include "cwImageProvider.h"
#include "cwTextureUploadTask.h"
#include "cwDebug.h"

//QT includes
#include <QtConcurrentRun>
#include <QtConcurrentMap>
#include <QVector2D>
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
}

/**
 * @brief cwImageTexture::~cwImageTexture
 *
 * The deconstructor assumes that the current opengl context has
 * been set, and this object is being destroyed in the correct thread
 */
cwImageTexture::~cwImageTexture()
{
    deleteGLTexture();
}

/**
  This initilizes the texture map in opengl
  */
void cwImageTexture::initialize()
{
    initializeOpenGLFunctions();

    glGenTextures(1, &TextureId);
    glBindTexture(GL_TEXTURE_2D, TextureId);

#ifdef Q_OS_WIN
    //Only upload one texture, GL_LINEAR, because some intel cards,
    //don't support npot dxt1 copression, so we just used GL_LINEAR
    //FIXME: ADD to rendering settings! Use mipmaps.
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
#else
    //All other platforms
    GLfloat fLargest;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);

    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargest);
#endif
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glBindTexture(GL_TEXTURE_2D, 0);

    updateTextureType();
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

        if(Image.isMipmapsValid()) {
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
    if(!isDirty()) { return; }

    if(DeleteTexture) {
        deleteGLTexture();
        TextureDirty = false;
        return;
    }

    if(UploadedTextureFuture.isRunning()) {
        return;
    }

    auto results = UploadedTextureFuture.result();

    QList<QPair<QByteArray, QSize> > mipmaps = results.mipmaps;
    ScaleTexCoords = results.scaleTexCoords;

    if(mipmaps.empty()) { return; }

    QSize firstLevel = mipmaps.first().second;
    if(!cwTextureUploadTask::isDivisibleBy4(firstLevel) && results.type == cwTextureUploadTask::DXT1Mipmaps) {
        qDebug() << "Trying to upload an image that isn't divisible by 4. This will crash ANGLE on windows." << LOCATION;
        TextureDirty = false;
        return;
    }

    if(TextureId == 0) {
        initialize();
    }

    //Load the data into opengl
    bind();

    //Get the max texture size
    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

    int trueMipmapLevel = 0;
    for(int mipmapLevel = 0; mipmapLevel < mipmaps.size(); mipmapLevel++) {

        //Get the mipmap data
        QPair<QByteArray, QSize> image = mipmaps.at(mipmapLevel);
        QByteArray imageData = image.first;
        QSize size = image.second;

        if(size.width() < maxTextureSize && size.height() < maxTextureSize) {
            qDebug() << "Uploading level:" << trueMipmapLevel << size;
            switch(results.type) {
            case cwTextureUploadTask::DXT1Mipmaps:
                glCompressedTexImage2D(GL_TEXTURE_2D, trueMipmapLevel, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                                       size.width(), size.height(), 0,
                                       imageData.size(), imageData.data());
                break;
            case cwTextureUploadTask::OpenGL_RGBA:
                Q_ASSERT_X(!imageData.isEmpty(), LOCATION_STR, "Image can't be empty, this will cause crashing");
                glTexImage2D(GL_TEXTURE_2D, trueMipmapLevel, GL_RGBA,
                             size.width(), size.height(),
                             0, GL_RGBA, GL_UNSIGNED_BYTE,
                             imageData.data());
                Q_ASSERT_X(mipmaps.size() == 1, LOCATION_STR, "There should only be one mipmap for GL_RGBA types");
                glGenerateMipmap(GL_TEXTURE_2D);
                break;
            }

            trueMipmapLevel++;

#ifdef Q_OS_WIN
            //Only upload one texture, because some intel cards, don't support npot dxt1 copression, so we just used nearest
            //FIXME: ADD to rendering settings!
            break;
#endif //Q_OS_WIN
        }
    }

    release();

    TextureDirty = false;
}

/**
 * @brief cwImageTexture::startLoadingImage
 *
 * Loads the image into the graphics card
 */
void cwImageTexture::startLoadingImage()
{
    if(Image.isMipmapsValid() && !project().isEmpty()) {

        UploadedTextureFuture.cancel();

        cwTextureUploadTask uploadTask;
        uploadTask.setImage(image());
        uploadTask.setProjectFilename(ProjectFilename);
        uploadTask.setType(TextureType);
        UploadedTextureFuture = QtConcurrent::run(uploadTask);

        auto context = QSharedPointer<QObject>::create();

        AsyncFuture::observe(UploadedTextureFuture).context(context.get(), [context, this](){
            Q_ASSERT(context->thread() == QThread::currentThread());
            markAsDirty();
            emit textureUploaded();
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
    if(TextureId > 0) {
        glDeleteTextures(0, &TextureId);
        TextureId = 0;
        DeleteTexture = false;
        markAsDirty();
    }
}

void cwImageTexture::updateTextureType()
{
    auto context = QOpenGLContext::currentContext();
    if(context->hasExtension("GL_EXT_texture_compression_dxt1")) {
        setTextureType(cwTextureUploadTask::DXT1Mipmaps);
    } else {
        setTextureType(cwTextureUploadTask::OpenGL_RGBA);
    }
}

void cwImageTexture::setTextureType(cwTextureUploadTask::Type type)
{
    if(type != TextureType) {
        TextureType = type;
        startLoadingImage();
    }
}

void cwImageTexture::markAsDirty()
{
    TextureDirty = true;
}

/**
  This binds the texture to current texture unit
  */
void cwImageTexture::bind() {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, TextureId);
}

/**
    Releases the texture
  */
void cwImageTexture::release() {
    glBindTexture(GL_TEXTURE_2D, 0);
}
