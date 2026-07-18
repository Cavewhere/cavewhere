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
#include "cwRayHit.h"

//Std includes
#include "math.h"

//Qt includes
#include <QStyleHints>
#include <QGuiApplication>

Q_LOGGING_CATEGORY(lcInteract, "cw.interaction", QtWarningMsg)

namespace {
    // Everything a user authors or surveys: scrap carpets, LiDAR notes (which
    // render as triangles through the same path as scraps) and the centerline.
    // Deliberately excludes Kind::Points — the LiDAR cloud — which the rotation
    // pivot treats as a second-class citizen. See unProject().
    constexpr cwPickQuery::Kinds kSurveyGeometryKinds =
        cwPickQuery::Kinds(cwPickQuery::Kind::Triangles) | cwPickQuery::Kind::Lines;

}

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

    resetViewImmediate();

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
 * @param point - screen point in GL viewport coordinates
 * @return World point under the cursor, or nullopt on a miss
 *
 * See the declaration for the full ladder. The constraint that shapes it: the
 * anchor returns a point OFF the cursor ray. Rotation and zoom both opt in —
 * rotation orbits the point directly, and zoom (issue #578) uses only its depth
 * along the cursor ray, so the off-ray offset is harmless; pan keeps the
 * exact-hit path.
 */
std::optional<QVector3D> cwBaseTurnTableInteraction::unProject(QPoint point,
                                                               bool anchorToNearestGeometry) const {
    Q_ASSERT(Camera);
    Q_ASSERT(scene());
    Q_ASSERT(scene()->geometryItersecter());

    //Create a ray from the back projection front and back plane
    const QRay3D ray = Camera->frustrumRay(point);
    const cwGeometryItersecter* intersecter = scene()->geometryItersecter();

    const cwPickQuery pivotQuery =
            Camera->pickQuery(pixelsForMillimeters(PivotPickRadiusMillimeters));

    //The grid plane is a last-resort pivot, but only when there's nothing to
    //anchor against (an empty/unloaded project). Once survey geometry exists, an
    //off-geometry click must not pivot on the grid: the infinite grid plane sits
    //at the cave floor (min Z), so a grazing hit lands far out in X/Y and
    //teleports the view, losing the rotation context (issues #562, #527). In
    //that case return nullopt so the caller keeps the current pivot instead.
    //
    //This asks the intersecter what a pick can actually reach, not merely what
    //exists: hiding every object leaves the picks with nothing, so the grid has
    //to come back or the pivot would be stuck with no way to recover.
    const auto gridPivot = [&]() -> std::optional<QVector3D> {
        const bool hasGeometry = !intersecter->isPickableEmpty();
        if(!hasGeometry) {
            const double gridT = m_gridPlane.intersection(ray);
            if(!std::isnan(gridT)) {
                const QVector3D gridHit = ray.point(gridT);
                qCDebug(lcInteract).nospace()
                    << "unProject(" << point << "): gridPlane t=" << gridT
                    << " world=" << gridHit << " rayOrigin=" << ray.origin();
                return gridHit;
            }
        }
        qCDebug(lcInteract).nospace()
            << "unProject(" << point << "): no geometry pivot"
            << " (hasGeometry=" << hasGeometry << ") -> miss"
            << " rayOrigin=" << ray.origin() << " rayDir=" << ray.direction();
        return std::nullopt;
    };

    //Pan and zoom want the point under the cursor, whatever it belongs to, so
    //they take the nearest exact hit of any kind. Only rotation sorts by kind
    //below — it is choosing something to orbit, not something to track.
    if(!anchorToNearestGeometry) {
        const cwRayHit hit = intersecter->intersectsDetailed(ray, pivotQuery);
        if(hit.hit()) {
            // Project the hit onto the cursor ray. A line hit returns the
            // closest point on the segment and a point hit returns the vertex
            // center, so an exact hit can sit off the ray by up to the pick
            // radius. Pan and zoom do delta math against this point, so an
            // off-ray anchor would lurch the first pan tick (and drift zoom) by
            // the miss distance. Projecting keeps the geometry's depth while
            // placing the anchor exactly under the cursor; a triangle hit is
            // already on-ray, so this is a no-op for it.
            const QVector3D world = ray.point(ray.projectedDistance(hit.pointWorld()));
            qCDebug(lcInteract).nospace()
                << "unProject(" << point << "): geometry world=" << world
                << " rayOrigin=" << ray.origin();
            return world;
        }
        return gridPivot();
    }

    //Rotation. Survey geometry — scraps, LiDAR notes, the centerline — is what
    //a user means to orbit, so it competes with BOTH an exact hit and the wider
    //near-miss anchor, while the point cloud competes only with an exact hit.
    //Depth then picks the winner between the two, with no priority rule: that
    //yields "geometry first" (a near-missed scrap beats a far cloud point,
    //issue #562) while still letting a cloud that genuinely occludes the
    //geometry take the pivot, rather than orbiting something out of sight.
    //
    //Splitting the kinds costs up to four top-level descents per press (two
    //here, plus an anchor each). The kind filter rejects a whole Object before
    //its sub-BVH is entered, so the cloud's 100M+ points are skipped outright
    //by the geometry query; the extra cost is top-level only, and this runs per
    //mouse-down. Deliberately NOT merged into one traversal: a shared best-depth
    //prune would let a near cloud hit discard the farther geometry hit that the
    //comparison below needs computed independently.
    cwPickQuery geometryQuery = pivotQuery;
    geometryQuery.kinds = kSurveyGeometryKinds;
    cwPickQuery cloudQuery = pivotQuery;
    cloudQuery.kinds = cwPickQuery::Kinds(cwPickQuery::Kind::Points);

    const cwRayHit geometryHit = intersecter->intersectsDetailed(ray, geometryQuery);
    const cwRayHit cloudHit = intersecter->intersectsDetailed(ray, cloudQuery);

    //The exact hit is on the ray; the anchor is the closest point ON geometry
    //within a much wider reach, so a near-miss still pivots on the geometry the
    //user aimed at instead of teleporting. Triangles are picked by an exact
    //ray-triangle test that never consults the tolerance, so for a scrap the
    //anchor is the ONLY thing that can catch a near-miss.
    std::optional<QVector3D> geometryPivot;
    double geometryDepth = 0.0;
    if(geometryHit.hit()) {
        geometryPivot = geometryHit.pointWorld();
        geometryDepth = geometryHit.tWorld();
    } else {
        cwPickQuery anchorQuery =
                Camera->pickQuery(pixelsForMillimeters(PivotAnchorRadiusMillimeters));
        anchorQuery.kinds = kSurveyGeometryKinds;
        if(const std::optional<QVector3D> anchor =
                intersecter->nearestGeometryPoint(ray, anchorQuery)) {
            geometryPivot = anchor;
            //The same measure nearestGeometryPoint ranks its own candidates by.
            geometryDepth = ray.projectedDistance(*anchor);
        }
    }

    //Not quite like-for-like, and knowingly so: geometryDepth is the depth of
    //the point returned, while a cloud exact hit reports its SPHERE-ENTRY depth
    //(the exact point pick ranks by tNear, deliberately, so a head-on point beats a
    //grazing one) even though the pivot it returns is the sphere CENTRE. So the
    //cloud is favoured by up to one pickRadius — one mean point spacing, a few
    //cm in a real cave. That is well under what a pivot shift needs to be for
    //an orbit to look wrong, and erring toward the thing that visually occludes
    //is the right direction to err, so it is left alone. Comparing centre depth
    //instead (ray.projectedDistance(cloudHit.pointWorld())) would remove the
    //bias if this ever matters.
    if(geometryPivot && (!cloudHit.hit() || geometryDepth < cloudHit.tWorld())) {
        qCDebug(lcInteract).nospace()
            << "unProject(" << point << "): survey geometry=" << *geometryPivot
            << " depth=" << geometryDepth
            << " (cloudHit=" << cloudHit.hit() << " cloudDepth=" << cloudHit.tWorld() << ")"
            << " rayOrigin=" << ray.origin();
        return geometryPivot;
    }

    if(cloudHit.hit()) {
        const QVector3D world = cloudHit.pointWorld();
        qCDebug(lcInteract).nospace()
            << "unProject(" << point << "): point cloud occludes=" << world
            << " depth=" << cloudHit.tWorld()
            << " rayOrigin=" << ray.origin();
        return world;
    }

    //Nothing under the cursor at all. Second-class is not last-resort-less: a
    //LiDAR-only project (a scan with no scraps drawn yet) still has to be
    //orbitable, so the cloud gets the wide anchor too — but only here, after
    //survey geometry has had both of its chances. This is #562's original
    //"can't rotate around a point cloud point".
    cwPickQuery cloudAnchorQuery =
            Camera->pickQuery(pixelsForMillimeters(PivotAnchorRadiusMillimeters));
    cloudAnchorQuery.kinds = cwPickQuery::Kinds(cwPickQuery::Kind::Points);
    if(const std::optional<QVector3D> cloudAnchor =
            intersecter->nearestGeometryPoint(ray, cloudAnchorQuery)) {
        qCDebug(lcInteract).nospace()
            << "unProject(" << point << "): point cloud anchor=" << *cloudAnchor
            << " rayOrigin=" << ray.origin();
        return cloudAnchor;
    }

    return gridPivot();
}

QVector3D cwBaseTurnTableInteraction::rayPointAtCenterDepth(QPoint mappedPoint) const {
    Q_ASSERT(Camera);

    // Plane through the current center, facing the camera. The ray's direction
    // sign doesn't matter — it's only used as the plane normal and the ray
    // parameter, both of which yield the same world point on the plane.
    const QRay3D ray = Camera->frustrumRay(mappedPoint);
    const QPlane3D plane(m_center, ray.direction());

    const double t = plane.intersection(ray);
    if(qIsNaN(t)) {
        return m_center;
    }
    return ray.point(t);
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
        m_perspectiveIntersection.setBinding([this]() -> QVector3D {
            // The zoom target: the nearest geometry within the wide anchor reach
            // (issue #578) so a cursor near — not exactly on — a wall still zooms
            // toward it, falling back to the pivot when nothing is near. This may
            // sit off the cursor ray, but zoomPerspective consumes only its depth
            // along that ray (cursorRay.projectedDistance), which ignores the
            // off-ray offset, so the raw point needs no on-axis projection here.
            return unProject(m_perspectiveMappedPos, true).value_or(m_center);
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
    const std::optional<QVector3D> clickWorld = unProject(mappedPosition);

    if(clickWorld && !m_centerLocked) {
        setCenter(*clickWorld);
    }

    // The pan anchor is always a point under the cursor — independent of the
    // orbit-pivot lock. On a real pick it's the click point; on a miss it's
    // the cursor ray at the current center's depth, so the pan tracks the
    // cursor without teleporting the pivot (issue #527). m_center stays where
    // it is (locked: sink centroid; unlocked: just got set to the pick too).
    m_panAnchor = clickWorld ? *clickWorld : rayPointAtCenterDepth(mappedPosition);

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
        // Only re-center on a real pick. On a miss keep m_center so the view
        // orbits the existing pivot instead of teleporting (issue #527).
        // m_center is also zoom's miss target: zoomPerspective zooms along the
        // cursor ray at m_center's depth when nothing is under the cursor.
        if(const std::optional<QVector3D> picked = unProject(position, true)) {
            setCenter(*picked);
        }
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
  \brief Resets the view to frame the whole scene

  Animates from the current view to the default orientation framed on the
  scene's visible framing bounds, so a model whose datum places it far from
  the origin (issue #549) is brought back into view without zooming out to
  include geometry the user hid. When the scene is empty, everything is
  hidden, or the bounds are degenerate there is nothing to frame, so fall
  back to the fixed default pose.
  */
void cwBaseTurnTableInteraction::resetView() {
    if(Camera.isNull() || scene() == nullptr) {
        resetViewImmediate();
        return;
    }

    const QBox3D box = scene()->visibleFramingBounds();
    if(box.isNull() || !box.isFinite()) {
        resetViewImmediate();
        return;
    }

    // framingViewState()'s fit is now an absolute, pure function of the box and
    // viewport (its ortho branch no longer scales off the live zoom — issue
    // #549), so we compute the target directly and animate from wherever the
    // user currently is. No normalize-measure-restore bounce through the
    // default pose, and no spurious pitch/azimuth/viewMatrix signals from it.
    const cwTurnTableViewState target =
            framingViewState(box, defaultAzimuth(), defaultPitch());
    animateToViewState(target);
}

/**
  \brief Instantly restores the default orientation and a fixed eye pose
  */
void cwBaseTurnTableInteraction::resetViewImmediate() {
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

    const double delta = ZoomDelta;

    //Make the event position into gl viewport
    const QPoint mappedPos = Camera->mapToGLViewport(ZoomPosition);
    m_perspectiveMappedPos = mappedPos; //Update the mapped position

    //Cursor ray: origin on the near plane, direction into the scene. The zoom
    //target (m_perspectiveIntersection) is a point on this ray — the near
    //geometry's depth on a hit, the pivot's depth on a miss. Recomputed by the
    //binding whenever m_perspectiveMappedPos changed above.
    const QRay3D cursorRay = Camera->frustrumRay(mappedPos);
    const QVector3D target = m_perspectiveIntersection.value();

    //Signed distance from the near plane to the target along the ray: positive
    //while the target is still ahead, <= 0 once the near plane reaches its depth.
    const double aheadDistance = cursorRay.projectedDistance(target);

    const double t = qBound(-1.0, delta, 1.0);

    //Zoom in only while the target is ahead. Clamping the ahead-distance at 0
    //makes the zoom stall once it reaches the target instead of letting the
    //direction flip and run the zoom backward when the near plane passes the
    //target depth (issue #578) — the perspective analogue of ortho zoom
    //asymptotically approaching the cursor point. For a target still ahead this
    //is identical to moving t of the way toward it. Zoom out is unconstrained.
    const double step = (t >= 0.0) ? qMax(0.0, aheadDistance) * t
                                   : aheadDistance * t;
    const QVector3D newPositionDelta = cursorRay.direction() * step;

    qCDebug(lcInteract).nospace()
        << "zoomPerspective mapped=" << mappedPos
        << " origin=" << cursorRay.origin()
        << " target=" << target
        << " aheadDistance=" << aheadDistance
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
// resetViewImmediate() leaves the view matrix as a plain T(0,0,-50) — i.e. no
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
        resetViewImmediate();
        emit cameraChanged();
    }
}

/**
 * @brief cwBaseTurnTableInteraction::reconcileZoomForProjection
 *
 * Converts the inactive zoom channel so the visible world half-height at the
 * orbit center is preserved across an ortho<->perspective swap. See the
 * header for the channel relationship.
 */
void cwBaseTurnTableInteraction::reconcileZoomForProjection(bool toPerspective, double fovRadians)
{
    if(Camera.isNull()) { return; }

    const double halfViewportHeight = Camera->viewport().height() / 2.0;
    if(halfViewportHeight <= 0.0) { return; }

    const double tanHalfFov = std::tan(0.5 * fovRadians);
    if(!std::isfinite(tanHalfFov) || tanHalfFov <= 0.0) { return; }

    cwTurnTableViewState state = viewState();

    if(toPerspective) {
        // Solve distance * tan(fov/2) == halfViewportHeight * zoomScale.
        state.distance = halfViewportHeight * state.zoomScale / tanHalfFov;
    } else {
        // Invert the same relation for zoomScale. distance is read from the
        // live (still perspective-built) view matrix.
        state.zoomScale = state.distance * tanHalfFov / halfViewportHeight;
    }

    setViewState(state);
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
    return scene()->geometryItersecter()->intersectsDetailed(
                ray, Camera->pickQuery(pixelsForMillimeters(PivotPickRadiusMillimeters)));
}

void cwBaseTurnTableInteraction::zoomTo(const QBox3D &box)
{
    // box.isNull() only catches the default-constructed case; an explicit
    // finite check guards against NaN/inf min/max propagating into the fit
    // math and poisoning the view matrix. Bail before touching the camera so
    // a bad box is a true no-op (framingViewState would otherwise echo the
    // current state straight back).
    if (box.isNull()
            || !isFinite3(box.minimum()) || !isFinite3(box.maximum())) {
        return;
    }

    // zoomTo is the default-orientation special case of framingViewState:
    // frame the box at the reset pose (azimuth=0, pitch=90 → identity view
    // rotation, so the rotated half-extents collapse to the raw box extents)
    // and snap to it. framingViewState owns the single copy of the ortho and
    // perspective fit math; setViewState is the authoritative writer that
    // applies the view matrix and zoom.
    setViewState(framingViewState(box, defaultAzimuth(), defaultPitch()));
}

/**
 * @brief cwBaseTurnTableInteraction::framingViewState
 *
 * Returns the 5-channel viewState that frames @a box at the supplied
 * @a azimuth / @a pitch (degrees). Pure and const: it reads no live camera
 * pose and mutates nothing, using the supplied orientation for BOTH the fit
 * math AND the returned target — so the AABB is sized against the
 * post-rotation view, not the current one. Box-null, camera-null, or
 * non-finite box falls through to the current viewState() unchanged.
 * Caller routes the result through animateToViewState() / setViewState()
 * (zoomTo() is the default-pose caller).
 *
 * Fit math runs in VIEW space, not world space: the box is framed at the
 * supplied orientation, so the eight AABB corners are projected through that
 * view rotation and the view-space half-extents read off them. Using the raw
 * world half-extents would under-fit at any non-default tilt.
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
        // Absolute ortho fit. cwCamera::orthoProjectionDefault sets the
        // frustum half-width to viewport/2 * zoomScale, so the full-viewport
        // view-space span at any zoom is viewportDim * zoomScale. The zoomScale
        // that makes the box's view-space extent (2*vh) fill that span — plus
        // pad — is therefore 2*vh*FramingPad / viewportDim, independent of the
        // current zoom. Take the larger axis so both fit. Being pose- and
        // zoom-independent is what lets resetView() frame without first
        // normalizing the camera to the default zoom.
        const QSizeF viewport = Camera->viewport().size();
        const float vpW = (std::max)(static_cast<float>(viewport.width()),
                                     kFramingEpsilon);
        const float vpH = (std::max)(static_cast<float>(viewport.height()),
                                     kFramingEpsilon);
        const float fitX = (2.0f * vhx) / vpW;
        const float fitY = (2.0f * vhy) / vpH;
        const float fit  = (std::max)(fitX, fitY) * FramingPad;
        if (!std::isfinite(fit) || fit <= 0.0f) {
            return viewState();
        }
        target.zoomScale = fit;
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

