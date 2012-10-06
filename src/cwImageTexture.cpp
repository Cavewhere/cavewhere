//Our includes
#include "cwImageTexture.h"
#include "cwProjectImageProvider.h"

//QT includes
#include <QtConcurrentRun>
#include <QtConcurrentMap>

/**

  */
cwImageTexture::cwImageTexture(QObject *parent) :
    QObject(parent),
    TextureDirty(false),
    TextureId(0),
    LoadNoteWatcher(NULL)
{
    reinitilizeLoadNoteWatcher();
}

cwImageTexture::~cwImageTexture()
{
    //FIXME: Texture needs to be delete when object is destroyed, this cause a crash on exit
    //    if(TextureId != 0) {
//        glDeleteTextures(1, &TextureId);
//    }
}

/**
  This initilizes the texture map in opengl
  */
void cwImageTexture::initialize()
{
    glGenTextures(1, &TextureId);
    glBindTexture(GL_TEXTURE_2D, TextureId);
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
    glBindTexture(GL_TEXTURE_2D, 0);
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
Sets image
*/
void cwImageTexture::setImage(cwImage image) {
    if(Image != image) {
        Image = image;
        startLoadingImage();
        emit imageChanged();
    }
}

/**
  This upload the results from texture image to the graphics card
  */
void cwImageTexture::updateData() {
    if(!isDirty()) { return; }

    QList<QPair<QByteArray, QSize> >mipmaps = LoadNoteWatcher->future().results();

    if(mipmaps.empty()) { return; }

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
#ifdef WIN32
            glCompressedTexImage2DARB(GL_TEXTURE_2D, trueMipmapLevel, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                                   size.width(), size.height(), 0,
                                   imageData.size(), imageData.data());
#else
            glCompressedTexImage2D(GL_TEXTURE_2D, trueMipmapLevel, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                                   size.width(), size.height(), 0,
                                   imageData.size(), imageData.data());
#endif
            trueMipmapLevel++;
        }
    }

    release();

    reinitilizeLoadNoteWatcher();

    TextureDirty = false;
}

/**
 * @brief cwImageTexture::startLoadingImage
 *
 * Loads the image into the graphics card
 */
void cwImageTexture::startLoadingImage()
{
    if(Image.isValid() && !project().isEmpty()) {
        //Load the notes in an asyn way
        LoadNoteWatcher->cancel(); //Cancel previous run, if still running
        QFuture<QPair<QByteArray, QSize> > future = QtConcurrent::mapped(Image.mipmaps(), LoadImage(ProjectFilename));
        LoadNoteWatcher->setFuture(future);
    }
}

/**
 * @brief cwImageTexture::reinitilizeLoadNoteWatcher
 */
void cwImageTexture::reinitilizeLoadNoteWatcher()
{
    if(LoadNoteWatcher != NULL) {
        //Clean up the memory
        LoadNoteWatcher->deleteLater();
    }

    LoadNoteWatcher = new QFutureWatcher<QPair<QByteArray, QSize> >(this);
    connect(LoadNoteWatcher, &QFutureWatcher<QPair<QByteArray, QSize> >::finished, this, &cwImageTexture::markAsDirty);
    connect(LoadNoteWatcher, &QFutureWatcher<QPair<QByteArray, QSize> >::finished, this, &cwImageTexture::textureUploaded);
}

void cwImageTexture::markAsDirty()
{
    TextureDirty = true;
}

/**
  \brief Allow QtConturrent to create a QImage

  This returns a opengl formatted image
  */
QPair<QByteArray, QSize> cwImageTexture::LoadImage::operator ()(int imageId) {
    //Extract the image data from the imageProvider
    cwProjectImageProvider imageProvidor;
    imageProvidor.setProjectPath(Filename);

    QSize size;
    QByteArray imageData = imageProvidor.requestImageData(imageId, &size);

    return QPair<QByteArray, QSize>(imageData, size);
}



