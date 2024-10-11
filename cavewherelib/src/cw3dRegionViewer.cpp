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
#include "cwOrthogonalProjection.h"
#include "cwPerspectiveProjection.h"

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
#include <QOpenGLFunctions>

//const float cw3dRegionViewer::DefaultPitch = 90.0f;
//const float cw3dRegionViewer::DefaultAzimuth = 0.0f;

cw3dRegionViewer::cw3dRegionViewer(QQuickItem *parent) :
    cwGLViewer(parent)
{
    //TODO: Not all Mac and Windows support multi-sampling
//#ifndef Q_OS_WIN
//    setAntialiasing(true);
//#endif

    OrthognalProjection = new cwOrthogonalProjection(this);
    OrthognalProjection->setViewer(this);
    OrthognalProjection->setEnabled(true);

    PerspectiveProjection = new cwPerspectiveProjection(this);
    PerspectiveProjection->setViewer(this);
    PerspectiveProjection->setEnabled(false);
}

/**
  \brief Called when the viewer's size changes

  This updates the projection matrix for the view
  */
void cw3dRegionViewer::resizeGL() {
    emit resized();
}

/**
* @brief cw3dRegionViewer::orthoProjectionObject
* @return
*/
 cwOrthogonalProjection* cw3dRegionViewer::orthoProjection() const {
    return OrthognalProjection;
}

/**
* @brief cw3dRegionViewer::perspectiveProjectionObject
* @return
*/
 cwPerspectiveProjection* cw3dRegionViewer::perspectiveProjection() const {
    return PerspectiveProjection;
}


///**
//  Initializes the last click for the panning state
//  */
//void cw3dRegionViewer::startPanning(QPoint position) {
//    QPoint mappedPosition = Camera->mapToGLViewport(position);
//    LastMouseGlobalPosition = unProject(mappedPosition);
////    LastMouseGlobalPosition = QVector3D(mappedPosition);

//    //Get the ray from the front of the screen to the back of the screen
//    //Using the center of the screen
//    QMatrix4x4 eyeMatrix = camera()->viewProjectionMatrix().inverted();


//    QVector3D front = eyeMatrix.map(QVector3D(0.0, 0.0, -1.0));
//    QVector3D back = eyeMatrix.map(QVector3D(0.0, 0.0, 1.0));

//    QVector3D direction = QVector3D(front - back).normalized();

//    //Create the plane
//    PanPlane = QPlane3D(LastMouseGlobalPosition, direction);

//    TranslatePosition = position;
//    translateLastPosition();

//}

///**
//  Pans the view allow the a plane
//  */
//void cw3dRegionViewer::pan(QPoint position) {
//    TranslatePosition = position;
//    if(!TranslateTimer->isActive()) {
//        TranslateTimer->start();
//    }
//}

///**
//  Called when the rotation is about to begin
//  */
//void cw3dRegionViewer::startRotating(QPoint position) {
//    position = Camera->mapToGLViewport(position);
//    LastMouseGlobalPosition = unProject(position);
//    LastMousePosition = position;
//}

///**
//  Rotates the view
//  */
//void cw3dRegionViewer::rotate(QPoint position) {
//    TimeoutRotationPosition = position;
//    if(!RotationInteractionTimer->isActive()) {
//        RotationInteractionTimer->start();
//    }
//}

///**
//  Zooms the view
//  */
//void cw3dRegionViewer::zoom(QPoint position, int delta) {
//    ZoomPosition = position;
//    if(!ZoomInteractionTimer->isActive()) {
//        ZoomInteractionTimer->start();
//        ZoomDelta = delta;
//    } else {
//        ZoomDelta += delta;
//    }
//}

///**
//  \brief Resets the view
//  */
//void cw3dRegionViewer::resetView() {
//    Pitch = DefaultPitch;
//    Azimuth = DefaultAzimuth;

//    setCurrentRotation(defaultRotation());

//    QMatrix4x4 viewMatrix;
//    viewMatrix.translate(QVector3D(0.0, 0.0, -50));
//    Camera->setViewMatrix(viewMatrix);

//    update();
//}

///**
// * @brief cw3dRegionViewer::rotateLastPosition
// *
// * This is called by the timer, after the timer has consumed mouse events.  This prevents
// * rendering thread to be starved
// */
//void cw3dRegionViewer::rotateLastPosition()
//{
//    QPoint position = TimeoutRotationPosition;

//    position = Camera->mapToGLViewport(position);
//    QPoint currentMousePosition = position;
//    QPointF delta = LastMousePosition - currentMousePosition;
//    LastMousePosition = currentMousePosition;
//    delta /= 2.0;

//    //Calculate the new pitch
//    Pitch = clampPitch(Pitch + delta.y());
//    emit pitchChanged();

//    //Calculate the new azimuth
//    Azimuth = clampAzimuth(Azimuth - delta.x());
//    emit azimuthChanged();

//    updateRotationMatrix();
//}

///**
// * @brief cw3dRegionViewer::zoomLastPosition
// *
// * This is called by the timer, after the timer has consumed mouse events.  This prevents
// * rendering thread to be starved
// */
//void cw3dRegionViewer::zoomLastPosition()
//{
//    switch(Camera->projection().type()) {
//    case cwProjection::Perspective:
//    case cwProjection::PerspectiveFrustum:
//        zoomPerspective();
//        break;
//    case cwProjection::Ortho:
//        zoomOrtho();
//        break;
//    default:
//        qDebug() << "Can't zoom because of an unknown projection";
//        return;
//    }
//}

///**
// * @brief cw3dRegionViewer::translateLastPosition
// *
// * This is called by the timer, after the timer has consumed mouse events.  This prevents
// * rendering thread to be starved
// */
//void cw3dRegionViewer::translateLastPosition()
//{
//    QPoint position = TranslatePosition;

//    QPoint mappedPos = Camera->mapToGLViewport(position);

//    //Get the ray from the front of the screen to the back of the screen
//    QVector3D front = Camera->unProject(mappedPos, 0.0);
//    QVector3D back = Camera->unProject(mappedPos, 1.0);

//    QVector3D direction = front - back;
//    direction.normalize();

//    //Create the ray that'll intersect
//    QRay3D ray(front, direction);

//    //Find the intsection on the plane
//    double t = PanPlane.intersection(ray); //xyPlane.intersectAsRay(ray, &hasIntersection);

//    //Ray and plane are parallel, can't do anything with this
//    if(qIsNaN(t)) { return; }

//    QVector3D intersection = ray.point(t);
//    QVector3D translateAmount = intersection - LastMouseGlobalPosition;

//    QMatrix4x4 viewMatrix = Camera->viewMatrix();
//    viewMatrix.translate(translateAmount);
//    Camera->setViewMatrix(viewMatrix);

//    //Update the gl window
//    update();
//}

/////**
////  \brief Sets the caving region for the renderer
////  */
////void cw3dRegionViewer::setCavingRegion(cwCavingRegion* region) {
////    if(Region != region) {
////        Region = region;
////        emit cavingRegionChanged();
////    }
////}

///**
// * @brief cw3dRegionViewer::setupInteractionTimers
// *
// * This sets up the interaction timers.  This prevents the operating system from firing more
// * mouse events than it needs to.
// *
// * On linux, mouse events are fired very, very quickly, causing the rendering thread to strave
// *
// * The timers prevent the starvation
// */
//void cw3dRegionViewer::setupInteractionTimers()
//{
//    int interactionInterval = 15; //1/60; //60hz
//    RotationInteractionTimer = new QTimer(this);
//    ZoomInteractionTimer = new QTimer(this);
//    TranslateTimer = new QTimer(this);

//    RotationInteractionTimer->setInterval(interactionInterval);
//    RotationInteractionTimer->setSingleShot(true);

//    ZoomInteractionTimer->setInterval(interactionInterval);
//    ZoomInteractionTimer->setSingleShot(true);

//    TranslateTimer->setInterval(interactionInterval);
//    TranslateTimer->setSingleShot(true);

//    connect(RotationInteractionTimer, &QTimer::timeout, this, &cw3dRegionViewer::rotateLastPosition);
//    connect(ZoomInteractionTimer, &QTimer::timeout, this, &cw3dRegionViewer::zoomLastPosition);
//    connect(TranslateTimer, &QTimer::timeout, this, &cw3dRegionViewer::translateLastPosition);
//}

//void cw3dRegionViewer::setCurrentRotation(QQuaternion rotation)
//{
//    CurrentRotation = rotation;
//    emit rotationChanged();
//}

//QQuaternion cw3dRegionViewer::defaultRotation() const
//{
//    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, DefaultPitch);
//    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, DefaultAzimuth);
//    return pitchQuat * azimuthQuat;
//}

//void cw3dRegionViewer::updateRotationMatrix()
//{
//    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, Pitch);
//    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, Azimuth);
//    QQuaternion newQuat = pitchQuat * azimuthQuat;
//    QQuaternion rotationDifferance = CurrentRotation.conjugate() * newQuat;
//    setCurrentRotation(newQuat);

//    QMatrix4x4 viewMatrix = Camera->viewMatrix();
//    viewMatrix.translate(LastMouseGlobalPosition);
//    viewMatrix.rotate(rotationDifferance);
//    viewMatrix.translate(-LastMouseGlobalPosition);
//    Camera->setViewMatrix(viewMatrix);

//    update();
//}

///**
// * @brief cw3dRegionViewer::clampAzimuth
// * @param azimuth
// * @return This clamps the azimuth between 0.0 and 360.0
// */
//double cw3dRegionViewer::clampAzimuth(double azimuth) const
//{
//    azimuth = fmod(azimuth, 360.0);
//    if(azimuth < 0) {
//        azimuth += 360.0;
//    }
//    return azimuth;
//}

///**
// * @brief cw3dRegionViewer::clampPitch
// * @param pitch
// * @return This clamps the pitch from -90 to 90
// */
//double cw3dRegionViewer::clampPitch(double pitch) const
//{
//    return qMin(90.0, qMax(-90.0, pitch));
//}

///**
// * @brief cw3dRegionViewer::zoomPerspective
// *
// * Zoom with a perspective transfromation
// */
//void cw3dRegionViewer::zoomPerspective()
//{
//    int delta = ZoomDelta;
//    QPoint position = ZoomPosition;

//    //Make the event position into gl viewport
//    QPoint mappedPos = Camera->mapToGLViewport(position);
//    //Get the ray from the front of the screen to the back of the screen
//    QVector3D front = Camera->unProject(mappedPos, 0.0);

//    //Find the intsection on the plane
//    QVector3D intersection = unProject(mappedPos);

//    //Smallray
//    QVector3D ray = intersection - front;
//    float rayLength = ray.length();
//    ray.normalize();

//    float t = .0005 * delta;
//    t = qMax(-1.0f, qMin(1.0f, t));
//    t = rayLength * t;

//    QVector3D newPositionDelta = ray * t;

//    QMatrix4x4 viewMatrix = Camera->viewMatrix();
//    viewMatrix.translate(newPositionDelta);
//    Camera->setViewMatrix(viewMatrix);
//}

///**
// * @brief cw3dRegionViewer::zoomOrtho
// *
// * Zoom with an orthoganal projection
// */
//void cw3dRegionViewer::zoomOrtho()
//{
//    int delta = ZoomDelta;
//    QPoint position = ZoomPosition;

//    //Make the event position into gl viewport
//    QPoint mappedPos = Camera->mapToGLViewport(position);

//    double direction = delta > 0 ? 1.075 : 0.925;
//    double zoomLevel = camera()->zoomScale() * direction;

//    QVector3D before = Camera->unProject(mappedPos, 1.0);

//    Camera->setZoomScale(zoomLevel);

//    QVector3D after = Camera->unProject(mappedPos, 1.0);

//    QVector3D changeInPosition = after - before;
//    QMatrix4x4 newTranslationMatrix = Camera->viewMatrix();
//    newTranslationMatrix.translate(changeInPosition);
//    Camera->setViewMatrix(newTranslationMatrix);
//}

/////**
//// * @brief cw3dRegionViewer::orthoProjection
//// * @return This get the current ortho projection at the current zoom level
//// */
////cwProjection cw3dRegionViewer::orthoProjectionDefault() const
////{
////    cwProjection projection;
////    projection.setOrtho(-width() / 2.0 * ZoomLevel, width() / 2.0 * ZoomLevel, -height() / 2.0 * ZoomLevel, height() / 2.0 * ZoomLevel, -10000, 10000);
////    return projection;
////}

/////**
//// * @brief cw3dRegionViewer::perspectiveProjection
//// * @return The current prespective projection for the viewer
//// */
////cwProjection cw3dRegionViewer::perspectiveProjectionDefault() const
////{
////    cwProjection projection;
////    projection.setPerspective(55, width() / (float)height(), 1, 10000);
////    return projection;
////}

///**
//Gets rotation current global rotation
//*/
//QQuaternion cw3dRegionViewer::rotation() const {
//    return defaultRotation().conjugate() * CurrentRotation;
//}

///**
//* @brief cw3dRegionViewer::setAzimuth
//* @param azimuth
//*/
//void cw3dRegionViewer::setAzimuth(double azimuth) {
//    azimuth = clampAzimuth(azimuth);
//    if(Azimuth != azimuth) {
//        Azimuth = azimuth;
//        emit azimuthChanged();

//        updateRotationMatrix();
//    }
//}


///**
//* @brief cw3dRegionViewer::setPitch
//* @param pitch
//*/
//void cw3dRegionViewer::setPitch(double pitch) {
//    pitch = clampPitch(pitch);

//    if(Pitch != pitch) {
//        Pitch = pitch;
//        emit pitchChanged();

//        updateRotationMatrix();
//    }
//}
