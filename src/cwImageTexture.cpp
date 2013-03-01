//Our includes
#include "cwImageTexture.h"
#include "cwProjectImageProvider.h"
#include "cwDebug.h"

//QT includes
#include <QtConcurrentRun>
#include <QtConcurrentMap>
#include <QVector2D>

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
    if(LoadNoteWatcher != NULL) {
        LoadNoteWatcher->deleteLater();
    }

    if(TextureId != 0) {
        glDeleteTextures(1, &TextureId);
    }

    NoteVertexBuffer.destroy();
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
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glBindTexture(GL_TEXTURE_2D, 0);

    //Create the vertex buffer
    NoteVertexBuffer = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    NoteVertexBuffer.create();    //Create the vertexes buffer to render a quad

    QVector<QVector2D> vertices;
    vertices.reserve(4);
    vertices.append(QVector2D(0.0, 1.0));
    vertices.append(QVector2D(0.0, 0.0));
    vertices.append(QVector2D(1.0, 1.0));
    vertices.append(QVector2D(1.0, 0.0));

    //Allocate the buffer array for this object
    NoteVertexBuffer.bind();
    NoteVertexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    NoteVertexBuffer.allocate(vertices.data(), vertices.size() * sizeof(QVector2D));
    NoteVertexBuffer.release();
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

    QList<LoadImageData>mipmaps = LoadNoteWatcher->future().results();

    if(mipmaps.empty()) { return; }

    QSize firstLevel = mipmaps.first().ImageDataSize;
    if(!isDivisibleBy4(firstLevel)) {
        qDebug() << "Trying to upload an image that isn't divisible by 4. This will crash ANGLE on windows." << LOCATION;
        TextureDirty = false;
        return;
    }

    //Load the data into opengl
    bind();

    //Get the max texture size
    GLint maxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

    int trueMipmapLevel = 0;
    for(int mipmapLevel = 0; mipmapLevel < mipmaps.size(); mipmapLevel++) {

        //Get the mipmap data
        LoadImageData image = mipmaps.at(mipmapLevel);
        QByteArray imageData = image.ImageData;
        QSize size = image.ImageDataSize;

        if(size.width() < maxTextureSize && size.height() < maxTextureSize) {            
            glCompressedTexImage2D(GL_TEXTURE_2D, trueMipmapLevel, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                                   size.width(), size.height(), 0,
                                   imageData.size(), imageData.data());

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
        QFuture<LoadImageData> future = QtConcurrent::mapped(Image.mipmaps(),
                                                             LoadImageKernal(ProjectFilename));
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

    LoadNoteWatcher = new QFutureWatcher<LoadImageData>();
    connect(LoadNoteWatcher, &QFutureWatcher<LoadImageData>::finished, this, &cwImageTexture::markAsDirty);
    connect(LoadNoteWatcher, &QFutureWatcher<LoadImageData>::finished, this, &cwImageTexture::textureUploaded);
}

/**
 * @brief cwImageTexture::isDivisibleBy4
 * @param size
 * @return True if the size is divisible by 4
 *
 * This is to prevent D3D from crashing when using ANGLE
 */
bool cwImageTexture::isDivisibleBy4(QSize size) const
{
    int widthRemainder = size.width() % 4;
    int heightRemainder = size.height() % 4;
    return widthRemainder == 0 && heightRemainder == 0;
}

void cwImageTexture::markAsDirty()
{
    TextureDirty = true;
}

/**
  \brief Allow QtConturrent to create a QImage

  This returns a opengl formatted image
  */
cwImageTexture::LoadImageData cwImageTexture::LoadImageKernal::operator ()(int imageId) {
    //Extract the image data from the imageProvider
    cwProjectImageProvider imageProvidor;
    imageProvidor.setProjectPath(Filename);

    LoadImageData data;

    QByteArray imageData = imageProvidor.requestImageData(imageId, &data.ImageDataSize);

    data.ImageData = imageData;

    return data;
}



