//Our includes
#include "cwNoteItem.h"
#include "cwShaderDebugger.h"
#include "cwGLShader.h"
#include "cwCamera.h"
#include "cwProjectImageProvider.h"

//Qt includes
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QUrl>
#include <QtConcurrentRun>
#include <QtConcurrentMap>
#include <QFuture>


cwNoteItem::cwNoteItem(QDeclarativeItem *parent) :
    cwGLRenderer(parent),
    LoadNoteWatcher(new QFutureWatcher<QPair<QByteArray, QSize> >(this)),
    Note(NULL)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);

    connect(LoadNoteWatcher, SIGNAL(finished()), SLOT(ImageFinishedLoading()));
}

void cwNoteItem::setProjectFilename(QString projectFilename) {
    if(projectFilename != ProjectFilename) {
        ProjectFilename = projectFilename;
        emit projectFilenameChanged();
    }
}

/**
  \brief Sets up the shaders for this item
  */
void cwNoteItem::initializeGL() {
    initializeShaders();
    initializeVertexBuffers();
    initializeTexture();
}

/**
  Initilizes the shaders for this object
  */
void cwNoteItem::initializeShaders() {
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
void cwNoteItem::initializeVertexBuffers() {
    //Create the vertex buffer
    VertexBuffer = QGLBuffer(QGLBuffer::VertexBuffer);
    VertexBuffer.create();    //Create the vertexes buffer to render a quad

    QVector<QVector2D> vertices;
    vertices.reserve(4);
    vertices.append(QVector2D(0.0, 1.0));
    vertices.append(QVector2D(0.0, 0.0));
    vertices.append(QVector2D(1.0, 1.0));
    vertices.append(QVector2D(1.0, 0.0));

    //Allocate the buffer array for this object
    VertexBuffer.bind();
    VertexBuffer.setUsagePattern(QGLBuffer::StaticDraw);
    VertexBuffer.allocate(vertices.data(), vertices.size() * sizeof(QVector2D));
    VertexBuffer.release();
}

/**
  \brief The initilizes the texture map
  */
void cwNoteItem::initializeTexture() {
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
void cwNoteItem::resizeGL() {

    if(!ImageSize.isValid()) { return; }

    QSize windowSize(width(), height());

    QTransform transformer;
    transformer.rotate(Note->rotate());
    QRectF rotatedImageRect = transformer.mapRect(QRectF(QPointF(), ImageSize));

    QSize scaledImageSize = rotatedImageRect.size().toSize();
    scaledImageSize.scale(windowSize, Qt::KeepAspectRatio);

    float changeInWidth = windowSize.width() / (float)scaledImageSize.width();
    float changeInHeight = windowSize.height() / (float)scaledImageSize.height();

    float widthProjection = ImageSize.width() * changeInWidth;
    float heightProjection = ImageSize.height() * changeInHeight;

    QMatrix4x4 orthognalProjection;
    orthognalProjection.ortho(0.0, widthProjection,
                              0.0, heightProjection,
                              -1.0, 1.0);
    Camera->setProjectionMatrix(orthognalProjection);
}

/**
  \brief Draws the note item
  */
void cwNoteItem::paintFramebuffer() {
    glClear(GL_COLOR_BUFFER_BIT);
    if(Note == NULL) { return; }

    VertexBuffer.bind();

    ImageProgram->bind();
    ImageProgram->setAttributeBuffer(vVertex, GL_FLOAT, 0, 2);
    ImageProgram->enableAttributeArray(vVertex);
    ImageProgram->setUniformValue(ModelViewProjectionMatrix, Camera->viewProjectionMatrix() * NoteScaleModelMatrix * RotationModelMatrix);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, NoteTexture);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); //Draw the quad

    VertexBuffer.release();
    ImageProgram->release();
    glBindTexture(GL_TEXTURE_2D, 0);
}


void cwNoteItem::mouseMoveEvent ( QGraphicsSceneMouseEvent * event ) {

    QPoint currentPanPoint = Camera->mapToGLViewport(event->pos().toPoint());

    QVector3D unProjectedCurrent = Camera->unProject(currentPanPoint, 0.1, NoteScaleModelMatrix);
    QVector3D unProjectedLast = Camera->unProject(LastPanPoint, 0.1, NoteScaleModelMatrix);

    QVector3D delta = unProjectedCurrent - unProjectedLast;

    NoteScaleModelMatrix.translate(delta);

    LastPanPoint = currentPanPoint;

    update();
}

void cwNoteItem::mousePressEvent ( QGraphicsSceneMouseEvent * event ) {
    LastPanPoint = Camera->mapToGLViewport(event->pos().toPoint());
    Camera->unProject(LastPanPoint, 0.1, NoteScaleModelMatrix);
    event->accept();
}

void cwNoteItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    //Calc the scaleFactor
    float scaleFactor = 1.1;
    if(event->delta() < 0) {
        //Zoom out
        scaleFactor = 1.0 / scaleFactor;
    }

    QPoint screenCoords = Camera->mapToGLViewport(event->pos().toPoint());
    QVector3D beforeZoom = Camera->unProject(screenCoords, 0.1, NoteScaleModelMatrix);

    QMatrix4x4 zoom;
    zoom.scale(scaleFactor, scaleFactor);
    NoteScaleModelMatrix = NoteScaleModelMatrix * zoom;

    QVector3D afterZoom = Camera->unProject(screenCoords, 0.1, NoteScaleModelMatrix);

    //Adjust the scale matrix
    QVector3D changeInPosition = afterZoom - beforeZoom;
    NoteScaleModelMatrix.translate(changeInPosition);

    update();
}

/**
  Sets the note that this note item displayes
  */
void cwNoteItem::setNote(cwNote* note) {
    if(Note != note) {
        if(Note != NULL) {
            disconnect(Note, NULL, this, NULL);
        }

        Note = note;

        if(Note != NULL) {
            connect(Note, SIGNAL(destroyed()), SLOT(noteDeleted()));
            connect(Note, SIGNAL(rotateChanged(float)), SLOT(updateNoteRotation(float)));
            connect(Note, SIGNAL(imageChanged(cwImage)), SLOT(setImage(cwImage)));
            setImage(Note->image());
        }
    }
}

/**
  \brief Sets the image source for the the note item
  */
void cwNoteItem::setImage(cwImage image) {
    //Load the notes in an asyn way
    LoadNoteWatcher->cancel(); //Cancel previous run, if still running
    QFuture<QPair<QByteArray, QSize> > future = QtConcurrent::mapped(image.mipmaps(), LoadImage(ProjectFilename));
    LoadNoteWatcher->setFuture(future);
}

/**
  The image has complete loading it self
  */
void cwNoteItem::ImageFinishedLoading() {
    GLWidget->makeCurrent();

    QList<QPair<QByteArray, QSize> >mipmaps = LoadNoteWatcher->future().results();

    if(mipmaps.empty()) { return; }

    //Load the data into opengl
    glBindTexture(GL_TEXTURE_2D, NoteTexture);

    for(int mipmapLevel = 0; mipmapLevel < mipmaps.size(); mipmapLevel++) {

        //Get the mipmap data
        QPair<QByteArray, QSize> image = mipmaps.at(mipmapLevel);
        QByteArray imageData = image.first;
        QSize size = image.second;

        glCompressedTexImage2D(GL_TEXTURE_2D, mipmapLevel, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                               size.width(), size.height(), 0,
                               imageData.size(), imageData.data());
    }

   // glGenerateMipmap(GL_TEXTURE_2D); //Generate the mipmaps for NoteTexture
    glBindTexture(GL_TEXTURE_2D, 0);

    ImageSize = mipmaps.first().second;
    NoteScaleModelMatrix.setToIdentity();
    NoteScaleModelMatrix.scale(ImageSize.width(), ImageSize.height(), 1.0);

    updateNoteRotation(Note->rotate());

    resizeGL();

    emit noteChanged(Note);

    update();
}

/**
  \brief Rotates the note item

  \param degrees, the amount of rotaton of the noteItem
  */
void cwNoteItem::updateNoteRotation(float degrees) {
    RotationModelMatrix.setToIdentity();
    RotationModelMatrix.translate(.5, .5, 0.0);
    RotationModelMatrix.rotate(-degrees, 0.0, 0.0, 1.0);
    RotationModelMatrix.translate(-.5, -.5, 0.0);
    resizeGL();
    update();
}

/**
  \brief Allow QtConturrent to create a QImage

  This returns a opengl formatted image
  */
QPair<QByteArray, QSize> cwNoteItem::LoadImage::operator ()(int imageId) {
    //Extract the image data from the imageProvider
    cwProjectImageProvider imageProvidor;
    imageProvidor.setProjectPath(Filename);

    QSize size;
    QByteArray imageData = imageProvidor.requestImageData(imageId, &size);

    return QPair<QByteArray, QSize>(imageData, size);
}
