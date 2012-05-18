//Our includes
#include "cwImageItem.h"
#include "cwGLShader.h"
#include "cwShaderDebugger.h"
#include "cwProjectImageProvider.h"
#include "cwImageProperties.h"
#include "cwGlobalDirectory.h"
#include "cwImageTexture.h"

//QT includes
#include <QtConcurrentRun>
#include <QtConcurrentMap>


cwImageItem::cwImageItem(QQuickItem *parent) :
    cwGLRenderer(parent),
    ImageProperties(new cwImageProperties(this)),
    Rotation(0.0),
    RotationCenter(0.5, 0.5),
    NoteTexture(new cwImageTexture(this))
{
    //Called when the image is finished loading
    connect(NoteTexture, SIGNAL(textureUploaded()), SLOT(imageFinishedLoading()));
    connect(NoteTexture, SIGNAL(projectChanged()), SIGNAL(projectFilenameChanged()));
    ImageProperties->setImage(Image);

    setOpaquePainting(false);
}

/**
  Sets the image that'll be displayed in the imageItem
  */
void cwImageItem::setImage(const cwImage& image) {
    if(Image != image) {
        Image = image;
        ImageProperties->setImage(Image);
        NoteTexture->setImage(Image);
        resizeGL();
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
  The image has complete loading it self
  */
void cwImageItem::imageFinishedLoading() {
    QSize imageSize = Image.origianlSize();
    resizeGL();

    QMatrix4x4 newViewMatrix;
    newViewMatrix.scale(imageSize.width(), imageSize.height(), 1.0);

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
    cwGLShader* imageVertexShader = new cwGLShader(QOpenGLShader::Vertex);
    imageVertexShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/NoteItem.vert");

    cwGLShader* imageFragmentShader = new cwGLShader(QOpenGLShader::Fragment);
    imageFragmentShader->setSourceFile(cwGlobalDirectory::baseDirectory() + "shaders/NoteItem.frag");

    ImageProgram = new QOpenGLShaderProgram(this);
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
  \brief The initilizes the texture map
  */
void cwImageItem::initializeTexture() {
    qDebug() << "Initialized";

    //Generate the color texture
    NoteTexture->initialize();
}


/**
  \brief Called when the note item is resized
  */
void cwImageItem::resizeGL() {
    QSize imageSize = Image.origianlSize();
    if(!imageSize.isValid()) { return; }

    QSize windowSize(width(), height());

    QTransform transformer;
    transformer.rotate(Rotation);
    QRectF rotatedImageRect = transformer.mapRect(QRectF(QPointF(), imageSize));

    QSize scaledImageSize = rotatedImageRect.size().toSize();
    scaledImageSize.scale(windowSize, Qt::KeepAspectRatio);

    float changeInWidth = windowSize.width() / (float)scaledImageSize.width();
    float changeInHeight = windowSize.height() / (float)scaledImageSize.height();

    float widthProjection = imageSize.width() * changeInWidth;
    float heightProjection = imageSize.height() * changeInHeight;

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
    update();
}

/**
  \brief Draws the note item
  */
void cwImageItem::paint(QPainter* painter) {
    if(!Image.isValid()) { return; }

    painter->beginNativePainting();

    NoteVertexBuffer.bind();

    ImageProgram->bind();
    ImageProgram->setAttributeBuffer(vVertex, GL_FLOAT, 0, 2);
    ImageProgram->enableAttributeArray(vVertex);
    ImageProgram->setUniformValue(ModelViewProjectionMatrix, Camera->viewProjectionMatrix() * RotationModelMatrix);

    glEnable(GL_TEXTURE_2D);
    NoteTexture->bind();

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); //Draw the quad

    NoteVertexBuffer.release();
    ImageProgram->release();
    NoteTexture->release();

    painter->endNativePainting();
}

/**
 * @brief cwImageItem::updatePaintNode
 * @param oldNode
 * @param data
 * @return
 *
 * Called when the renderer can update opengl objects safely.  This is called by the rendering
 * thread.
 */
QSGNode *cwImageItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData * data)
{
    NoteTexture->updateData();
    return cwGLRenderer::updatePaintNode(oldNode, data);
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

/**
  Set the project filename

  The project filename allow this imageItem to extra data from disk.
  */
void cwImageItem::setProjectFilename(QString filename) {
    NoteTexture->setProject(filename);
}

/**
  \brief Gets the project Filename
  */
QString cwImageItem::projectFilename() const {
    return NoteTexture->project();
}
