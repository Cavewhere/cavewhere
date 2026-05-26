/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBaseTurnTableInteraction.h"
#include "cwCamera.h"
#include "cwDebug.h"
#include "cwInteractionLog.h"
#include "cwScene.h"
#include "cwGeometryItersecter.h"
#include "cwMatrix4x4Animation.h"
#include "cwMath.h"

//Std includes
#include "math.h"

//Qt includes
#include <QStyleHints>
#include <QGuiApplication>

Q_LOGGING_CATEGORY(lcInteract, "cw.interaction", QtWarningMsg)

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
    Q_ASSERT(Camera);
    Q_ASSERT(scene());
    Q_ASSERT(scene()->geometryItersecter());

    //Create a ray from the back projection front and back plane
    auto ray = Camera->frustrumRay(point);

    //See if it hits any of the scraps or objects
    double t = scene()->geometryItersecter()->intersects(ray);
    const bool geometryHit = !std::isnan(t);

    if(std::isnan(t)) {
        //See where it intersects ground plane geometry
        t = m_gridPlane.intersection(ray);
        if(std::isnan(t)) {
            qCDebug(lcInteract).nospace()
                << "unProject(" << point << "): no geometry hit, no grid hit -> (0,0,0)"
                << " rayOrigin=" << ray.origin()
                << " rayDir=" << ray.direction();
            return QVector3D();
        }
    }

    const QVector3D world = ray.point(t);
    qCDebug(lcInteract).nospace()
        << "unProject(" << point << "): "
        << (geometryHit ? "geometry" : "gridPlane")
        << " t=" << t
        << " world=" << world
        << " rayOrigin=" << ray.origin();
    return world;
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

void cwBaseTurnTableInteraction::bindPerspectiveIntersection()
{
    if(Camera && Scene) {
        m_perspectiveIntersection.setBinding([this](){
            return unProject(m_perspectiveMappedPos);
        });
    } else {
        m_perspectiveIntersection.setValue(QVector3D());
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
    if(!m_centerLocked) {
        setCenter(unProject(mappedPosition));
    }

    //Get the ray from the front of the screen to the back of the screen
    //Using the center of the screen
    QMatrix4x4 eyeMatrix = camera()->viewProjectionMatrix().inverted();


    QVector3D front = eyeMatrix.map(QVector3D(0.0, 0.0, -1.0));
    QVector3D back = eyeMatrix.map(QVector3D(0.0, 0.0, 1.0));

    QVector3D direction = QVector3D(front - back).normalized();

    //Create the plane through the pan pivot. When centerLocked is on the
    //pivot stays put — m_center is the authoritative pan/orbit pivot.
    PanPlane = QPlane3D(m_center, direction);

    qCDebug(lcInteract).nospace()
        << "startPanning raw=" << position
        << " mapped=" << mappedPosition
        << " pivot=" << m_center
        << " planeNormal=" << direction;

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
    const QPoint rawPosition = position;
    position = Camera->mapToGLViewport(position);
    if(!m_centerLocked) {
        setCenter(unProject(position));
    }
    LastMousePosition = position;
    qCDebug(lcInteract).nospace()
        << "startRotating raw=" << rawPosition
        << " mapped=" << position
        << " pivot=" << m_center;
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
void cwBaseTurnTableInteraction::zoom(QPoint position, double delta) {
    ZoomPosition = position;
    ZoomDelta = delta;
    qCDebug(lcInteract).nospace()
        << "zoom() pos=" << position << " delta=" << delta;
    zoomLastPosition();
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

    bindPerspectiveIntersection();
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

    qCDebug(lcInteract).nospace()
        << "rotateLastPosition mapped=" << position
        << " delta=" << delta
        << " pitch=" << Pitch
        << " azimuth=" << Azimuth
        << " pivot=" << m_center;

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
    QVector3D translateAmount = intersection - m_center;

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
    viewMatrix.translate(m_center);
    viewMatrix.rotate(rotationDifferance);
    viewMatrix.translate(-m_center);

    Camera->setViewMatrix(viewMatrix);
}

/**
 * @brief cwBaseTurnTableInteraction::clampAzimuth
 * @param azimuth
 * @return This clamps the azimuth between 0.0 and 360.0
 */
double cwBaseTurnTableInteraction::clampAzimuth(double azimuth) const
{
    return cwWrapDegrees360(azimuth);
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

    double delta = ZoomDelta;
    QPoint position = ZoomPosition;

    //Make the event position into gl viewport
    QPoint mappedPos = Camera->mapToGLViewport(position);
    m_perspectiveMappedPos = mappedPos; //Update the mapped position

    //Get the ray from the front of the screen to the back of the screen
    QVector3D front = Camera->unProject(mappedPos, 0.0);

    //Find the intsection on the plane, this is only updated if the
    //mouse position changed because of the property bindings
    QVector3D intersection = m_perspectiveIntersection.value(); //unProject(mappedPos);

    //Smallray
    QVector3D ray = intersection - front;
    float rayLength = ray.length();
    ray.normalize();

    // double t =  delta;
    double t = qMax(-1.0, qMin(1.0, delta));
    t = rayLength * t;

    QVector3D newPositionDelta = ray * t;

    qCDebug(lcInteract).nospace()
        << "zoomPerspective mapped=" << mappedPos
        << " front=" << front
        << " intersection=" << intersection
        << " rayLength=" << rayLength
        << " delta=" << delta
        << " translate=" << newPositionDelta;

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

    double delta = ZoomDelta;
    QPoint position = ZoomPosition;

    //Make the event position into gl viewport
    QPoint mappedPos = Camera->mapToGLViewport(position);

    double direction = 1.0 + delta;
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
 * @brief cwBaseTurnTableInteraction::setAzimuthLocked
 *
 * When locked, both setAzimuth() and rotation drags ignore azimuth changes.
 */
void cwBaseTurnTableInteraction::setAzimuthLocked(bool locked) {
    if(m_azimuthLocked == locked) { return; }
    m_azimuthLocked = locked;
    emit azimuthLockedChanged();
}

/**
 * @brief cwBaseTurnTableInteraction::setPitchLocked
 *
 * When locked, both setPitch() and rotation drags ignore pitch changes.
 */
void cwBaseTurnTableInteraction::setPitchLocked(bool locked) {
    if(m_pitchLocked == locked) { return; }
    m_pitchLocked = locked;
    emit pitchLockedChanged();
}

/**
 * @brief cwBaseTurnTableInteraction::setGridPlane
 *
 * The grid plane is the fallback intersection target used by unProject()
 * when the click ray doesn't hit any scene geometry.
 */
void cwBaseTurnTableInteraction::setGridPlane(const QPlane3D& gridPlane) {
    // QPlane3D::operator== is non-const in QMath3d (cannot be fixed here —
    // it's a submodule), but m_gridPlane is non-const so this still compiles.
    if(m_gridPlane == gridPlane) { return; }
    m_gridPlane = gridPlane;
    emit gridPlaneChanged();
}

/**
 * @brief cwBaseTurnTableInteraction::setCenter
 *
 * Sets the orbit/look-at center in world coordinates. updateRotationMatrix()
 * uses this point as the pivot for rotation drags and for setAzimuth /
 * setPitch.
 */
void cwBaseTurnTableInteraction::setCenter(QVector3D center) {
    if(m_center == center) { return; }
    m_center = center;
    emit centerChanged();
}

/**
 * @brief cwBaseTurnTableInteraction::setCenterLocked
 *
 * When true, startRotating() and startPanning() do not update m_center
 * from the click point. setCenter() still works programmatically.
 */
void cwBaseTurnTableInteraction::setCenterLocked(bool locked) {
    if(m_centerLocked == locked) { return; }
    m_centerLocked = locked;
    emit centerLockedChanged();
}

namespace {

// Minimum eye-to-center distance allowed in the 5-channel state. Anything
// smaller (or non-positive) puts the eye on the look-at point and produces
// a degenerate view matrix (no parallax; subsequent projection of m_center
// divides by w=0). The exact value is not load-bearing — it just has to be
// strictly positive and small enough that no realistic survey scene will
// hit it; 1e-3 world units (~1 mm in cave coordinates) is well below
// anything users can manipulate to.
constexpr double kMinViewDistance = 1.0e-3;

// Minimum ortho zoomScale allowed. Zero or negative values would degenerate
// the ortho frustum (zero-width view volume or inverted projection).
constexpr double kMinZoomScale = 1.0e-6;

// Build the canonical view matrix for a 5-channel turntable state. The recipe
// is: translate eye back by `distance` along view-space -Z, then apply the
// (pitch, azimuth) rotation, then translate world so `center` sits at the
// origin. This matches the orbit semantics used by updateRotationMatrix()
// (rotate around center) and the reset path (eye-to-center distance along
// view-space -Z).
QMatrix4x4 buildViewMatrix(const cwTurnTableViewState& s) {
    QMatrix4x4 m;
    m.translate(QVector3D(0.0f, 0.0f, -static_cast<float>(s.distance)));
    m.rotate(QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, static_cast<float>(s.pitch)));
    m.rotate(QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, static_cast<float>(s.azimuth)));
    m.translate(-s.center);
    return m;
}

} // namespace

/**
 * @brief cwBaseTurnTableInteraction::viewState
 *
 * Returns the current 5-channel view state. The distance channel is derived
 * from the current view matrix as the view-space depth of m_center: that's
 * the natural eye-to-center distance the canonical recipe is built from.
 *
 * The returned distance is clamped to kMinViewDistance so that a
 * viewState() -> setViewState() round-trip never produces a degenerate
 * matrix even if the live camera was somehow set up with m_center in front
 * of the eye (pre-existing pan/zoom paths don't guarantee that invariant).
 */
cwTurnTableViewState cwBaseTurnTableInteraction::viewState() const {
    cwTurnTableViewState s;
    s.center = m_center;
    s.azimuth = Azimuth;
    s.pitch = Pitch;
    if(!Camera.isNull()) {
        // View-space position of the world-space center; canonical recipe
        // puts it at (0, 0, -distance), so distance = -z_view.
        const QVector3D centerView = Camera->viewMatrix().map(m_center);
        s.distance = std::max(static_cast<double>(-centerView.z()), kMinViewDistance);
        s.zoomScale = std::max(static_cast<double>(Camera->zoomScale()), kMinZoomScale);
    }
    return s;
}

/**
 * @brief cwBaseTurnTableInteraction::setViewState
 *
 * Authoritative writer: rebuilds the view matrix from the 5-channel state
 * using the canonical recipe, and updates the cached azimuth/pitch/center
 * so subsequent orbit drags pick up the new pivot.
 *
 * Lock policy: setViewState intentionally bypasses azimuthLocked /
 * pitchLocked. The locks gate interactive drag and the individual
 * setAzimuth/setPitch entry points; setViewState is the snapshot/restore
 * path (animations, bookmarks, preview enter/exit) where the whole 5-tuple
 * must apply atomically. Callers that need lock-respecting updates should
 * route through setAzimuth / setPitch / setCenter instead.
 *
 * Validation: azimuth and pitch are clamped via clampAzimuth/clampPitch.
 * distance and zoomScale are clamped to small positive minima to avoid
 * producing a degenerate view matrix or ortho frustum from caller-supplied
 * (or deserialized) bad data.
 */
void cwBaseTurnTableInteraction::setViewState(const cwTurnTableViewState& state) {
    cwTurnTableViewState clean = state;
    clean.azimuth = clampAzimuth(state.azimuth);
    clean.pitch = clampPitch(state.pitch);
    clean.distance = std::max(state.distance, kMinViewDistance);
    clean.zoomScale = std::max(state.zoomScale, kMinZoomScale);

    // Exact != to match setAzimuth/setPitch's contract — emit on every
    // observable change, including arbitrarily small ones. qFuzzyCompare
    // is unreliable when either operand is zero (its relative epsilon
    // collapses there).
    const float newAzimuth = static_cast<float>(clean.azimuth);
    const float newPitch = static_cast<float>(clean.pitch);
    const bool azimuthDelta = Azimuth != newAzimuth;
    const bool pitchDelta = Pitch != newPitch;
    const bool centerDelta = m_center != clean.center;

    Azimuth = newAzimuth;
    Pitch = newPitch;
    m_center = clean.center;

    // Keep CurrentRotation aligned with the new (pitch, azimuth) so the next
    // updateRotationMatrix() computes the right rotationDifferance.
    QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, newPitch);
    QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, newAzimuth);
    setCurrentRotation(pitchQuat * azimuthQuat);

    if(!Camera.isNull()) {
        Camera->setViewMatrix(buildViewMatrix(clean));
        // Apply zoomScale unconditionally. cwCamera retains the value even
        // when the active projection is perspective; this keeps the round
        // trip viewState() <-> setViewState() faithful across projection
        // swaps.
        Camera->setZoomScale(clean.zoomScale);
    }

    if(azimuthDelta) { emit azimuthChanged(); }
    if(pitchDelta) { emit pitchChanged(); }
    if(centerDelta) { emit centerChanged(); }
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

    bindPerspectiveIntersection();
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

cwRayHit cwBaseTurnTableInteraction::pick(QPointF qtViewPoint) const
{
    Q_ASSERT(Camera);
    Q_ASSERT(scene());
    Q_ASSERT(scene()->geometryItersecter());

    QPoint mappedPos = Camera->mapToGLViewport(qtViewPoint.toPoint());

    //Create a ray from the back projection front and back plane
    const auto ray = Camera->frustrumRay(mappedPos);
    return scene()->geometryItersecter()->intersectsDetailed(ray);
}

void cwBaseTurnTableInteraction::zoomTo(const QBox3D &box)
{
    if(box.isNull()) {
        return;
    }

    // 1) Reset orientation/eye to your default
    resetView();

    // 2) Re-center the target box at the origin so zoom occurs “from the origin”
    const QVector3D c = box.center();
    QMatrix4x4 view = Camera->viewMatrix();
    view.translate(-c);                    // shift world so that box center is at origin
    Camera->setViewMatrix(view);

    // Padding so the box isn’t touching the frame
    constexpr float pad = 1.08f;          // ~8% margin

    switch(Camera->projection().type()) {
    case cwProjection::Ortho: {
        // -------- ORTHOGRAPHIC FIT --------
        // We compute the current world-span visible in the viewport at the box’s depth by
        // unprojecting the viewport corners, then scale zoom so min(viewSpan/boxSpan) fits.

        // Current viewport size (replace with your accessors)
        const QSize viewportSize = Camera->viewport().size();

        // Unproject top-left and bottom-right at w=1.0 (your code uses 1.0 for “far plane” in ortho path)
        const QVector3D topLeft = Camera->unProject(QPoint(0, 0),   1.0);
        const QVector3D bottomRight = Camera->unProject(QPoint(viewportSize.width(), viewportSize.height()), 1.0);

        const float viewSpanX = std::abs(bottomRight.x() - topLeft.x());
        const float viewSpanY = std::abs(bottomRight.y() - topLeft.y());

        const QVector3D half = 0.5f * (box.maximum() - box.minimum());
        const float boxSizeX = 2.0f * std::max(half.x(), 1e-6f);
        const float boxSizeY = 2.0f * std::max(half.y(), 1e-6f);

        // How much we can “grow” the box before it hits an edge
        const float fitX = boxSizeX / viewSpanX;
        const float fitY = boxSizeY / viewSpanY;
        const float fit  = std::max(fitX, fitY) * pad;

        // In your ortho zoom, zoomScale scales linearly; keep the box centered at origin.
        Camera->setZoomScale(Camera->zoomScale() * fit);

        // No extra translation: we already centered the box at the origin.
        return;
    }
    case cwProjection::Perspective:
    case cwProjection::PerspectiveFrustum: {
        qDebug() << "Warning this hasn't been tested" << LOCATION;

        // -------- PERSPECTIVE FIT --------
        // We compute the distance from the eye to the origin so that the box fits in both X and Y
        // using the FOV and aspect. Then we move the camera along its forward vector.

        // Replace with your getters if names differ.
        const float fovY = Camera->projection().fieldOfView() * cwGlobals::degreesToRadians();
        const float aspect = Camera->projection().aspectRatio();

        // Convert to FOVx for width fit
        const float tanHalfFovY = std::tan(0.5f * fovY);
        const float tanHalfFovX = tanHalfFovY * aspect;

        // Half extents in world after recentring (axis-aligned in your world frame)
        const QVector3D half = 0.5f * (box.maximum() - box.minimum());
        const float hx = std::max(half.x(), 1e-6f);
        const float hy = std::max(half.y(), 1e-6f);
        const float hz = std::max(half.z(), 0.0f);

        // Minimum distances so that full width/height fits
        const float dY = hy / tanHalfFovY;
        const float dX = hx / std::max(tanHalfFovX, 1e-6f);

        // Add half-depth so the near face doesn’t clip when the box has thickness toward the camera
        float dist = std::max(dX, dY) + hz;
        dist *= pad; // give it some breathing room

        // Move the camera *back* along its forward axis by 'dist'.
        // We derive the forward direction from the current view.
        // In view space, forward is -Z. Pull that into world with the rotation part of inv(view).
        bool canInvert;
        QMatrix4x4 invView = Camera->viewMatrix().inverted(&canInvert);
        if (canInvert) { /* extremely unlikely; just bail gracefully */ return; }

        const QVector3D forwardWorld = (invView * QVector4D(0.f, 0.f, -1.0f, 0.f)).toVector3D().normalized();
        QMatrix4x4 newView = Camera->viewMatrix();
        newView.translate(-forwardWorld * dist);
        Camera->setViewMatrix(newView);
        return;
    }
    default:
        break;
    }
}

