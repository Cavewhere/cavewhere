//Our includes
#include "cwNoteItem.h"
#include "cwShaderDebugger.h"
#include "cwGLShader.h"
#include "cwCamera.h"

//Qt includes
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPixmapItem>
#include <QLabel>
#include <QUrl>
#include <QtConcurrentRun>
#include <QFuture>


cwNoteItem::cwNoteItem(QDeclarativeItem *parent) :
    cwGLRenderer(parent),
    //ImageItem(NULL),
//    PixmapItem(new QGraphicsPixmapItem(this)),
    //SmoothPixmapItem(new QGraphicsPixmapItem(this)),
//    PixmapFutureWatcher(new QFutureWatcher<QImage>(this)),
    LoadNoteWatcher(new QFutureWatcher<QImage>(this))
//    LODTimer(new QTimer(this))
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);

//    LODTimer->setInterval(250); //100ms

//    connect(PixmapFutureWatcher, SIGNAL(finished()), SLOT(SetSmoothPixmap()));
    connect(LoadNoteWatcher, SIGNAL(finished()), SLOT(ImageFinishedLoading()));
//    connect(LODTimer, SIGNAL(timeout()), SLOT(RenderSmooth()));



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
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
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

   float windowAspect = width() / height();

   float width;
   float height;

   if(windowAspect < 1.0) {
       //Height is bigger
       width = ImageSize.width();
       height = ImageSize.height() / windowAspect;
   } else {
       width = ImageSize.width() * windowAspect;
       height = ImageSize.height();
   }




    QMatrix4x4 orthognalProjection;
    orthognalProjection.ortho(0.0, width,
                              0.0, height,
                              -1.0, 1.0);
    Camera->setProjectionMatrix(orthognalProjection);
}

/**
  \brief Draws the note item
  */
void cwNoteItem::paintFramebuffer() {
    glClear(GL_COLOR_BUFFER_BIT);

    VertexBuffer.bind();

    ImageProgram->bind();
    ImageProgram->setAttributeBuffer(vVertex, GL_FLOAT, 0, 2);
    ImageProgram->enableAttributeArray(vVertex);
    ImageProgram->setUniformValue(ModelViewProjectionMatrix, Camera->viewProjectionMatrix() * NoteModelMatrix);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, NoteTexture);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); //Draw the quad

    VertexBuffer.release();
    ImageProgram->release();
    glBindTexture(GL_TEXTURE_2D, 0);
}

///**
//  \brief Fits the note item to the view
//  */
//void cwNoteItem::fitToView() {
//    ScaleCenter = QPointF();
//    FastPixmapTransform.reset();
//    SmoothTransform.reset();

//    //Find the scale of the item to
//    float scaleX = width() / OriginalNote.width();
//    float scaleY = height() / OriginalNote.height();

//    float scale = qMin(scaleX, scaleY);
//    SetScale(scale);
//    RenderSmooth();
//}


void cwNoteItem::mouseMoveEvent ( QGraphicsSceneMouseEvent * event ) {

    QPoint currentPanPoint = Camera->mapToGLViewport(event->pos().toPoint());

    QVector3D unProjectedCurrent = Camera->unProject(currentPanPoint, 0.1, NoteModelMatrix);
    QVector3D unProjectedLast = Camera->unProject(LastPanPoint, 0.1, NoteModelMatrix);

    QVector3D delta = unProjectedCurrent - unProjectedLast - ;

    NoteModelMatrix.translate(delta);

    LastPanPoint = currentPanPoint;

    update();

//    if(!LastPanPoint.isNull()) { // && ImageItem != NULL) {
//        //For panning
//        QTransform inverseFastPixmapTransform = FastPixmapTransform.inverted();
//        QTransform inverseSmoothTransform =SmoothTransform.inverted();

//        QPointF pixmapItemDelta = inverseFastPixmapTransform.map(event->pos()) - inverseFastPixmapTransform.map(LastPanPoint); //mapToScene(event->pos());
//        QPointF smoothItemDelta = inverseSmoothTransform.inverted().map(event->pos()) - inverseSmoothTransform.inverted().map(LastPanPoint);
//        LastPanPoint = event->pos();

//        //ImageItem->translate(delta.x(), delta.y());
//        FastPixmapTransform.translate(pixmapItemDelta.x(), pixmapItemDelta.y());
//        SmoothTransform.translate(smoothItemDelta.x(), smoothItemDelta.y());

//        if(PixmapItem->pixmap().cacheKey() == FastPixmap.cacheKey()) {
//            PixmapItem->setTransform(FastPixmapTransform);
//        } else {
//            PixmapItem->setTransform(SmoothTransform);
//        }
//        update();
//    }
}

void cwNoteItem::mousePressEvent ( QGraphicsSceneMouseEvent * event ) {
    LastPanPoint = Camera->mapToGLViewport(event->pos().toPoint());
    Camera->unProject(LastPanPoint, 0.1, NoteModelMatrix);
    event->accept();
}

//void cwNoteItem::mouseReleaseEvent ( QGraphicsSceneMouseEvent * /*event*/ ) {
//    LastPanPoint = QPointF();
//}

void cwNoteItem::wheelEvent(QGraphicsSceneWheelEvent *event) {
    //Calc the scaleFactor
    float scaleFactor = 1.1;
    if(event->delta() < 0) {
        //Zoom out
        scaleFactor = 1.0 / scaleFactor;
    }

    QPoint screenCoords = Camera->mapToGLViewport(event->pos().toPoint());
    QVector3D beforeZoom = Camera->unProject(screenCoords, 0.1, NoteModelMatrix);

    QMatrix4x4 zoom;
    zoom.scale(scaleFactor, scaleFactor);
    NoteModelMatrix = NoteModelMatrix * zoom;

    QVector3D afterZoom = Camera->unProject(screenCoords, 0.1, NoteModelMatrix);

    //Adjust the scale matrix
    QVector3D changeInPosition = afterZoom - beforeZoom;
    NoteModelMatrix.translate(changeInPosition);

    update();
}

//void cwNoteItem::RenderSmooth() {
//    LODTimer->stop();

//    QSize area = PixmapItem->mapRectToScene(PixmapItem->boundingRect()).size().toSize();

//    if(area.width() >= OriginalNote.size().width() &&
//            area.height() >= OriginalNote.height()) {
//        return;
//    }

//    QFuture<QImage> scaledFuture = QtConcurrent::run(OriginalNote, &QImage::scaled, area, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
//    PixmapFutureWatcher->setFuture(scaledFuture);

//}

//void cwNoteItem::SetSmoothPixmap() {

//    QImage scaledImage = PixmapFutureWatcher->future().result();
//    SmoothPixmap = QPixmap::fromImage(scaledImage);

//    if(!LODTimer->isActive()) {
//        PixmapItem->setTransform(SmoothTransform);
//        PixmapItem->setPixmap(SmoothPixmap);
//    }

//}

/**
  \brief Sets the image source for the the note item
  */
void cwNoteItem::setImageSource(QString imageSource) {
    if(imageSource != NoteSource) {
        //Transform.reset();

        NoteSource = imageSource;

        //Load the notes in an asyn way
        LoadNoteWatcher->cancel(); //Cancel previous run, if still running
        QFuture<QImage> future = QtConcurrent::run(cwNoteItem::LoadImage, QUrl(NoteSource).toLocalFile());
        LoadNoteWatcher->setFuture(future);


    }
}


///**
//  * Sets the current zoom factor for the view
//  * This will reset the transform to the identity
//  \see SetScaleCenter()
//  */
//void cwNoteItem::SetScale(float newScale) {
//    QTransform matrix;
//    matrix.translate(ScaleCenter.x(), ScaleCenter.y());
//    matrix.scale(newScale, newScale);
//    matrix.translate(-ScaleCenter.x(), -ScaleCenter.y());
//    FastPixmapTransform = matrix * FastPixmapTransform;
//    PixmapItem->setTransform(FastPixmapTransform, false);

//    QPointF pixmapItemPosition = FastPixmapTransform.map(QPointF(0.0, 0.0));
//    QPointF smoothItemPosition = SmoothTransform.map(QPointF(0.0, 0.0));
//    QPointF differance = pixmapItemPosition - smoothItemPosition;
//    SmoothTransform.translate(differance.x(), differance.y());
//}

/**
  The image has complete loading it self
  */
void cwNoteItem::ImageFinishedLoading() {
    GLWidget->makeCurrent();

    QImage glNoteImage = LoadNoteWatcher->future().result();

    //Load the data into opengl
    glBindTexture(GL_TEXTURE_2D, NoteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 glNoteImage.width(), glNoteImage.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, glNoteImage.bits());
    glGenerateMipmap(GL_TEXTURE_2D); //Generate the mipmaps for NoteTexture
    glBindTexture(GL_TEXTURE_2D, 0);


//    qDebug() << "Image finished loading" << OriginalNote.size() << LoadNoteWatcher->isCanceled();
//    FastPixmap = QPixmap::fromImage(OriginalNote);
//    PixmapItem->setPixmap(FastPixmap);

//    RenderSmooth();
    ImageSize = glNoteImage.size();
    NoteModelMatrix.setToIdentity();
    NoteModelMatrix.scale(ImageSize.width(), ImageSize.height(), 1.0);

    resizeGL();

    emit imageSourceChanged();

    update();
}

/**
  \brief Allow QtConturrent to create a QImage

  This returns a opengl formatted image
  */
QImage cwNoteItem::LoadImage(QString& filename) {
    return QGLWidget::convertToGLFormat(QImage(filename));
}
