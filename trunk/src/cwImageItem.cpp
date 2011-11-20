//Our includes
#include "cwImageItem.h"
#include "cwGLShader.h"
#include "cwShaderDebugger.h"
#include "cwProjectImageProvider.h"
#include "cwImageProperties.h"

//QT includes
#include <QtConcurrentRun>
#include <QtConcurrentMap>


cwImageItem::cwImageItem(QDeclarativeItem *parent) :
    cwGLRenderer(parent),
    ImageProperties(new cwImageProperties(this)),
    Rotation(0.0),
    RotationCenter(0.5, 0.5),
    LoadNoteWatcher(new QFutureWatcher<QPair<QByteArray, QSize> >(this))
{
    //Called when the image is finished loading
    connect(LoadNoteWatcher, SIGNAL(finished()), SLOT(ImageFinishedLoading()));
    ImageProperties->setImage(Image);
}

/**
  Sets the image that'll be displayed in the imageItem
  */
void cwImageItem::setImage(const cwImage& image) {
    if(Image != image) {
        Image = image;
        ImageProperties->setImage(Image);

        //Load the notes in an asyn way
        LoadNoteWatcher->cancel(); //Cancel previous run, if still running
        QFuture<QPair<QByteArray, QSize> > future = QtConcurrent::mapped(Image.mipmaps(), LoadImage(ProjectFilename));
        LoadNoteWatcher->setFuture(future);
    }
}

/**
  \brief This updates the rotation center for the image

  By default the rotation center is 0.5, 0.5, in normalize image coordinates

  This will update the rotation center such that the image will rotate around
  the center of the view.

  This is useful for animating the Image while it's rotating
  */
void cwImageItem::updateRotationCenter() {
    QPoint center(width() / 2.0, height() / 2.0);
    QVector3D rotationCenter = Camera->unProject(center, 0.0);
    RotationCenter = rotationCenter.toPointF();
}

/**
  Sets the rotation of the image
  */
void cwImageItem::setRotation(float degrees) {
    if(Rotation != degrees) {
        Rotation = degrees;

        QMatrix4x4 rotationModelMatrix;
        rotationModelMatrix.translate(RotationCenter.x(), RotationCenter.y(), 0.0);
        rotationModelMatrix.rotate(-degrees, 0.0, 0.0, 1.0);
        rotationModelMatrix.translate(-RotationCenter.x(), -RotationCenter.y(), 0.0);
        RotationModelMatrix = rotationModelMatrix;

        resizeGL();

        emit rotationChanged();
        emit modelMatrixChanged();
    }
}

/**
  Set the project filename

  The project filename allow this imageItem to extra data from disk.
  */
void cwImageItem::setProjectFilename(QString projectFilename) {
    if(projectFilename != ProjectFilename) {
        ProjectFilename = projectFilename;
        emit projectFilenameChanged();
    }
}

/**
  The image has complete loading it self
  */
void cwImageItem::ImageFinishedLoading() {
    GLWidget->makeCurrent();

    QList<QPair<QByteArray, QSize> >mipmaps = LoadNoteWatcher->future().results();

    if(mipmaps.empty()) { return; }

    //Load the data into opengl
    glBindTexture(GL_TEXTURE_2D, NoteTexture);

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
            glCompressedTexImage2D(GL_TEXTURE_2D, trueMipmapLevel, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                                   size.width(), size.height(), 0,
                                   imageData.size(), imageData.data());
            trueMipmapLevel++;
        }
    }

   // glGenerateMipmap(GL_TEXTURE_2D); //Generate the mipmaps for NoteTexture
    glBindTexture(GL_TEXTURE_2D, 0);

    ImageSize = mipmaps.first().second;
    resizeGL();

    QMatrix4x4 newViewMatrix;
    newViewMatrix.scale(ImageSize.width(), ImageSize.height(), 1.0);

    //Center the page of notes in the middle
    QPoint center(width() / 2.0, height() / 2.0);
    QVector3D newCenter = Camera->unProject(center, 0.0, newViewMatrix, QMatrix4x4());
    newViewMatrix.translate(newCenter.x() - 0.5, newCenter.y() - 0.5, 0.0);

    //Update the view
    Camera->setViewMatrix(newViewMatrix);

    emit imageChanged();

    update();
}

/**
  \brief Sets up the shaders for this item
  */
void cwImageItem::initializeGL() {
    initializeShaders();
    initializeVertexBuffers();
    initializeTexture();
}

/**
  Initilizes the shaders for this object
  */
void cwImageItem::initializeShaders() {
    cwGLShader* imageVertexShader = new cwGLShader(QGLShader::Vertex);
    imageVertexShader->setSourceFile("shaders/NoteItem.vert");

    cwGLShader* imageFragmentShader = new cwGLShader(QGLShader::Fragment);
    imageFragmentShader->setSourceFile("shaders/NoteItem.frag");

    ImageProgram = new QGLShaderProgram(this);
    ImageProgram->addShader(imageVertexShader);
    ImageProgram->addShader(imageFragmentShader);

    bool linkingErrors = ImageProgram->link();
    if(!linkingErrors) {
        qDebug() << "Linking errors:" << ImageProgram->log();
    }

    ShaderDebugger->addShaderProgram(ImageProgram);

    ImageProgram->bind();

    vVertex = ImageProgram->attributeLocation("vVertex");
    ModelViewProjectionMatrix = ImageProgram->uniformLocation("ModelViewProjectionMatrix");

    ImageProgram->setUniformValue("Texture", 0); //set the texture unit to 0
}

/**
  Initilizes geometry for rendering the texture
  */
void cwImageItem::initializeVertexBuffers() {
    //Create the vertex buffer
    NoteVertexBuffer = QGLBuffer(QGLBuffer::VertexBuffer);
    NoteVertexBuffer.create();    //Create the vertexes buffer to render a quad

    QVector<QVector2D> vertices;
    vertices.reserve(4);
    vertices.append(QVector2D(0.0, 1.0));
    vertices.append(QVector2D(0.0, 0.0));
    vertices.append(QVector2D(1.0, 1.0));
    vertices.append(QVector2D(1.0, 0.0));

    //Allocate the buffer array for this object
    NoteVertexBuffer.bind();
    NoteVertexBuffer.setUsagePattern(QGLBuffer::StaticDraw);
    NoteVertexBuffer.allocate(vertices.data(), vertices.size() * sizeof(QVector2D));
    NoteVertexBuffer.release();
}

/**
  \brief The initilizes the texture map
  */
void cwImageItem::initializeTexture() {
    //Generate the color texture
    glGenTextures(1, &NoteTexture);
    glBindTexture(GL_TEXTURE_2D, NoteTexture);
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
    glBindTexture(GL_TEXTURE_2D, 0);
}


/**
  \brief Called when the note item is resized
  */
void cwImageItem::resizeGL() {

    if(!ImageSize.isValid()) { return; }

    QSize windowSize(width(), height());

    QTransform transformer;
    transformer.rotate(Rotation);
    QRectF rotatedImageRect = transformer.mapRect(QRectF(QPointF(), ImageSize));

    QSize scaledImageSize = rotatedImageRect.size().toSize();
    scaledImageSize.scale(windowSize, Qt::KeepAspectRatio);

    float changeInWidth = windowSize.width() / (float)scaledImageSize.width();
    float changeInHeight = windowSize.height() / (float)scaledImageSize.height();

    float widthProjection = ImageSize.width() * changeInWidth;
    float heightProjection = ImageSize.height() * changeInHeight;

    QPoint center(width() / 2.0, height() / 2.0);
    QVector3D oldViewCenter = Camera->unProject(center, 0.0);

    QMatrix4x4 orthognalProjection;
    orthognalProjection.ortho(0.0, widthProjection,
                              0.0, heightProjection,
                              -1.0, 1.0);
    Camera->setProjectionMatrix(orthognalProjection);

    QVector3D newViewCenter = Camera->unProject(center, 0.0);
    QVector3D difference = newViewCenter - oldViewCenter;
    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(difference);
    Camera->setViewMatrix(viewMatrix);

    Camera->setViewport(QRect(QPoint(0.0, 0.0), windowSize));

//    updateQMLTransformItem();
}

/**
  \brief Draws the note item
  */
void cwImageItem::paintFramebuffer() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(!Image.isValid()) { return; }

    glDisable(GL_DEPTH_TEST);

    NoteVertexBuffer.bind();

    ImageProgram->bind();
    ImageProgram->setAttributeBuffer(vVertex, GL_FLOAT, 0, 2);
    ImageProgram->enableAttributeArray(vVertex);
    ImageProgram->setUniformValue(ModelViewProjectionMatrix, Camera->viewProjectionMatrix() * RotationModelMatrix);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, NoteTexture);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); //Draw the quad

    NoteVertexBuffer.release();
    ImageProgram->release();
    glBindTexture(GL_TEXTURE_2D, 0);
}

/**
  \brief Allow QtConturrent to create a QImage

  This returns a opengl formatted image
  */
QPair<QByteArray, QSize> cwImageItem::LoadImage::operator ()(int imageId) {
    //Extract the image data from the imageProvider
    cwProjectImageProvider imageProvidor;
    imageProvidor.setProjectPath(Filename);

    QSize size;
    QByteArray imageData = imageProvidor.requestImageData(imageId, &size);

    return QPair<QByteArray, QSize>(imageData, size);
}

/**
  This converts a mouse click into note coordinates

  The note coordinate are normalized from 0.0 to 1.0 for both the x and y.
  */
QPointF cwImageItem::mapQtViewportToNote(QPoint qtViewportCoordinate) {
    QPoint glViewportPoint = Camera->mapToGLViewport(qtViewportCoordinate);
    QVector3D notePosition = Camera->unProject(glViewportPoint, 0.0, RotationModelMatrix);
    return notePosition.toPointF();
}
