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
    TextureId(0),
    LoadNoteWatcher(new QFutureWatcher<QPair<QByteArray, QSize> >(this))
{
    connect(LoadNoteWatcher, SIGNAL(finished()), SLOT(uploadImageToGraphicsCard()));
}

cwImageTexture::~cwImageTexture()
{
    if(TextureId != 0) {
        glDeleteTextures(1, &TextureId);
    }
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
        emit projectChanged();
    }
}

/**
Sets image
*/
void cwImageTexture::setImage(cwImage image) {
    if(Image != image) {
        Image = image;

        //If texture hasn't been initiziled
        if(TextureId == 0) {
            initialize();
        }

        //Load the notes in an asyn way
        LoadNoteWatcher->cancel(); //Cancel previous run, if still running
        QFuture<QPair<QByteArray, QSize> > future = QtConcurrent::mapped(Image.mipmaps(), LoadImage(ProjectFilename));
        LoadNoteWatcher->setFuture(future);

        emit imageChanged();
    }
}

/**
  This upload the results from texture image to the graphics card
  */
void cwImageTexture::uploadImageToGraphicsCard() {
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

        qDebug() << "DXT1 size: " << size.width() << size.height() << imageData.size() << imageData.size() % 8 << glGetError();
//        qDebug() << "DXT1 size check:" << (size.width() / 4) * (size.height() / 4) * 8;


        if(size.width() < maxTextureSize && size.height() < maxTextureSize) {
            glCompressedTexImage2D(GL_TEXTURE_2D, trueMipmapLevel, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                                   size.width(), size.height(), 0,
                                   imageData.size(), imageData.data());
            trueMipmapLevel++;
        }

        qDebug() << "Error: " << glGetError();
    }

    release();

    emit textureUploaded();
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

