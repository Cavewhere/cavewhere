/**************************************************************************
**
**    Copyright (C) 2014 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/


#ifndef CWBASETURNTABLEINTERACTION_H
#define CWBASETURNTABLEINTERACTION_H

//Our includes
#include "cwInteraction.h"
#include "cwCamera.h"
#include "cwRayHit.h"
#include "cwScene.h"
#include "cwTurnTableViewState.h"

//Qt 3D
#include <QPlane3D>
#include <qbox3d.h>

//Qt includes
#include <QTimer>
#include <QPointer>
#include <QQmlEngine>
#include <QQuaternion>
#include <QVariantAnimation>

//Std includes
#include <optional>

class cwBaseTurnTableInteraction : public cwInteraction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BaseTurnTableInteraction)

    Q_PROPERTY(QQuaternion cameraRotation READ cameraRotation NOTIFY cameraRotationChanged)
    Q_PROPERTY(double azimuth READ azimuth WRITE setAzimuth NOTIFY azimuthChanged)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY pitchChanged)
    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(cwScene* scene READ scene WRITE setScene NOTIFY sceneChanged)
    Q_PROPERTY(int startDragDistance READ startDragDistance CONSTANT)

    Q_PROPERTY(bool azimuthLocked READ isAzimuthLocked WRITE setAzimuthLocked NOTIFY azimuthLockedChanged)
    Q_PROPERTY(bool pitchLocked READ isPitchLocked WRITE setPitchLocked NOTIFY pitchLockedChanged)

    Q_PROPERTY(QVector3D center READ center WRITE setCenter NOTIFY centerChanged)
    Q_PROPERTY(bool centerLocked READ isCenterLocked WRITE setCenterLocked NOTIFY centerLockedChanged)

    Q_PROPERTY(QPlane3D gridPlane READ gridPlane WRITE setGridPlane NOTIFY gridPlaneChanged)

public:
    // Default duration (ms) for animateToViewState() when no explicit
    // duration is supplied. Used both as the header default-arg value and
    // as the QVariantAnimation initial duration in the constructor.
    static constexpr int DefaultStateAnimationDurationMs = 1000;

    // objectName attached to the state-animation child QVariantAnimation,
    // so tests can locate it via QObject::findChild without exposing the
    // animator pointer on the public API.
    static constexpr const char* stateAnimationObjectName = "stateAnimation";

    // Margin multiplier applied by zoomTo() so the framed box doesn't sit
    // flush against the viewport edges. Exposed so tests can use the same
    // value without duplicating the literal.
    static constexpr float FramingPad = 1.08f;

    // Screen-space reach (physical millimeters) of the rotation pivot's
    // nearest-geometry anchor (issue #562): when a rotation click misses the
    // exact pick, the pivot snaps to the closest geometry point within this
    // radius of the cursor; farther clicks keep the current pivot. Much wider
    // than the exact pick radius — it's a "grab what I'm looking at" reach,
    // not a selection. Exposed so tests can derive the same world-space
    // radius through cwCamera::pickQuery.
    static constexpr double PivotAnchorRadiusMillimeters = 20.0;

    explicit cwBaseTurnTableInteraction(QQuickItem *parent = 0);

    QQuaternion cameraRotation() const;

    double azimuth() const;
    void setAzimuth(double azimuth);

    bool isAzimuthLocked() const { return m_azimuthLocked; }
    void setAzimuthLocked(bool locked);

    bool isPitchLocked() const { return m_pitchLocked; }
    void setPitchLocked(bool locked);

    double pitch() const;
    void setPitch(double pitch);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

    cwScene* scene() const;
    void setScene(cwScene* scene);

    QVector3D center() const { return m_center; }
    void setCenter(QVector3D center);

    bool isCenterLocked() const { return m_centerLocked; }
    void setCenterLocked(bool locked);

    Q_INVOKABLE void centerOn(QVector3D point, bool animate = false);

    Q_INVOKABLE cwTurnTableViewState viewState() const;
    Q_INVOKABLE void setViewState(const cwTurnTableViewState& state);

    Q_INVOKABLE void animateToViewState(const cwTurnTableViewState& target,
                                        int durationMs = DefaultStateAnimationDurationMs);

    int startDragDistance() const;

    QPlane3D gridPlane() const { return m_gridPlane; }
    void setGridPlane(const QPlane3D& gridPlane);

    Q_INVOKABLE cwRayHit pick(QPointF qtViewPoint) const;

    Q_INVOKABLE void zoomTo(const QBox3D& box);

    // Converts the inactive zoom channel so the visible world half-height at
    // the orbit center is preserved across an ortho<->perspective swap (issue
    // #513). The two modes encode zoom differently — ortho in zoomScale (the
    // frustum half-height is viewportHeight/2 * zoomScale), perspective in the
    // eye-to-center distance (half-height is distance * tan(fov/2)). toPerspective
    // selects the direction (true: set the eye distance from the current ortho
    // zoom; false: set the ortho zoom from the current eye distance).
    // fovRadians is the perspective field of view to convert against. This is a
    // pure service: cwProjectionTransition calls it at the start of a projection
    // transition; the interaction itself never initiates a swap.
    void reconcileZoomForProjection(bool toPerspective, double fovRadians);

    // Returns the 5-channel viewState that frames @a box at the supplied
    // @a azimuth / @a pitch (degrees). Unlike zoomTo() this is const, does
    // NOT reset the view, and uses the supplied orientation for BOTH the
    // fit math AND the returned target — so the AABB is sized against the
    // post-rotation view, not the current one. Lets callers snap to a
    // canonical orientation (e.g. pitch=0 profile view for a sink) and
    // still get a tight fit. Box-null, camera-null, or non-finite box
    // falls through to the current viewState() unchanged. Caller routes
    // the result through animateToViewState() / setViewState().
    Q_INVOKABLE cwTurnTableViewState framingViewState(const QBox3D& box,
                                                      double azimuth,
                                                      double pitch) const;

signals:
    void cameraRotationChanged();
    void azimuthChanged();
    void pitchChanged();
    void cameraChanged();
    void sceneChanged();
    void gridPlaneChanged();
    void azimuthLockedChanged();
    void pitchLockedChanged();
    void centerChanged();
    void centerLockedChanged();
    void animationFinished();

public slots:

    void startPanning(QPoint currentMousePos);
    void pan(QPoint currentMousePos);

    void startRotating(QPoint currentMousePos);
    void rotate(QPoint currentMousePos);

    void zoom(QPoint position, double delta);

    //! Frames the whole scene (the geometry intersecter's root bounding box)
    //! at the default orientation, animating there from the current view.
    //! Falls back to the fixed default pose via resetViewImmediate() when the
    //! scene is empty or its bounding box is degenerate.
    void resetView();

private slots:
    void rotateLastPosition();
    void zoomLastPosition();
    void translateLastPosition();

    void onStateAnimTick(const QVariant& value);

private:
    bool m_azimuthLocked = false;
    bool m_pitchLocked = false;
    QPlane3D m_gridPlane;

    QVector3D m_center; //!< World-space orbit/look-at center, used as the pivot for rotation
    bool m_centerLocked = false; //!< When true, startRotating/startPanning don't move m_center

    //! World-space anchor that the active pan drag tracks under the cursor.
    //! Always set to the click's ray-plane intersection at startPanning,
    //! independent of m_centerLocked — that's how pan stays smooth while
    //! the rotation pivot (m_center) is pinned to e.g. the sink centroid.
    QVector3D m_panAnchor;

    QPlane3D PanPlane;
    QPointF LastMousePosition; //For rotation
    QQuaternion CurrentRotation;
    float Pitch;
    float Azimuth;

    static float defaultPitch() { return 90.0f; }
    static float defaultAzimuth() { return 0.0f; }

    QTimer* RotationInteractionTimer;
    QPoint TimeoutRotationPosition;

    QTimer* ZoomInteractionTimer;
    QPoint ZoomPosition;
    double ZoomDelta;
    double ZoomLevel; //This is for orthoginal projections, This is in pixel / meter

    QTimer* TranslateTimer;
    QPoint TranslatePosition;

    //For zoom perspective
    QProperty<QPoint> m_perspectiveMappedPos;
    QProperty<QVector3D> m_perspectiveIntersection; //The intersection with geometry under the mouse

    QPointer<cwCamera> Camera; //!<
    QPointer<cwScene> Scene; //!<

    // State-based animator: drives setViewState() per tick from a captured
    // start state to a target state, both stored as 5-channel snapshots.
    // The animation itself is a scalar t in [0, 1]; per-tick interpolation
    // happens in onStateAnimTick().
    QVariantAnimation* m_stateAnimation;
    cwTurnTableViewState m_stateAnimStart;
    cwTurnTableViewState m_stateAnimTarget;

    //! Instantaneously restores the default orientation and a fixed eye pose
    //! (no scene framing). Used for initial setup (constructor, setCamera),
    //! as the orientation reset inside zoomTo(), and as resetView()'s
    //! empty-scene fallback.
    void resetViewImmediate();

    void zoomPerspective();
    void zoomOrtho();

    void setupInteractionTimers();

    void setCurrentRotation(QQuaternion rotation);
    QQuaternion defaultRotation() const;

    void updateRotationMatrix();

    double clampAzimuth(double azimuth) const;
    double clampPitch(double pitch) const;

    //! Picks a world point under @a point (in GL viewport coords), else a
    //! grid-plane hit (only when the scene has no geometry), else nullopt so
    //! callers keep the current pivot instead of teleporting (issues #527,
    //! #562).
    //!
    //! With @a anchorToNearestGeometry false (pan, zoom) this is the nearest
    //! exact hit of any kind: those callers track whatever is under the cursor.
    //!
    //! With it true (rotation) the pick sorts by kind, because a user orbits
    //! what they authored, not the LiDAR cloud that happens to be behind it.
    //! Survey geometry (scraps, LiDAR notes, centerline) competes with BOTH an
    //! exact hit and a near-miss anchor within PivotAnchorRadiusMillimeters.
    //! The cloud competes with an exact hit only — its pick spheres are sized
    //! to be watertight (cwRenderPointCloud::PointPickRadiusScale), so a cloud
    //! surface the user is actually over always yields one. Depth then decides
    //! between the two winners: a near-missed scrap beats a far cloud point,
    //! while a cloud genuinely occluding the scrap still takes the pivot —
    //! orbiting geometry hidden behind a LiDAR wall would just trade one
    //! teleport for another.
    //!
    //! If BOTH miss, the cloud gets the wide anchor too, as a last rung. A
    //! LiDAR-only project has no survey geometry to lose to and still has to be
    //! orbitable (#562's "can't rotate around a point cloud point").
    //!
    //! Only a triangle hit is strictly on the cursor ray. A line hit returns
    //! the closest point on the segment and a point hit returns the vertex
    //! center, so an exact hit can sit off-ray by up to the pick radius
    //! (~kPivotPickRadiusMillimeters of screen). Callers doing delta math
    //! against the result (pan, zoom) inherit that as a small mouse-down step.
    //! The anchor widens that error to PivotAnchorRadiusMillimeters, which
    //! orbits fine but would visibly jerk a pan or drift a zoom — the other
    //! reason only rotation opts in.
    std::optional<QVector3D> unProject(QPoint point,
                                       bool anchorToNearestGeometry = false) const;

    //! World point on the cursor ray at the depth of the current orbit center.
    //! Used as the pan anchor when unProject misses, so panning stays attached
    //! to the cursor without moving the pivot. @a mappedPoint is in GL viewport
    //! coords. Falls back to m_center if the ray is parallel to the plane.
    QVector3D rayPointAtCenterDepth(QPoint mappedPoint) const;

    void stopAnimation();

    void bindPerspectiveIntersection();
};

/**
* @brief cw3dRegionViewer::azimuth
* @return
*/
inline double cwBaseTurnTableInteraction::azimuth() const {
    return Azimuth;
}

/**
* @brief cw3dRegionViewer::pitch
* @return
*/
inline double cwBaseTurnTableInteraction::pitch() const {
    return Pitch;
}


#endif // CWTURNTABLEINTERACTION_H
