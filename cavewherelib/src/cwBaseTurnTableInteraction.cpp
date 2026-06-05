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
#include "cwMath.h"

//Std includes
#include "math.h"

//Qt includes
#include <QStyleHints>
#include <QGuiApplication>

Q_LOGGING_CATEGORY(lcInteract, "cw.interaction", QtWarningMsg)

cwBaseTurnTableInteraction::cwBaseTurnTableInteraction(QQuickItem *parent) :
    cwInteraction(parent),
    m_stateAnimation(new QVariantAnimation(this))
{
    ZoomLevel = 1.0; //1 pixel = 1 meter

    m_stateAnimation->setObjectName(QLatin1String(stateAnimationObjectName));
    m_stateAnimation->setDuration(DefaultStateAnimationDurationMs);
    m_stateAnimation->setEasingCurve(QEasingCurve(QEasingCurve::InOutCubic));
    m_stateAnimation->setStartValue(0.0);
    m_stateAnimation->setEndValue(1.0);
    connect(m_stateAnimation, &QVariantAnimation::valueChanged,
            this, &cwBaseTurnTableInteraction::onStateAnimTick);
    // Queue the public animationFinished re-emit so that listener slots
    // (e.g. QML chains that start a follow-on animateToViewState in
    // response) run AFTER the current stop/finish bookkeeping has fully
    // unwound. Direct connection here would let a listener re-enter
    // animateToViewState mid-stop, corrupting m_stateAnimStart /
    // m_stateAnimTarget (see D-002 in the commit-3 review).
    connect(m_stateAnimation, &QVariantAnimation::finished, this, [this]() {
        QMetaObject::invokeMethod(this, &cwBaseTurnTableInteraction::animationFinished,
                                  Qt::QueuedConnection);
    });

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
 * Centers the camera on point so it lands at the middle of the screen.
 * Orientation (pitch, azimuth) and zoom (distance, zoomScale) are preserved
 * via the 5-channel viewState — only the center channel moves. When animate
 * is true the move is dispatched through animateToViewState() so it can
 * compose with the rest of the state-based animation framework.
 */
void cwBaseTurnTableInteraction::centerOn(QVector3D point, bool animate)
{
    if(Camera.isNull()) { return; }

    cwTurnTableViewState target = viewState();
    target.center = point;
    // distance, azimuth, pitch, zoomScale preserved -> the rebuilt view
    // matrix puts `point` at view-space (0, 0, -distance), which is the
    // screen center for both ortho and perspective.

    if(animate) {
        animateToViewState(target);
    } else {
        setViewState(target);
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
 * Snaps the 5-channel state animator to its end value before stopping, so
 * the camera ends up in a consistent committed state.
 */
void cwBaseTurnTableInteraction::stopAnimation()
{
    if(m_stateAnimation->state() == QAbstractAnimation::Running) {
        m_stateAnimation->setCurrentTime(m_stateAnimation->duration());
        m_stateAnimation->stop();
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
  Initializes the last click for the panning state
  */
void cwBaseTurnTableInteraction::startPanning(QPoint position) {
    QPoint mappedPosition = Camera->mapToGLViewport(position);
    const QVector3D clickWorld = unProject(mappedPosition);
    if(!m_centerLocked) {
        setCenter(clickWorld);
    }

    // The pan anchor is always the click point — independent of the
    // orbit-pivot lock. m_center stays where it is (locked: sink centroid;
    // unlocked: just got set to clickWorld too), but the pan drag tracks
    // the click position under the cursor so there's no initial jump.
    m_panAnchor = clickWorld;

    //Get the ray from the front of the screen to the back of the screen
    //Using the center of the screen
    QMatrix4x4 eyeMatrix = camera()->viewProjectionMatrix().inverted();


    QVector3D front = eyeMatrix.map(QVector3D(0.0, 0.0, -1.0));
    QVector3D back = eyeMatrix.map(QVector3D(0.0, 0.0, 1.0));

    QVector3D direction = QVector3D(front - back).normalized();

    //Create the plane through the pan anchor (the click point).
    PanPlane = QPlane3D(m_panAnchor, direction);

    qCDebug(lcInteract).nospace()
        << "startPanning raw=" << position
        << " mapped=" << mappedPosition
        << " pivot=" << m_center
        << " panAnchor=" << m_panAnchor
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
    QVector3D translateAmount = intersection - m_panAnchor;

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

// Floor used by zoomTo's fit math when dividing by half-extents or
// FOV-tangent values. Anything below this collapses to 1e-6 to avoid
// divide-by-zero / NaN propagation into the view matrix. Same magnitude
// as kMinZoomScale by coincidence; kept distinct because the meanings are
// different (geometry epsilon vs ortho-zoom epsilon).
constexpr float kFramingEpsilon = 1.0e-6f;

// True iff all three components of v are finite (not NaN, not +/-inf).
inline bool isFinite3(const QVector3D& v) {
    return std::isfinite(v.x()) && std::isfinite(v.y()) && std::isfinite(v.z());
}

// Minimum animation duration in ms accepted by animateToViewState. Zero
// would make QVariantAnimation skip straight to end (no interpolation);
// negative values are documented as invalid. 1 ms is "effectively instant
// but still ticks once," which preserves the signal sequence.
constexpr int kMinAnimationDurationMs = 1;

// One full turn around the azimuth circle.
constexpr double kFullCircleDegrees = 360.0;
constexpr double kHalfCircleDegrees = 180.0;

// Defaults used when a 5-channel state channel comes in as NaN/inf.
constexpr double kDefaultPitchDegrees = 90.0;     // matches defaultPitch()

// Pitch bounds. Mirror cwBaseTurnTableInteraction::clampPitch but exposed
// here so the free-function clampViewState helper can do its own clamp
// without requiring a member call.
constexpr double kMaxPitchDegrees = 90.0;
constexpr double kMinPitchDegrees = -90.0;

// Replace NaN/inf scalar components with zero. std::isfinite returns false
// for NaN and +/-inf; this is the cheapest finite-or-zero guard.
inline float finiteOrZero(float v) {
    return std::isfinite(v) ? v : 0.0f;
}

// Sanitize a 5-channel view state on intake: clamp ranges and replace
// non-finite components with safe defaults. std::max(NaN, x) returns NaN
// on libstdc++/libc++ (NaN comparisons are false), so a plain max is not
// enough to defend against poisoned input — explicit isfinite is required.
// Single source of truth used by viewState(), setViewState(), and
// animateToViewState() so the clamping rules can't drift between writers.
cwTurnTableViewState clampViewState(const cwTurnTableViewState& state) {
    cwTurnTableViewState clean;

    clean.center = QVector3D(finiteOrZero(state.center.x()),
                             finiteOrZero(state.center.y()),
                             finiteOrZero(state.center.z()));

    // cwWrapDegrees360 already returns 0.0 on NaN/inf input; no extra guard
    // needed here.
    clean.azimuth = cwWrapDegrees360(state.azimuth);

    clean.pitch = std::isfinite(state.pitch)
            ? (std::min)(kMaxPitchDegrees, (std::max)(kMinPitchDegrees, state.pitch))
            : kDefaultPitchDegrees;

    clean.distance = std::isfinite(state.distance)
            ? (std::max)(state.distance, kMinViewDistance)
            : kMinViewDistance;

    clean.zoomScale = std::isfinite(state.zoomScale)
            ? (std::max)(state.zoomScale, kMinZoomScale)
            : kMinZoomScale;

    // eyeOffset is an eye-space XY shift; Z is reserved (always zero).
    // No clamp on XY range — any finite pan is legal — just scrub
    // non-finite components so a poisoned channel can't propagate into
    // the view matrix. Z is forced to zero to keep the invariant
    // symmetric with viewState()'s extractor.
    clean.eyeOffset = QVector3D(finiteOrZero(state.eyeOffset.x()),
                                finiteOrZero(state.eyeOffset.y()),
                                0.0f);

    return clean;
}

// Scalar linear interpolation.
inline double lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

// Component-wise linear interpolation for QVector3D.
inline QVector3D lerp(const QVector3D& a, const QVector3D& b, double t) {
    return a + (b - a) * static_cast<float>(t);
}

// Short-arc azimuth interpolation on a 0..360 circle. Going from 350° to
// 10° at t=0.5 must produce 0° (a 20° forward sweep), not 180° (a 340°
// backward sweep). The naive lerp would do the latter; this routes the
// delta through the short way.
inline double lerpAzimuth(double a, double b, double t) {
    double delta = b - a;
    if(delta > kHalfCircleDegrees) {
        delta -= kFullCircleDegrees;
    } else if(delta < -kHalfCircleDegrees) {
        delta += kFullCircleDegrees;
    }
    return cwWrapDegrees360(a + delta * t);
}

// View-matrix rotation that matches cwBaseTurnTableInteraction's existing
// (Pitch, Azimuth) convention.
//
// resetView() leaves the view matrix as a plain T(0,0,-50) — i.e. no
// rotation — and caches CurrentRotation = R_x(90)*R_z(0) as "default."
// updateRotationMatrix() then applies INCREMENTAL deltas
// CurrentRotation^-1 * newQuat to the live view matrix. So the cached
// Pitch=90 / Azimuth=0 state corresponds to identity on the view-matrix
// rotation, NOT to a literal 90° X rotation.
//
// To reproduce that mapping in a canonical rebuild, the view-matrix
// rotation must be the delta from defaultRotation:
//   R_view = defaultRotation^-1 * (R_x(pitch) * R_z(azimuth))
// which evaluates to identity for the default and to the right tilt for
// any other (pitch, azimuth) pair. Without this, setViewState() /
// animateToViewState() / centerOn() rebuild the view matrix with a 90°
// X-rotation baked in, flipping the camera from plan view to profile and
// leaving CurrentRotation out of sync with the live matrix (the bug
// surfaced via gotoLead from the lead page in plan view).
inline QQuaternion viewMatrixRotation(double pitch, double azimuth) {
    const QQuaternion pitchQuat = QQuaternion::fromAxisAndAngle(
                1.0f, 0.0f, 0.0f, static_cast<float>(pitch));
    const QQuaternion azimuthQuat = QQuaternion::fromAxisAndAngle(
                0.0f, 0.0f, 1.0f, static_cast<float>(azimuth));
    const QQuaternion defaultRot =
            QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f,
                                          static_cast<float>(kDefaultPitchDegrees))
            * QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, 0.0f);
    return defaultRot.conjugated() * (pitchQuat * azimuthQuat);
}

// Build the canonical view matrix for a 5-channel turntable state. The
// recipe is: translate the eye back by `distance` along view-space -Z,
// apply the (pitch, azimuth) view-matrix rotation per viewMatrixRotation(),
// then translate world so `center` sits at the origin. This matches the
// orbit semantics used by updateRotationMatrix() and the reset path.
QMatrix4x4 buildViewMatrix(const cwTurnTableViewState& s) {
    QMatrix4x4 m;
    // eye-space pan offset is applied LAST in eye coords. Qt's
    // QMatrix4x4::translate(v) does M = M * T(v), so the first call here
    // becomes the outermost (leftmost) transform — i.e. T(eyeOffset) is
    // applied to the eye-space output of the rest of the recipe. This
    // preserves orbit-around-m_center while letting the pan show on screen.
    m.translate(s.eyeOffset);
    m.translate(QVector3D(0.0f, 0.0f, -static_cast<float>(s.distance)));
    m.rotate(viewMatrixRotation(s.pitch, s.azimuth));
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
 * The returned state is run through clampViewState() so that a
 * viewState() -> setViewState() round-trip never produces a degenerate
 * matrix even if the live camera was somehow set up with m_center in front
 * of the eye, or if any channel is non-finite.
 */
cwTurnTableViewState cwBaseTurnTableInteraction::viewState() const {
    cwTurnTableViewState s;
    s.center = m_center;
    s.azimuth = Azimuth;
    s.pitch = Pitch;
    if(!Camera.isNull()) {
        // View-space position of the world-space center under the CURRENT
        // view matrix. Without user pan this lands at (0, 0, -distance) —
        // the canonical recipe target. With pan, the XY components capture
        // the eye-space shift; that's what eyeOffset stores so the next
        // setViewState() / animator tick can reproduce the panned view
        // faithfully instead of snapping back to centred.
        const QVector3D centerView = Camera->viewMatrix().map(m_center);
        s.distance = -centerView.z();
        // Z component is identically zero by construction
        // (distance = -centerView.z()), so we only carry the eye-space XY
        // shift that the pan introduced.
        s.eyeOffset = QVector3D(centerView.x(), centerView.y(), 0.0f);
        s.zoomScale = Camera->zoomScale();
    }
    return clampViewState(s);
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
 * Validation: all five channels run through clampViewState (range clamps,
 * NaN/inf rejection). std::max alone is not enough — std::max(NaN, x)
 * returns NaN on libstdc++/libc++, so explicit isfinite checks live in
 * clampViewState.
 */
void cwBaseTurnTableInteraction::setViewState(const cwTurnTableViewState& state) {
    cwTurnTableViewState clean = clampViewState(state);

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
 * @brief cwBaseTurnTableInteraction::animateToViewState
 *
 * Animates the camera from the current 5-channel state to `target` over
 * `durationMs`. Each tick interpolates per-channel and applies the result
 * via setViewState() — the same authoritative writer used for snap
 * restores, so the animation end-state is bit-for-bit equal to a snap
 * call with the same target.
 *
 * Channel rules:
 *   - center, pitch, zoomScale: straight linear lerp.
 *   - azimuth: short-arc lerp on a 0..360 circle (350° → 10° = +20°).
 *   - distance: perspective only. Under ortho the projection is
 *     decoupled from eye distance, so the start value is held until
 *     the final tick where it snaps to target. (Lerping distance under
 *     ortho would change the depth buffer but not the visible scene,
 *     and risks confusing follow-on snapshots.)
 *
 * Calling animateToViewState while another animation is running stops
 * the prior one cleanly (snaps to its end value).
 */
void cwBaseTurnTableInteraction::animateToViewState(
        const cwTurnTableViewState& target, int durationMs)
{
    if(Camera.isNull()) { return; }

    stopAnimation();

    m_stateAnimStart = viewState();

    // Clamp target on intake (single source of truth shared with
    // setViewState) so a re-entrant setViewState mid-tick can't see an
    // un-clamped channel.
    m_stateAnimTarget = clampViewState(target);

    // Clamp duration: 0 makes QVariantAnimation skip to end with no
    // valueChanged ticks; negative is documented invalid.
    m_stateAnimation->setDuration((std::max)(durationMs, kMinAnimationDurationMs));
    m_stateAnimation->start();
}

/**
 * @brief cwBaseTurnTableInteraction::onStateAnimTick
 *
 * QVariantAnimation::valueChanged handler. Builds the interpolated state
 * from the captured start/target snapshots and pushes it through
 * setViewState() so every tick goes through the same canonical recipe.
 */
void cwBaseTurnTableInteraction::onStateAnimTick(const QVariant& value)
{
    if(Camera.isNull()) { return; }

    const double t = value.toReal();
    const bool isPerspective =
            Camera->projection().type() == cwProjection::Perspective
            || Camera->projection().type() == cwProjection::PerspectiveFrustum;

    cwTurnTableViewState s;
    s.center = lerp(m_stateAnimStart.center, m_stateAnimTarget.center, t);
    s.azimuth = lerpAzimuth(m_stateAnimStart.azimuth, m_stateAnimTarget.azimuth, t);
    s.pitch = lerp(m_stateAnimStart.pitch, m_stateAnimTarget.pitch, t);
    s.distance = isPerspective
                 ? lerp(m_stateAnimStart.distance, m_stateAnimTarget.distance, t)
                 : m_stateAnimTarget.distance;
    s.zoomScale = lerp(m_stateAnimStart.zoomScale, m_stateAnimTarget.zoomScale, t);
    s.eyeOffset = lerp(m_stateAnimStart.eyeOffset, m_stateAnimTarget.eyeOffset, t);

    setViewState(s);
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
    // box.isNull() only catches the default-constructed case; an explicit
    // finite check guards against NaN/inf min/max propagating into the
    // fit math and poisoning the view matrix.
    if (!isFinite3(box.minimum()) || !isFinite3(box.maximum())) {
        return;
    }

    // 1) Reset orientation/eye to your default
    resetView();

    const QVector3D c = box.center();

    switch(Camera->projection().type()) {
    case cwProjection::Ortho: {
        // -------- ORTHOGRAPHIC FIT --------
        // Re-center first: ortho's fit math reads the current world span
        // via unProject, and the final positioning needs box.center() at
        // view-space (0,0,-defaultDistance). The perspective branch builds
        // its view matrix from scratch and does not want this intermediate
        // write.
        QMatrix4x4 view = Camera->viewMatrix();
        view.translate(-c);
        Camera->setViewMatrix(view);

        // Compute the current world-span visible in the viewport at the
        // box's depth by unprojecting the viewport corners, then scale zoom
        // so the larger box dimension fills the corresponding viewport span.
        const QSize viewportSize = Camera->viewport().size();
        const QVector3D topLeft     = Camera->unProject(QPoint(0, 0), 1.0);
        const QVector3D bottomRight = Camera->unProject(
                    QPoint(viewportSize.width(), viewportSize.height()), 1.0);

        const float viewSpanX = std::abs(bottomRight.x() - topLeft.x());
        const float viewSpanY = std::abs(bottomRight.y() - topLeft.y());

        const QVector3D half = 0.5f * (box.maximum() - box.minimum());
        const float boxSizeX = 2.0f * (std::max)(half.x(), kFramingEpsilon);
        const float boxSizeY = 2.0f * (std::max)(half.y(), kFramingEpsilon);

        const float fitX = boxSizeX / (std::max)(viewSpanX, kFramingEpsilon);
        const float fitY = boxSizeY / (std::max)(viewSpanY, kFramingEpsilon);
        const float fit  = (std::max)(fitX, fitY) * FramingPad;
        if (!std::isfinite(fit) || fit <= 0.0f) {
            return;
        }

        Camera->setZoomScale(Camera->zoomScale() * fit);
        return;
    }
    case cwProjection::Perspective:
    case cwProjection::PerspectiveFrustum: {
        // -------- PERSPECTIVE FIT --------
        // Required eye-to-box-center distance so the box's half-width
        // subtends half of the appropriate FOV. Take the bigger of the X
        // and Y fits (whichever axis is wider). Add half-depth so the near
        // face of a box with Z-thickness doesn't clip. Multiply by pad.
        const float fovY = Camera->projection().fieldOfView() * cwGlobals::degreesToRadians();
        const float aspect = Camera->projection().aspectRatio();
        const float tanHalfFovY = std::tan(0.5f * fovY);
        const float tanHalfFovX = tanHalfFovY * aspect;

        const QVector3D half = 0.5f * (box.maximum() - box.minimum());
        const float hx = (std::max)(half.x(), kFramingEpsilon);
        const float hy = (std::max)(half.y(), kFramingEpsilon);
        const float hz = (std::max)(half.z(), 0.0f);

        // Symmetric guards on both FOV-tangent divisors so a degenerate
        // projection (fov == 0 → tan == 0) can't slip through.
        const float dY = hy / (std::max)(tanHalfFovY, kFramingEpsilon);
        const float dX = hx / (std::max)(tanHalfFovX, kFramingEpsilon);
        const float dist = ((std::max)(dX, dY) + hz) * FramingPad;
        if (!std::isfinite(dist) || dist <= 0.0f) {
            return;
        }

        // Build the view matrix from scratch: camera at view-space (0,0,0)
        // with box.center() at view-space (0,0,-dist), looking down -Z
        // (the orientation resetView left us in). Replaces the previous
        // translate-the-existing-matrix path that had a sign error AND an
        // inverted `canInvert` guard.
        QMatrix4x4 newView;
        newView.translate(0.0f, 0.0f, -dist);
        newView.translate(-c);
        Camera->setViewMatrix(newView);
        return;
    }
    default:
        break;
    }
}

/**
 * @brief cwBaseTurnTableInteraction::framingViewState
 *
 * Returns the 5-channel viewState that frames @a box at the supplied
 * @a azimuth / @a pitch (degrees). Unlike zoomTo() this is const, does
 * NOT call resetView(), and uses the supplied orientation for BOTH the
 * fit math AND the returned target — so the AABB is sized against the
 * post-rotation view, not the current one. Box-null, camera-null, or
 * non-finite box falls through to the current viewState() unchanged.
 * Caller routes the result through animateToViewState() / setViewState().
 *
 * Fit math runs in VIEW space, not world space. zoomTo() can use raw
 * box.x()/y() half-extents because it resetView()'s to identity-on-view
 * first, so the box's world axes align with the view axes. Here the
 * orientation is whatever was supplied, so we project the eight AABB
 * corners through that view rotation and read the view-space
 * half-extents — anything less would under-fit at non-default tilts.
 */
cwTurnTableViewState cwBaseTurnTableInteraction::framingViewState(
        const QBox3D& box, double azimuth, double pitch) const
{
    cwTurnTableViewState target = viewState();

    if (box.isNull() || Camera.isNull()) {
        return target;
    }
    if (!isFinite3(box.minimum()) || !isFinite3(box.maximum())) {
        return target;
    }

    target.azimuth = azimuth;
    target.pitch = pitch;
    target.center = box.center();
    // Framing always lands the box centered on screen — drop any pan
    // offset carried over from the current view. The animator interpolates
    // from current eyeOffset down to zero, so the pan smoothly resolves
    // toward the centered frame instead of being preserved into preview.
    target.eyeOffset = QVector3D();

    const QVector3D half = 0.5f * (box.maximum() - box.minimum());
    const float hx = (std::max)(half.x(), kFramingEpsilon);
    const float hy = (std::max)(half.y(), kFramingEpsilon);
    const float hz = (std::max)(half.z(), 0.0f);

    // Rotate the AABB corners into view space (no translation — the box is
    // already considered relative to its centroid). The view-space half
    // extents are the max-abs of each axis over the eight corners.
    QMatrix4x4 viewRot;
    viewRot.rotate(viewMatrixRotation(target.pitch, target.azimuth));

    const QVector3D corners[8] = {
        QVector3D(-hx, -hy, -hz), QVector3D( hx, -hy, -hz),
        QVector3D(-hx,  hy, -hz), QVector3D( hx,  hy, -hz),
        QVector3D(-hx, -hy,  hz), QVector3D( hx, -hy,  hz),
        QVector3D(-hx,  hy,  hz), QVector3D( hx,  hy,  hz),
    };
    float vhx = 0.0f;
    float vhy = 0.0f;
    float vhz = 0.0f;
    for (const QVector3D& c : corners) {
        const QVector3D vc = viewRot.map(c);
        vhx = (std::max)(vhx, std::abs(vc.x()));
        vhy = (std::max)(vhy, std::abs(vc.y()));
        vhz = (std::max)(vhz, std::abs(vc.z()));
    }
    vhx = (std::max)(vhx, kFramingEpsilon);
    vhy = (std::max)(vhy, kFramingEpsilon);

    switch (Camera->projection().type()) {
    case cwProjection::Ortho: {
        // Read the current ortho frustum's view-space half-extents by
        // unprojecting the viewport corners and forward-transforming back
        // into view space. For ortho, P^-1 maps NDC ±1 to ±frustum half;
        // unProject + view.map cancels the view inverse and lands exactly
        // there.
        const QSize viewportSize = Camera->viewport().size();
        const QVector3D worldTL = Camera->unProject(QPoint(0, 0), 1.0);
        const QVector3D worldBR = Camera->unProject(
                    QPoint(viewportSize.width(), viewportSize.height()), 1.0);
        const QMatrix4x4 view = Camera->viewMatrix();
        const QVector3D vTL = view.map(worldTL);
        const QVector3D vBR = view.map(worldBR);
        const float viewSpanX = std::abs(vBR.x() - vTL.x());
        const float viewSpanY = std::abs(vBR.y() - vTL.y());

        const float fitX = (2.0f * vhx) / (std::max)(viewSpanX, kFramingEpsilon);
        const float fitY = (2.0f * vhy) / (std::max)(viewSpanY, kFramingEpsilon);
        const float fit  = (std::max)(fitX, fitY) * FramingPad;
        if (!std::isfinite(fit) || fit <= 0.0f) {
            return viewState();
        }
        target.zoomScale = Camera->zoomScale() * fit;
        // distance unchanged for ortho — scale comes from zoomScale.
        break;
    }
    case cwProjection::Perspective:
    case cwProjection::PerspectiveFrustum: {
        const float fovY = Camera->projection().fieldOfView()
                * cwGlobals::degreesToRadians();
        const float aspect = Camera->projection().aspectRatio();
        const float tanHalfFovY = std::tan(0.5f * fovY);
        const float tanHalfFovX = tanHalfFovY * aspect;
        const float dY = vhy / (std::max)(tanHalfFovY, kFramingEpsilon);
        const float dX = vhx / (std::max)(tanHalfFovX, kFramingEpsilon);
        const float dist = ((std::max)(dX, dY) + vhz) * FramingPad;
        if (!std::isfinite(dist) || dist <= 0.0f) {
            return viewState();
        }
        target.distance = dist;
        // zoomScale carries no visible meaning for perspective — leave it
        // untouched so the round-trip on exit is exact.
        break;
    }
    default:
        break;
    }
    return target;
}

