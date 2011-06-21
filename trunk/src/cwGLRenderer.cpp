//Glew includes
#include "GL/glew.h"

//Our includes
#include "cwGLRenderer.h"
#include "cwCamera.h"
#include "cwGraphicsSceneMouseTransition.h"
#include "cwMouseEventTransition.h"
#include "cwWheelEventTransition.h"
#include "cwCamera.h"
#include "cwLine3D.h"
#include "cwPlane.h"
#include "cwRegularTile.h"
#include "cwEdgeTile.h"
#include "cwShaderDebugger.h"
#include "cwGLShader.h"
#include "cwGLTerrain.h"
#include "cwGLLinePlot.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwStation.h"

//Qt includes
#include <QPainter>
#include <QString>
#include <QVector3D>
#include <QRect>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMouseEventTransition>
#include <QKeyEventTransition>
#include <QDebug>
#include <QGraphicsSceneWheelEvent>
#include <QtConcurrentMap>
#include <QFontMetrics>

//Std includes
#include <math.h>


cwGLRenderer::cwGLRenderer(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{
    Region = NULL;

    GLWidget = NULL;
    setFlag(QGraphicsItem::ItemHasNoContents, false);

    //setupInteractionStateMachine();

    Camera = new cwCamera(this);
    ShaderDebugger = new cwShaderDebugger(this);

    Terrain = new cwGLTerrain(this);
    Terrain->setCamera(Camera);
    Terrain->setShaderDebugger(ShaderDebugger);
    Terrain->setNumberOfLevels(10);
    //connect(Terrain, SIGNAL(redraw()), SLOT(updateGL()));

    LinePlot = new cwGLLinePlot(this);
    LinePlot->setCamera(Camera);
    LinePlot->setShaderDebugger(ShaderDebugger);

    connect(this, SIGNAL(widthChanged()), SLOT(resizeGL()));
    connect(this, SIGNAL(heightChanged()), SLOT(resizeGL()));
}

void cwGLRenderer::paint(QPainter* painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->beginNativePainting();

    //Draw everything to a framebuffer
    glPushAttrib(GL_VIEWPORT_BIT);
    MultiSampleFramebuffer->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, width(), height());
    glEnable(GL_DEPTH_TEST);
    Terrain->draw();
    LinePlot->draw();
    glDisable(GL_DEPTH_TEST);
    glPopAttrib();

    MultiSampleFramebuffer->release();

    //Copy the MultiSampleFramebuffer data to the textureFramebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, MultiSampleFramebuffer->handle());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, TextureFramebuffer);
    glBlitFramebuffer(0, 0, MultiSampleFramebuffer->width(), MultiSampleFramebuffer->height(),
                      0, 0, MultiSampleFramebuffer->width(), MultiSampleFramebuffer->height(),
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                      GL_LINEAR);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    //Draw the texture that the TextureFramebuffer just rendered to
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, ColorTexture);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 1.0);
    glVertex2f(0, 0);

    glTexCoord2f(0.0, 0.0);
    glVertex2f(0, height());

    glTexCoord2f(1.0, 0.0);
    glVertex2f(width(), height());

    glTexCoord2f(1.0, 1.0);
    glVertex2f(width(), 0);

    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);

    painter->endNativePainting();

    painter->save();
    painter->setPen(QPen());
    painter->setBrush(QBrush());
    painter->setFont(QFont());

    //Render text labels
    renderStationLabels(painter);
    painter->restore();

}

/**
  \brief This initializes this item's opengl stuff
  */
void cwGLRenderer::initializeGL() {
    GLWidget->makeCurrent();

    //Setup the camera
    resetView();

    //Genereate the multi sample buffer for anti aliasing
    MultiSampleFramebuffer = new QGLFramebufferObject(QSize(1, 1));

    //Generate the texture framebuffer for rendering
    glGenFramebuffers(1, &TextureFramebuffer);

    //Generate the color texture
    glGenTextures(1, &ColorTexture);
    glBindTexture(GL_TEXTURE_2D, ColorTexture);
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
    glBindTexture(GL_TEXTURE_2D, 0);

    //Generate the depth texture
    glGenTextures(1, &DepthTexture);
    glBindTexture(GL_TEXTURE_2D, DepthTexture);
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
    glBindTexture(GL_TEXTURE_2D, 0);

    Terrain->initialize();
    LinePlot->initialize();

    glEnableClientState(GL_VERTEX_ARRAY); // activate vertex coords array
}

void cwGLRenderer::resizeGL() {
    if(GLWidget == NULL) { return; }
    if(width() == 0.0 || height() == 0.0) { return; }
    QSize framebufferSize(width(), height());

    if(!framebufferSize.isValid()) { return; }

    //Recreate the multisample framebuffer
    delete MultiSampleFramebuffer;
    QGLFramebufferObjectFormat multiSampleFormat;
    multiSampleFormat.setSamples(8);
    multiSampleFormat.setAttachment(QGLFramebufferObject::Depth);
    MultiSampleFramebuffer = new QGLFramebufferObject(framebufferSize, multiSampleFormat);

    glBindTexture(GL_TEXTURE_2D, ColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 framebufferSize.width(), framebufferSize.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, DepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 framebufferSize.width(), framebufferSize.height(), 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glBindFramebuffer(GL_FRAMEBUFFER, TextureFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ColorTexture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, TextureFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthTexture, 0);

    glBindTexture(GL_TEXTURE_2D, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE) {
        qDebug() << "Can't complete framebuffer:" << TextureFramebuffer;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //Update the viewport
    QRect viewportRect(QPoint(0, 0), framebufferSize);
    Camera->setViewport(viewportRect);

    QMatrix4x4 projectionMatrix;
    projectionMatrix.perspective(55, width() / (float)height(), 1, 10000);
    Camera->setProjectionMatrix(projectionMatrix);

    update();
}

/**
  \brief Sets the QGLWidget for this renderer

  This assumes that the widget's context is created and is valid.
  */
void cwGLRenderer::setGLWidget(QGLWidget* widget) {
    if(widget == GLWidget) {
        return;
    }

    qDebug() << "Initializing!";

    //Initialize the object here!
    GLWidget = widget;
    initializeGL();

    emit glWidgetChanged();

    resizeGL();
}

/**
  Initializes the last click for the panning state
  */
void cwGLRenderer::startPanning(QPoint position) {
    QPoint mappedPosition = Camera->mapToGLViewport(position);
    LastMouseGlobalPosition = unProject(mappedPosition);
}

/**
  Pans the view allow the a plane
  */
void cwGLRenderer::pan(QPoint position) {
    QPoint mappedPos = Camera->mapToGLViewport(position);

    //Get the ray from the front of the screen to the back of the screen
    QVector3D front = Camera->unProject(mappedPos, 0.0);
    QVector3D back = Camera->unProject(mappedPos, 1.0);

    //Create the ray that'll intersect
    cwLine3D ray(front, back);

    //Create the plane
    cwPlane xyPlane(LastMouseGlobalPosition,
                    LastMouseGlobalPosition + QVector3D(1.0, 0.0, 0.0),
                    LastMouseGlobalPosition + QVector3D(0.0, 1.0, 0.0));

    //Find the intsection on the plane
    bool hasIntersection;
    QVector3D intersection = xyPlane.intersectAsRay(ray, &hasIntersection);



    //Ray and plane are parallel, can't do anything with this
    if(!hasIntersection) { return; }

    QVector3D translateAmount = intersection - LastMouseGlobalPosition;

    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(translateAmount);
    Camera->setViewMatrix(viewMatrix);

    //Update the gl window
    update();
}

/**
  Called when the rotation is about to begin
  */
void cwGLRenderer::startRotating(QPoint position) {
    position = Camera->mapToGLViewport(position);
    LastMouseGlobalPosition = unProject(position);
    LastMousePosition = position;
}

/**
  Rotates the view
  */
void cwGLRenderer::rotate(QPoint position) {
    position = Camera->mapToGLViewport(position);
    QPoint currentMousePosition = position;
    QPointF delta = LastMousePosition - currentMousePosition;
    LastMousePosition = currentMousePosition;
    delta /= 2.0;

    //Calculate the new pitch
    Pitch = qMin(90.0f, qMax(-90.0f, Pitch + (float)delta.y()));

    //Calculate the new azimuth
    Azimuth = fmod(Azimuth - delta.x(), 360);

    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, Pitch);
    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, Azimuth);
    QQuaternion newQuat =  pitchQuat * azimuthQuat;
    QQuaternion rotationDifferance = CurrentRotation.conjugate() * newQuat;
    CurrentRotation = newQuat;

    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(LastMouseGlobalPosition);
    viewMatrix.rotate(rotationDifferance);
    viewMatrix.translate(-LastMouseGlobalPosition);
    Camera->setViewMatrix(viewMatrix);

    update();
}

void cwGLRenderer::wheelEvent(QGraphicsSceneWheelEvent *event) {
    zoom(event);
    event->accept();
}

/**
  Zooms the view
  */
void cwGLRenderer::zoom(QGraphicsSceneWheelEvent* event) {

    //Make the event position into gl viewport
    QPoint mappedPos = Camera->mapToGLViewport(event->pos().toPoint());

    //Get the ray from the front of the screen to the back of the screen
    QVector3D front = Camera->unProject(mappedPos, 0.0);

    //Find the intsection on the plane
    QVector3D intersection = unProject(mappedPos);

    //Smallray
    QVector3D ray = intersection - front;
    float rayLength = ray.length();
    ray.normalize();

    float t = .0005 * -event->delta();
    t = qMax(-1.0f, qMin(1.0f, t));
    t = rayLength * t;

    QVector3D newPositionDelta = ray * t;

    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(newPositionDelta);
    Camera->setViewMatrix(viewMatrix);

    update();
}

void cwGLRenderer::resetView() {
//    QMatrix4x4 projectionMatrix;
//    projectionMatrix.perspective(55, 1.0, .1, 10000); //ortho(-2.0, 2.0, -2.0, 2.0, -2.0, 2.0);
//    Camera->setProjectionMatrix(projectionMatrix);

    Pitch = 90.0;
    Azimuth = 0.0;

    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, Pitch);
    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, Azimuth);

    CurrentRotation =  pitchQuat * azimuthQuat;

    QMatrix4x4 viewMatrix;
    viewMatrix.translate(QVector3D(0.0, 0.0, -2));
    Camera->setViewMatrix(viewMatrix);

    update();
}

/**
  Unprojects the screen point at point and returns a QVector3d in world coordinates
  */
QVector3D cwGLRenderer::unProject(QPoint point) {
    //Sample the depth buffer
    float depth = sampleDepthBuffer(point);

    //Unproject the point
    return Camera->unProject(point, depth);
}

/**
  Samples the depth buffer using a 3 by 3 filter

  This returns an average of valid samples in that area.  If no samples are valid (when the
  user clicks on an empty region for example), then this will always return 1.0.

  The return value will be from 0.0 to 1.0
  */
float cwGLRenderer::sampleDepthBuffer(QPoint point) {

    const int samplerSize = 3;
    const int samplerCenter = samplerSize / 2;
    const QRect samplerArea(QPoint(point.x() - samplerCenter, point.y() - samplerCenter),
                        QSize(samplerSize, samplerSize));

    //Allocate memory
    QVector<float> buffer;
    int bufferSize = samplerArea.width() * samplerArea.height();
    buffer.reserve(bufferSize);
    buffer.resize(bufferSize);

    //Get data from opengl framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, TextureFramebuffer);
    glReadPixels(samplerArea.x(), samplerArea.y(), //where
                 samplerArea.width(), samplerArea.height(), //size
                 GL_DEPTH_COMPONENT, //what buffer
                 GL_FLOAT, //Returned data type
                 buffer.data()); //The buffer for the data
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    float sum = 0.0;
    int count = 0;
    for(int i = 0; i < buffer.size(); i++) {
        //Make sure we're in range
        if(buffer[i] <= .9999999f && buffer[i] > 0.0f) {
            sum += buffer[i];
            count++;
        }
    }

    if(count > 0) {
        //We have at least one valid sample
        return sum / (float)count;
    } else {
        //Use the middle value in the buffer
        int centerIndex = samplerSize * samplerCenter + samplerCenter;
        return buffer[centerIndex];
    }
}

/**
  \brief This renders the station labels for the caving region

  This will go through all the caves and render station labels for them
  */
void cwGLRenderer::renderStationLabels(QPainter* painter) {
    //Clear the collision lable kd-tree
    LabelKdTree.clear();

    for(int i = 0; i < Region->caveCount(); i++) {
       cwCave* cave = Region->cave(i);
       renderStationLabels(painter, cave);
    }

    LabelKdTree.paintTree(painter, QRect(QPoint(0, 0), QSize(width(), height())));
}

/**
  \brief This renders the station labels for a cave
  */
void cwGLRenderer::renderStationLabels(QPainter* painter, cwCave* cave) {

    QList <QWeakPointer<cwStation> > stations = cave->stations();

    //Transforms all the station's points
    QList<QVector3D> transformedStationPoints = QtConcurrent::blockingMapped(stations,
                                                                             TransformPoint(Camera->viewProjectionMatrix(),
                                                                                            Camera->viewport()));


    Q_ASSERT(transformedStationPoints.size() == cave->stations().count());

    QFont defaultFont;
    QFontMetrics fontMetrics(defaultFont);

    //Go through all the station points and render the text
    for(int i = 0; i < stations.size(); i++) {
        QVector3D projectedStationPosition = transformedStationPoints[i];

        //Clip the stations to the rendering area
        if(projectedStationPosition.z() > 1.0 ||
                projectedStationPosition.z() < 0.0 ||
                !Camera->viewport().contains(projectedStationPosition.x(), projectedStationPosition.y())) {
            continue;
        }

        QString stationName = stations[i].data()->name();

        //See if stationName overlaps with other stations
        QPoint topLeftPoint = projectedStationPosition.toPoint();
        QSize stationNameTextSize = fontMetrics.size(Qt::TextSingleLine, stationName);
        QRect stationRect(topLeftPoint, stationNameTextSize);
        stationRect.moveTop(stationRect.top() - stationNameTextSize.height() / 1.5);
        bool couldAddText = LabelKdTree.addRect(stationRect);

        if(couldAddText) {
            painter->drawText(topLeftPoint, stationName);
        }
    }
}

/**
  \brief This is a helper for QtCurrentent function

  This is the kernel for multi threaded algroithm to transform the points into
  screen coordinates.  This is a helper function to renderStationLabels
  */
QVector3D cwGLRenderer::TransformPoint::operator()(QWeakPointer<cwStation> station) {
    QVector3D normalizeSceenCoordinate =  ModelViewProjection * station.data()->position();
    QVector3D viewportCoord = cwCamera::mapNormalizeScreenToGLViewport(normalizeSceenCoordinate, Viewport);
    return viewportCoord;
}

/**
  \brief Sets the caving region for the renderer
  */
void cwGLRenderer::setCavingRegion(cwCavingRegion* region) {
    if(Region != region) {
        Region = region;
        emit cavingRegionChanged();
    }
}
