/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBaseTurnTableInteraction.h"
#include "cwCamera.h"
#include "cwScene.h"
#include "cwGeometryItersecter.h"
#include "cwMatrix4x4Animation.h"

//Std includes
#include "math.h"

//Qt includes
#include <QStyleHints>
#include <QGuiApplication>

cwBaseTurnTableInteraction::cwBaseTurnTableInteraction(QQuickItem *parent) :
    cwInteraction(parent),
    ViewMatrixAnimation(new cwMatrix4x4Animation(this))
{
    ZoomLevel = 1.0; //1 pixel = 1 meter

    ViewMatrixAnimation->setDuration(1000);
    ViewMatrixAnimation->setEasingCurve(QEasingCurve(QEasingCurve::InOutCubic));
    connect(ViewMatrixAnimation, &cwMatrix4x4Animation::valueChanged,
            this, &cwBaseTurnTableInteraction::updateViewMatrixFromAnimation);

    resetView();

    setupInteractionTimers();
}

/**
 * @brief cwBaseTurnTableInteraction::setGridPlane
 * @param plan
 *
 * This is for interaction. If the mouse can intersect with any
 * other geometry, this will intesect with this plane so rotation work correctly.
 */


/**
 * @brief cwBaseTurnTableInteraction::centerOn
 * @param point - Should be in world coordinates
 *
 * This will center the camera on point. The point will be in the center of the screen.
 */
void cwBaseTurnTableInteraction::centerOn(QVector3D point, bool animate)
{
    QVector3D projectPoint = (Camera->projectionMatrix() * Camera->viewMatrix()).map(point);
    double depth = projectPoint.z();

    QPoint screenCenter = Camera->viewport().center();
    QVector3D worldScreenCenter = Camera->unProject(screenCenter, (depth + 1.0) / 2.0);

    QVector3D translation = worldScreenCenter - point;

    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(translation);

    if(animate) {
        stopAnimation();

        ViewMatrixAnimation->setStartValue(Camera->viewMatrix());
        ViewMatrixAnimation->setEndValue(viewMatrix);
        ViewMatrixAnimation->start();
    } else {
        Camera->setViewMatrix(viewMatrix);
    }
}

/**
 * @brief cwBaseTurnTableInteraction::unProject
 * @param point
 * @return  QVector3d in world coordinates
 *
 * Unprojects the screen point at point and returns a QVector3d in world coordinates
 */
QVector3D cwBaseTurnTableInteraction::unProject(QPoint point) {

    //Create a ray from the back projection front and back plane
    QVector3D frontPoint = Camera->unProject(point, 0.0);
    QVector3D backPoint = Camera->unProject(point, 1.0);
    QVector3D direction = QVector3D(backPoint - frontPoint).normalized();
    QRay3D ray(frontPoint, direction);

    //See if it hits any of the scraps or objects
    double t = scene()->geometryItersecter()->intersects(ray);

    if(std::isnan(t)) {
        //See where it intersects ground plane geometry
        t = m_gridPlane.value().intersection(ray);
        if(std::isnan(t)) {
            return QVector3D();
        }
    }

    return ray.point(t);
}

/**
 * @brief cwBaseTurnTableInteraction::stopAnimation
 *
 * Stops h
 */
void cwBaseTurnTableInteraction::stopAnimation()
{
    if(ViewMatrixAnimation->state() == QAbstractAnimation::Running) {
        ViewMatrixAnimation->setCurrentTime(ViewMatrixAnimation->duration());
        ViewMatrixAnimation->stop();
    }
}

/**
 * @brief cwBaseTurnTableInteraction::updateViewMatrixFromAnimation
 *
 * This updates the view matrix of the camera
 */
void cwBaseTurnTableInteraction::updateViewMatrixFromAnimation(QVariant matrix)
{
    Camera->setViewMatrix(matrix.value<QMatrix4x4>());
}

/**
  Initializes the last click for the panning state
  */
void cwBaseTurnTableInteraction::startPanning(QPoint position) {
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
void cwBaseTurnTableInteraction::pan(QPoint position) {
    TranslatePosition = position;
    if(!TranslateTimer->isActive()) {
        TranslateTimer->start();
    }
}

/**
  Called when the rotation is about to begin
  */
void cwBaseTurnTableInteraction::startRotating(QPoint position) {
    position = Camera->mapToGLViewport(position);
    LastMouseGlobalPosition = unProject(position);
    LastMousePosition = position;
}

/**
  Rotates the view
  */
void cwBaseTurnTableInteraction::rotate(QPoint position) {
    TimeoutRotationPosition = position;
    if(!RotationInteractionTimer->isActive()) {
        RotationInteractionTimer->start();
    }
}

/**
  Zooms the view
  */
void cwBaseTurnTableInteraction::zoom(QPoint position, int delta) {
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
void cwBaseTurnTableInteraction::resetView() {
    Pitch = defaultPitch();
    Azimuth = defaultAzimuth();

    setCurrentRotation(defaultRotation());

    emit pitchChanged();
    emit azimuthChanged();

    if(Camera) {
        QMatrix4x4 viewMatrix;
        viewMatrix.translate(QVector3D(0.0, 0.0, -50));
        Camera->setViewMatrix(viewMatrix);

        if(Camera->projection().type() == cwProjection::Ortho) {
            Camera->setZoomScale(Camera->defaultZoomScale());
        }
    }
}


/**
 * @brief cwBaseTurnTableInteraction::rotateLastPosition
 *
 * This is called by the timer, after the timer has consumed mouse events.  This prevents
 * rendering thread to be starved
 */
void cwBaseTurnTableInteraction::rotateLastPosition()
{
    if(Camera.isNull()) { return; }

    QPoint position = TimeoutRotationPosition;

    position = Camera->mapToGLViewport(position);
    QPoint currentMousePosition = position;
    QPointF delta = LastMousePosition - currentMousePosition;
    LastMousePosition = currentMousePosition;
    delta /= 2.0;

    //Calculate the new pitch
    if(!isPitchLocked()) {
        Pitch = clampPitch(Pitch + delta.y());
        emit pitchChanged();
    }

    //Calculate the new azimuth
    if(!isAzimuthLocked()) {
        Azimuth = clampAzimuth(Azimuth - delta.x());
        emit azimuthChanged();
    }

    updateRotationMatrix();
}

/**
 * @brief cwBaseTurnTableInteraction::zoomLastPosition
 *
 * This is called by the timer, after the timer has consumed mouse events.  This prevents
 * rendering thread to be starved
 */
void cwBaseTurnTableInteraction::zoomLastPosition()
{
    if(Camera.isNull()) { return; }

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
 * @brief cwBaseTurnTableInteraction::translateLastPosition
 *
 * This is called by the timer, after the timer has consumed mouse events.  This prevents
 * rendering thread to be starved
 */
void cwBaseTurnTableInteraction::translateLastPosition()
{
    if(Camera.isNull()) { return; }

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
}

///**
//  \brief Sets the caving region for the renderer
//  */
//void cwBaseTurnTableInteraction::setCavingRegion(cwCavingRegion* region) {
//    if(Region != region) {
//        Region = region;
//        emit cavingRegionChanged();
//    }
//}

/**
 * @brief cwBaseTurnTableInteraction::setupInteractionTimers
 *
 * This sets up the interaction timers.  This prevents the operating system from firing more
 * mouse events than it needs to.
 *
 * On linux, mouse events are fired very, very quickly, causing the rendering thread to strave
 *
 * The timers prevent the starvation
 */
void cwBaseTurnTableInteraction::setupInteractionTimers()
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

    connect(RotationInteractionTimer, &QTimer::timeout, this, &cwBaseTurnTableInteraction::rotateLastPosition);
    connect(ZoomInteractionTimer, &QTimer::timeout, this, &cwBaseTurnTableInteraction::zoomLastPosition);
    connect(TranslateTimer, &QTimer::timeout, this, &cwBaseTurnTableInteraction::translateLastPosition);
}

void cwBaseTurnTableInteraction::setCurrentRotation(QQuaternion rotation)
{
    CurrentRotation = rotation;
    emit cameraRotationChanged();
}

QQuaternion cwBaseTurnTableInteraction::defaultRotation() const
{
    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, defaultPitch());
    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, defaultAzimuth());
    return pitchQuat * azimuthQuat;
}

void cwBaseTurnTableInteraction::updateRotationMatrix()
{
    if(Camera.isNull()) { return; }

    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0, 0.0, 0.0, Pitch);
    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0, 0.0, 1.0, Azimuth);
    QQuaternion newQuat = pitchQuat * azimuthQuat;
    QQuaternion rotationDifferance = CurrentRotation.conjugated() * newQuat;
    setCurrentRotation(newQuat);

    QMatrix4x4 viewMatrix = Camera->viewMatrix();
    viewMatrix.translate(LastMouseGlobalPosition);
    viewMatrix.rotate(rotationDifferance);
    viewMatrix.translate(-LastMouseGlobalPosition);

    Camera->setViewMatrix(viewMatrix);
}

/**
 * @brief cwBaseTurnTableInteraction::clampAzimuth
 * @param azimuth
 * @return This clamps the azimuth between 0.0 and 360.0
 */
double cwBaseTurnTableInteraction::clampAzimuth(double azimuth) const
{
    azimuth = fmod(azimuth, 360.0);
    if(azimuth < 0) {
        azimuth += 360.0;
    }
    return azimuth;
}

/**
 * @brief cwBaseTurnTableInteraction::clampPitch
 * @param pitch
 * @return This clamps the pitch from -90 to 90
 */
double cwBaseTurnTableInteraction::clampPitch(double pitch) const
{
    return qMin(90.0, qMax(-90.0, pitch));
}

/**
 * @brief cwBaseTurnTableInteraction::zoomPerspective
 *
 * Zoom with a perspective transfromation
 */
void cwBaseTurnTableInteraction::zoomPerspective()
{
    if(Camera.isNull()) { return; }

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
 * @brief cwBaseTurnTableInteraction::zoomOrtho
 *
 * Zoom with an orthoganal projection
 */
void cwBaseTurnTableInteraction::zoomOrtho()
{
    if(Camera.isNull()) { return; }

    int delta = ZoomDelta;
    QPoint position = ZoomPosition;

    //Make the event position into gl viewport
    QPoint mappedPos = Camera->mapToGLViewport(position);

    double direction = delta > 0 ? 1.075 : 0.925;
    double zoomLevel = camera()->zoomScale() * direction;

    QVector3D before = Camera->unProject(mappedPos, 1.0);

    Camera->setZoomScale(zoomLevel);

    QVector3D after = Camera->unProject(mappedPos, 1.0);

    QVector3D changeInPosition = after - before;
    QMatrix4x4 newTranslationMatrix = Camera->viewMatrix();
    newTranslationMatrix.translate(changeInPosition);
    Camera->setViewMatrix(newTranslationMatrix);
}

/**
Gets rotation current global rotation
*/
QQuaternion cwBaseTurnTableInteraction::cameraRotation() const {
    return defaultRotation().conjugated() * CurrentRotation;
}

/**
* @brief cwBaseTurnTableInteraction::setAzimuth
* @param azimuth
*/
void cwBaseTurnTableInteraction::setAzimuth(double azimuth) {
    azimuth = clampAzimuth(azimuth);
    if(Azimuth != azimuth && !isAzimuthLocked()) {
        Azimuth = azimuth;
        emit azimuthChanged();

        updateRotationMatrix();
    }
}


/**
* @brief cwBaseTurnTableInteraction::setPitch
* @param pitch
*/
void cwBaseTurnTableInteraction::setPitch(double pitch) {
    pitch = clampPitch(pitch);

    if(Pitch != pitch && !isPitchLocked()) {
        Pitch = pitch;
        emit pitchChanged();

        updateRotationMatrix();
    }
}

/**
* @brief cwBaseTurnTableInteraction::setCamera
* @param camera
*
* This interaction maniputates this camera
*/
void cwBaseTurnTableInteraction::setCamera(cwCamera* camera) {
    if(Camera != camera) {
        Camera = camera;
        resetView();
        emit cameraChanged();
    }
}

/**
* @brief cwBaseTurnTableInteraction::setScene
* @param scene
*/
void cwBaseTurnTableInteraction::setScene(cwScene* scene) {
    if(Scene != scene) {
        Scene = scene;
        emit sceneChanged();
    }
}

/**
* @brief cwBaseTurnTableInteraction::camera
* @return
*/
cwCamera* cwBaseTurnTableInteraction::camera() const {
    return Camera;
}

/**
* @brief cwBaseTurnTableInteraction::scene
* @return
*/
cwScene* cwBaseTurnTableInteraction::scene() const {
    return Scene;
}

/**
* @brief cwBaseTurnTableInteraction::startDragDistance
* @return QStyleHints::startDragDistance
*/
int cwBaseTurnTableInteraction::startDragDistance() const {
    return QGuiApplication::styleHints()->startDragDistance();
}

