/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwGLViewer.h"
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
#include "cwProjection.h"
#include "cwScene.h"

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
#include <QRay3D>
#include <QGLFunctions>

const float cw3dRegionViewer::DefaultPitch = 90.0f;
const float cw3dRegionViewer::DefaultAzimuth = 0.0f;

cw3dRegionViewer::cw3dRegionViewer(QQuickItem *parent) :
    cwGLViewer(parent)
{
    ZoomLevel = 1.0; //1 pixel = 1 meter

    //TODO: Not all Mac and Windows support multi-sampling
//#ifndef Q_OS_WIN
//    setAntialiasing(true);
//#endif

    setupInteractionTimers();

    resetView();
}

/**
 * @brief cw3dRegionViewer::setGridPlane
 * @param plan
 *
 * This is for interaction. If the mouse can intersect with any
 * other geometry, this will intesect with this plane so rotation work correctly.
 */
void cw3dRegionViewer::setGridPlane(const QPlane3D &plan)
{
    LastDitchRotationPlane = plan;
}

///**
//  \brief Initilizes the viwer
//  */
//void cw3dRegionViewer::initializeGL() {
//    //Initilizes the cameras
//    resetView();

//    Terrain->initialize();
//    Scraps->initialize();
//    LinePlot->initialize();
//    Plane->initialize();
//}


/**
  \brief Called when the viewer's size changes

  This updates the projection matrix for the view
  */
void cw3dRegionViewer::resizeGL() {
    emit resized();
//    cwProjection projection = orthoProjection();
//    Camera->setProjection(projection);
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
    double t = scene()->geometryItersecter()->intersects(ray);

    if(qIsNaN(t)) {

        //See where it intersects ground plane geometry
        t = LastDitchRotationPlane.intersection(ray);
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
//QSGNode *cw3dRegionViewer::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData * data) {
//    if(LinePlot->isDirty()) {
//        LinePlot->updateData();
//    }

//    if(Scraps->isDirty()) {
//        Scraps->updateData();
//    }

//    return cwGLViewer::updatePaintNode(oldNode, data);
//}


/**
  Initializes the last click for the panning state
  */
void cw3dRegionViewer::startPanning(QPoint position) {
    QPoint mappedPosition = Camera->mapToGLViewport(position);
    LastMouseGlobalPosition = unProject(mappedPosition);
//    LastMouseGlobalPosition = QVector3D(mappedPosition);

    //Get the ray from the front of the screen to the back of the screen
    //Using the center of the screen
    QMatrix4x4 eyeMatrix = camera()->viewProjectionMatrix().inverted();


    QVector3D front = eyeMatrix.map(QVector3D(0.0, 0.0, -1.0));
    QVector3D back = eyeMatrix.map(QVector3D(0.0, 0.0, 1.0));

    QVector3D direction = QVector3D(front - back).normalized();

    //Create the plane
    PanPlane = QPlane3D(LastMouseGlobalPosition, direction);

    TranslatePosition = position;
    translateLastPosition();

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
    Pitch = DefaultPitch;
    Azimuth = DefaultAzimuth;

    setCurrentRotation(defaultRotation());

    QMatrix4x4 viewMatrix;
    viewMatrix.translate(QVector3D(0.0, 0.0, -50));
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
    QQuaternion newQuat = pitchQuat * azimuthQuat;
    QQuaternion rotationDifferance = CurrentRotation.conjugate() * newQuat;
    setCurrentRotation(newQuat);

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
    switch(Camera->projection().type()) {
    case cwProjection::Perspective:
    case cwProjection::PerspectiveFrustum:
        zoomPerspective();
        break;
    case cwProjection::Ortho:
        zoomOrtho();
        break;
    default:
        qDebug() << "Can't zoom because of an unknown projection";
        return;
    }
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

    QVector3D direction = front - back;
    direction.normalize();

    //Create the ray that'll intersect
    QRay3D ray(front, direction);

    //Find the intsection on the plane
    double t = PanPlane.intersection(ray); //xyPlane.intersectAsRay(ray, &hasIntersection);

    //Ray and plane are parallel, can't do anything with this
    if(qIsNaN(t)) { return; }

    QVector3D intersection = ray.point(t);
    QVector3D translateAmount = intersection - LastMouseGlobalPosition;

    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(translateAmount);
    Camera->setViewMatrix(viewMatrix);

    //Update the gl window
    update();
}

///**
//  \brief Sets the caving region for the renderer
//  */
//void cw3dRegionViewer::setCavingRegion(cwCavingRegion* region) {
//    if(Region != region) {
//        Region = region;
//        emit cavingRegionChanged();
//    }
//}

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
    int interactionInterval = 15; //1/60; //60hz
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

void cw3dRegionViewer::setCurrentRotation(QQuaternion rotation)
{
    CurrentRotation = rotation;
    emit rotationChanged();
}

QQuaternion cw3dRegionViewer::defaultRotation() const
{
    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, DefaultPitch);
    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, DefaultAzimuth);
    return pitchQuat * azimuthQuat;
}

/**
 * @brief cw3dRegionViewer::zoomPerspective
 *
 * Zoom with a perspective transfromation
 */
void cw3dRegionViewer::zoomPerspective()
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
}

/**
 * @brief cw3dRegionViewer::zoomOrtho
 *
 * Zoom with an orthoganal projection
 */
void cw3dRegionViewer::zoomOrtho()
{
    int delta = ZoomDelta;
    QPoint position = ZoomPosition;

    //Make the event position into gl viewport
    QPoint mappedPos = Camera->mapToGLViewport(position);

    double direction = delta > 0 ? 1.1 : 0.9;
    ZoomLevel = ZoomLevel * direction;

    QVector3D before = Camera->unProject(mappedPos, 1.0);

    cwProjection projection = orthoProjection();
    Camera->setProjection(orthoProjection());

    QVector3D after = Camera->unProject(mappedPos, 1.0);

    QVector3D changeInPosition = after - before;
    QMatrix4x4 newTranslationMatrix = Camera->viewMatrix();
    newTranslationMatrix.translate(changeInPosition);
    Camera->setViewMatrix(newTranslationMatrix);
}

/**
 * @brief cw3dRegionViewer::orthoProjection
 * @return This get the current ortho projection at the current zoom level
 */
cwProjection cw3dRegionViewer::orthoProjection() const
{
    cwProjection projection;
    projection.setOrtho(-width() / 2.0 * ZoomLevel, width() / 2.0 * ZoomLevel, -height() / 2.0 * ZoomLevel, height() / 2.0 * ZoomLevel, -10000, 10000);
    return projection;
}

/**
 * @brief cw3dRegionViewer::perspectiveProjection
 * @return The current prespective projection for the viewer
 */
cwProjection cw3dRegionViewer::perspectiveProjection() const
{
    cwProjection projection;
    projection.setPerspective(55, width() / (float)height(), 1, 10000);
    return projection;
}

/**
Gets rotation current global rotation
*/
QQuaternion cw3dRegionViewer::rotation() const {
    return defaultRotation().conjugate() * CurrentRotation;
}
