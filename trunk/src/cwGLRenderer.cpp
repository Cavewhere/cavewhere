//Glew includes
#include "GL/glew.h"

//Our includes
#include "cwGLRenderer.h"
#include "cwCamera.h"
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

//Std includes
#include <math.h>


cwGLRenderer::cwGLRenderer(QDeclarativeItem *parent) :
    QDeclarativeItem(parent)
{
    GLWidget = NULL;
    setFlag(QGraphicsItem::ItemHasNoContents, false);

    setupInteractionStateMachine();

    Camera = new cwCamera(this);
    ShaderDebugger = new cwShaderDebugger(this);

    Terrain = new cwGLTerrain(this);
    Terrain->setCamera(Camera);
    Terrain->setShaderDebugger(ShaderDebugger);
    Terrain->setNumberOfLevels(10);
    connect(Terrain, SIGNAL(redraw()), SLOT(updateGL()));

    connect(this, SIGNAL(widthChanged()), SLOT(resizeGL()));
    connect(this, SIGNAL(heightChanged()), SLOT(resizeGL()));


}

void cwGLRenderer::paint(QPainter* painter, const QStyleOptionGraphicsItem *, QWidget *) {

    painter->beginNativePainting();

    //Draw everything to a framebuffer
    MultiSampleFramebuffer->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Terrain->draw();
    MultiSampleFramebuffer->release();

    //Copy the MultiSampleFramebuffer data to the textureFramebuffer
    QGLFramebufferObject::blitFramebuffer(TextureFramebuffer,
                                          QRect(QPoint(0,0), TextureFramebuffer->size()),
                                          MultiSampleFramebuffer,
                                          QRect(QPoint(0,0), MultiSampleFramebuffer->size()));

    //Draw the texture that the TextureFramebuffer just rendered to
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, TextureFramebuffer->texture());
    glBegin(GL_QUADS);
    glTexCoord2f(0.0, 0.0);
    glVertex2f(0, 0);
    glTexCoord2f(0.0, 1.0);
    glVertex2f(0, height());
    glTexCoord2f(1.0, 1.0);
    glVertex2f(width(), height());
    glTexCoord2f(1.0, 0.0);
    glVertex2f(width(), 0);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);

    painter->endNativePainting();

}

/**
  \brief This initializes this item's opengl stuff
  */
void cwGLRenderer::initializeGL() {
    GLWidget->makeCurrent();

    //Setup the camera
    resetView();

    MultiSampleFramebuffer = new QGLFramebufferObject(QSize(1, 1));
    TextureFramebuffer = new QGLFramebufferObject(QSize(1, 1));

    //Generate the signle texture
//    TextureFramebuffer->drawTexture(QPointF(0.0, 0.0));
//    glGenTextures(1, &Texture);
//    glBindTexture(GL_TEXTURE_2D, Texture);
//    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
//    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
//    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
//    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
//    glBindTexture(GL_TEXTURE_2D, 0);

    Terrain->initalize();

   // glEnableClientState(GL_VERTEX_ARRAY); // activate vertex coords array
}

void cwGLRenderer::resizeGL() {

    QSize framebufferSize(width(), height());

    //Recreate the multisample framebuffer
    delete MultiSampleFramebuffer;
    QGLFramebufferObjectFormat multiSampleFormat;
    multiSampleFormat.setSamples(4);
    multiSampleFormat.setAttachment(QGLFramebufferObject::Depth);
    MultiSampleFramebuffer = new QGLFramebufferObject(framebufferSize, multiSampleFormat);

//    glBindTexture(GL_TEXTURE_2D, Texture);
//    glTexImage2D(GL_TEXTURE, 0, GL_RGBA, framebufferSize.width(), framebufferSize.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    delete TextureFramebuffer;
    QGLFramebufferObjectFormat textureFramebufferFormat;
//    textureFramebufferFormat.setTextureTarget(Texture);
    textureFramebufferFormat.setAttachment(QGLFramebufferObject::Depth);
    TextureFramebuffer = new QGLFramebufferObject(framebufferSize, textureFramebufferFormat);

    //Update the viewport
    QRect viewportRect(QPoint(x(), y()), QSize(width(), height()));
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

    //Initialize the object here!
    GLWidget = widget;
    initializeGL();

    emit glWidgetChanged();
}

/**
  Initializes the last click for the panning state
  */
void cwGLRenderer::startPanning(QMouseEvent* event) {
    LastMouseGlobalPosition = unProject(event->pos());
    qDebug() << "Clicked: " << LastMouseGlobalPosition;
}

/**
  Pans the view allow the a plane
  */
void cwGLRenderer::pan(QMouseEvent* event) {
    QPoint mappedPos = Camera->mapToGLViewport(event->pos());

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
void cwGLRenderer::startRotating(QMouseEvent* event) {
    LastMouseGlobalPosition = unProject(event->pos());
    LastMousePosition = event->pos();
}

/**
  Rotates the view
  */
void cwGLRenderer::rotate(QMouseEvent* event) {
    QPoint currentMousePosition = event->pos();
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

/**
  Zooms the view
  */
void cwGLRenderer::zoom(QWheelEvent* event) {

    //Make the event position into gl viewport
    QPoint mappedPos = Camera->mapToGLViewport(event->pos());

    //Get the ray from the front of the screen to the back of the screen
    QVector3D front = Camera->unProject(mappedPos, 0.0);
//    QVector3D back = Camera->unProject(mappedPos, 1.0);

    //Find the intsection on the plane
    QVector3D intersection = unProject(event->pos());

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
    QMatrix4x4 projectionMatrix;
    projectionMatrix.perspective(55, 1.0, .1, 5000); //ortho(-2.0, 2.0, -2.0, 2.0, -2.0, 2.0);
    Camera->setProjectionMatrix(projectionMatrix);

    Pitch = 90.0;
    Azimuth = 0.0;

    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, Pitch);
    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, Azimuth);

    CurrentRotation =  pitchQuat * azimuthQuat;

    QMatrix4x4 viewMatrix;
    viewMatrix.translate(QVector3D(0.0, 0.0, -20));
    Camera->setViewMatrix(viewMatrix);

    update();
}

/**
  Unprojects the screen point at point and returns a QVector3d in world coordinates
  */
QVector3D cwGLRenderer::unProject(QPoint point) {
    //Flip the point's y
    point = Camera->mapToGLViewport(point);

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
    glReadPixels(samplerArea.x(), samplerArea.y(), //where
                 samplerArea.width(), samplerArea.height(), //size
                 GL_DEPTH_COMPONENT, //what buffer
                 GL_FLOAT, //Returned data type
                 buffer.data()); //The buffer for the data

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
  Sets up the camera for the view
  */
void cwGLRenderer::setupCamera() {
    Camera = new cwCamera(this);

    resetView();
}



/**
  Setup the statemachine for the interaction for the viewer
  */
 void cwGLRenderer::setupInteractionStateMachine() {
     InteractionMachine = new QStateMachine(this);

     DefaultInteractionState = new QState(InteractionMachine);
     QState* noState = new QState(DefaultInteractionState);

     setupPan();
     setupRotate();
     setupZoom();

     DefaultInteractionState->setInitialState(noState);
     InteractionMachine->setInitialState(DefaultInteractionState);

     InteractionMachine->start();
 }

 /**
   Setup the pan state machine
   */
 void cwGLRenderer::setupPan() {
     QState* panState = new QState(DefaultInteractionState);

     cwMouseEventTransition* enterPanTransition = new cwMouseEventTransition(this,
                                                    QEvent::MouseButtonPress,
                                                    Qt::LeftButton);
     connect(enterPanTransition, SIGNAL(onMouseEvent(QMouseEvent*)), SLOT(startPanning(QMouseEvent*)));

     enterPanTransition->setTargetState(panState);
     DefaultInteractionState->addTransition(enterPanTransition);

     //Circular pan event
     cwMouseEventTransition* panTransition = new cwMouseEventTransition(this,
                                                QEvent::MouseMove,
                                                Qt::NoButton);

     panState->addTransition(panTransition);
     connect(panTransition, SIGNAL(onMouseEvent(QMouseEvent*)), SLOT(pan(QMouseEvent*)));

     QMouseEventTransition* mouseReleaseTransition = new QMouseEventTransition(this,
                                                                               QEvent::MouseButtonRelease,
                                                                               Qt::LeftButton);
     mouseReleaseTransition->setTargetState(DefaultInteractionState);
     panState->addTransition(mouseReleaseTransition);


 }

 /**
   Setup the rotation interaction state machine
   */
 void cwGLRenderer::setupRotate() {
     QState* rotateState = new QState(DefaultInteractionState);

     cwMouseEventTransition* enterRotateTransition = new cwMouseEventTransition(this,
                                                        QEvent::MouseButtonPress,
                                                        Qt::RightButton);
     connect(enterRotateTransition, SIGNAL(onMouseEvent(QMouseEvent*)), SLOT(startRotating(QMouseEvent*)));
     enterRotateTransition->setTargetState(rotateState);
     DefaultInteractionState->addTransition(enterRotateTransition);

     //Circular rotation event
     cwMouseEventTransition* rotateTransition = new cwMouseEventTransition(this,
                                                   QEvent::MouseMove,
                                                   Qt::NoButton);

     rotateState->addTransition(rotateTransition);
     connect(rotateTransition, SIGNAL(onMouseEvent(QMouseEvent*)), SLOT(rotate(QMouseEvent*)));

     QMouseEventTransition* mouseReleaseTransition = new QMouseEventTransition(this,
                                                                               QEvent::MouseButtonRelease,
                                                                               Qt::RightButton);
     mouseReleaseTransition->setTargetState(DefaultInteractionState);
     rotateState->addTransition(mouseReleaseTransition);
 }

 void cwGLRenderer::setupZoom() {
     cwWheelEventTransition* wheelEventTrasition = new cwWheelEventTransition(this,
                                                                              QEvent::Wheel);
     connect(wheelEventTrasition, SIGNAL(onWheelEvent(QWheelEvent*)), SLOT(zoom(QWheelEvent*)));
     DefaultInteractionState->addTransition(wheelEventTrasition);


     QKeyEventTransition* resetTransition = new QKeyEventTransition(this,
                                                                    QEvent::KeyPress,
                                                                    Qt::Key_R);
     connect(resetTransition, SIGNAL(triggered()), SLOT(resetView()));
     DefaultInteractionState->addTransition(resetTransition);

 }
