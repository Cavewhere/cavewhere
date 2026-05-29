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

    // Returns the 5-channel viewState that frames @a box in the current
    // projection, preserving the user's azimuth and pitch. Unlike zoomTo(),
    // this is a const computation that does NOT call resetView(), write to
    // the camera, or change orientation — callers route the result through
    // animateToViewState() / setViewState(). Used by the sink-clip preview
    // camera layer to fit-without-snapping. A null or non-finite box yields
    // the current viewState() unchanged.
    Q_INVOKABLE cwTurnTableViewState framingViewState(const QBox3D& box) const;

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

    void zoomPerspective();
    void zoomOrtho();

    void setupInteractionTimers();

    void setCurrentRotation(QQuaternion rotation);
    QQuaternion defaultRotation() const;

    void updateRotationMatrix();

    double clampAzimuth(double azimuth) const;
    double clampPitch(double pitch) const;

    QVector3D unProject(QPoint point);

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
