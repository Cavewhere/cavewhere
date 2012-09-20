//Our includes
#include "cwGLRenderer.h"
#include "cwCamera.h"
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
#include "cw3dRegionViewer.h"
#include "cwGLScraps.h"
#include "cwStationPositionLookup.h"
#include "cwGLGridPlane.h"
#include "cwGeometryItersecter.h"

//Qt includes
#include <QPainter>
#include <QString>
#include <QVector3D>
#include <QRect>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QtConcurrentMap>
#include <QFontMetrics>
#include <QQuickCanvas>
#include <QRay3D>

cw3dRegionViewer::cw3dRegionViewer(QQuickItem *parent) :
    cwGLRenderer(parent)
{
    Region = NULL;

    Terrain = new cwGLTerrain();
    Terrain->setNumberOfLevels(10);
    //    connect(Terrain, SIGNAL(redraw()), SLOT(updateGL()));

    LinePlot = new cwGLLinePlot();
    Scraps = new cwGLScraps();
    Plane = new cwGLGridPlane();

    Terrain->setScene(this);
    LinePlot->setScene(this);
    Scraps->setScene(this);
    Plane->setScene(this);

    setupInteractionTimers();
}

/**
  \brief This paints the cwGLRender and the linePlot labels
  */
void cw3dRegionViewer::paint(QPainter * painter) {
    //Run paint Framebuffer
    //cwGLRenderer::paint(painter, item, widget);

    painter->beginNativePainting();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    //    Terrain->draw();
    Scraps->draw();
    LinePlot->draw();
    Plane->draw();

    glDisable(GL_DEPTH_TEST);

    painter->endNativePainting();
}

/**
  \brief Initilizes the viwer
  */
void cw3dRegionViewer::initializeGL() {
    //Initilizes the cameras
    resetView();

    Terrain->initialize();
    Scraps->initialize();
    LinePlot->initialize();
    Plane->initialize();

    glEnableClientState(GL_VERTEX_ARRAY); // activate vertex coords array
}

/**
  \brief Called when the viewer's size changes

  This updates the projection matrix for the view
  */
void cw3dRegionViewer::resizeGL() {
    QMatrix4x4 projectionMatrix;
    //projectionMatrix.ortho(-250, 250, -250, 250, -10000, 10000);
    projectionMatrix.perspective(55, width() / (float)height(), 1, 10000);
    Camera->setProjectionMatrix(projectionMatrix);
}

/**
 * @brief cw3dRegionViewer::unProject
 * @param point
 * @return  QVector3d in world coordinates
 *
 * Unprojects the screen point at point and returns a QVector3d in world coordinates
 */
QVector3D cw3dRegionViewer::unProject(QPoint point) {

    //Create a ray from the back projection front and back plane
    QVector3D frontPoint = Camera->unProject(point, 0.0);
    QVector3D backPoint = Camera->unProject(point, 1.0);
    QVector3D direction = QVector3D(backPoint - frontPoint).normalized();
    QRay3D ray(frontPoint, direction);

    //See if it hits any of the scraps
    double t = GeometryItersecter->intersects(ray);

    if(qIsNaN(t)) {

        //See where it intersects ground plane geometry
        t = Plane->plane().intersection(ray);
        if(qIsNaN(t)) {
            return QVector3D();
        }

    }

    return ray.point(t);
}

/**
 * @brief cw3dRegionViewer::updatePaintNode
 *
 * This is called by the rendering thread and but the main thread is blocked
 *
 * @param oldNode
 * @param data
 * @return
 */
QSGNode *cw3dRegionViewer::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData * data) {
    if(LinePlot->isDirty()) {
        LinePlot->updateData();
    }

    if(Scraps->isDirty()) {
        Scraps->updateData();
    }

    return cwGLRenderer::updatePaintNode(oldNode, data);
}


/**
  Initializes the last click for the panning state
  */
void cw3dRegionViewer::startPanning(QPoint position) {
    QPoint mappedPosition = Camera->mapToGLViewport(position);
    LastMouseGlobalPosition = unProject(mappedPosition);
}

/**
  Pans the view allow the a plane
  */
void cw3dRegionViewer::pan(QPoint position) {
    TranslatePosition = position;
    if(!TranslateTimer->isActive()) {
        TranslateTimer->start();
    }
}

/**
  Called when the rotation is about to begin
  */
void cw3dRegionViewer::startRotating(QPoint position) {
    position = Camera->mapToGLViewport(position);
    LastMouseGlobalPosition = unProject(position);
    LastMousePosition = position;
}

/**
  Rotates the view
  */
void cw3dRegionViewer::rotate(QPoint position) {
    TimeoutRotationPosition = position;
    if(!RotationInteractionTimer->isActive()) {
        RotationInteractionTimer->start();
    }
}

/**
  Zooms the view
  */
void cw3dRegionViewer::zoom(QPoint position, int delta) {
    ZoomPosition = position;
    if(!ZoomInteractionTimer->isActive()) {
        ZoomInteractionTimer->start();
        ZoomDelta = delta;
    } else {
        ZoomDelta += delta;
    }
}

/**
  \brief Resets the view
  */
void cw3dRegionViewer::resetView() {
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
 * @brief cw3dRegionViewer::rotateLastPosition
 *
 * This is called by the timer, after the timer has consumed mouse events.  This prevents
 * rendering thread to be starved
 */
void cw3dRegionViewer::rotateLastPosition()
{
    QPoint position = TimeoutRotationPosition;

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

/**
 * @brief cw3dRegionViewer::zoomLastPosition
 *
 * This is called by the timer, after the timer has consumed mouse events.  This prevents
 * rendering thread to be starved
 */
void cw3dRegionViewer::zoomLastPosition()
{
    int delta = ZoomDelta;
    QPoint position = ZoomPosition;

    //Make the event position into gl viewport
    QPoint mappedPos = Camera->mapToGLViewport(position);

    //Get the ray from the front of the screen to the back of the screen
    QVector3D front = Camera->unProject(mappedPos, 0.0);

    //Find the intsection on the plane
    QVector3D intersection = unProject(mappedPos);

    //Smallray
    QVector3D ray = intersection - front;
    float rayLength = ray.length();
    ray.normalize();

    float t = .0005 * delta;
    t = qMax(-1.0f, qMin(1.0f, t));
    t = rayLength * t;

    QVector3D newPositionDelta = ray * t;

    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(newPositionDelta);
    Camera->setViewMatrix(viewMatrix);

    update();
}

/**
 * @brief cw3dRegionViewer::translateLastPosition
 *
 * This is called by the timer, after the timer has consumed mouse events.  This prevents
 * rendering thread to be starved
 */
void cw3dRegionViewer::translateLastPosition()
{
    QPoint position = TranslatePosition;

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
  \brief Sets the caving region for the renderer
  */
void cw3dRegionViewer::setCavingRegion(cwCavingRegion* region) {
    if(Region != region) {
        Region = region;
        emit cavingRegionChanged();
    }
}

/**
 * @brief cw3dRegionViewer::setupInteractionTimers
 *
 * This sets up the interaction timers.  This prevents the operating system from firing more
 * mouse events than it needs to.
 *
 * On linux, mouse events are fired very, very quickly, causing the rendering thread to strave
 *
 * The timers prevent the starvation
 */
void cw3dRegionViewer::setupInteractionTimers()
{
    int interactionInterval = 1/60; //60hz
    RotationInteractionTimer = new QTimer(this);
    ZoomInteractionTimer = new QTimer(this);
    TranslateTimer = new QTimer(this);

    RotationInteractionTimer->setInterval(interactionInterval);
    RotationInteractionTimer->setSingleShot(true);

    ZoomInteractionTimer->setInterval(interactionInterval);
    ZoomInteractionTimer->setSingleShot(true);

    TranslateTimer->setInterval(interactionInterval);
    TranslateTimer->setSingleShot(true);

    connect(RotationInteractionTimer, &QTimer::timeout, this, &cw3dRegionViewer::rotateLastPosition);
    connect(ZoomInteractionTimer, &QTimer::timeout, this, &cw3dRegionViewer::zoomLastPosition);
    connect(TranslateTimer, &QTimer::timeout, this, &cw3dRegionViewer::translateLastPosition);
}
