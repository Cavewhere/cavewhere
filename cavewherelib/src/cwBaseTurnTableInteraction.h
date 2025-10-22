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
#include "cwRayTriangleHit.h"
#include "cwScene.h"
class cwMatrix4x4Animation;

//Qt 3D
#include <QPlane3D>

//Qt includes
#include <QTimer>
#include <QPointer>
#include <QQmlEngine>
#include <QObjectBindableProperty>

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

    Q_PROPERTY(bool azimuthLocked READ isAzimuthLocked WRITE setAzimuthLocked BINDABLE azimuthLockedBinding)
    Q_PROPERTY(bool pitchLocked READ isPitchLocked WRITE setPitchLocked BINDABLE pitchLockedBinding)

    Q_PROPERTY(QPlane3D gridPlane READ gridPlane WRITE setGridPlane NOTIFY gridPlaneChanged BINDABLE bindableGridPlane)

public:
    explicit cwBaseTurnTableInteraction(QQuickItem *parent = 0);

    QQuaternion cameraRotation() const;

    double azimuth() const;
    void setAzimuth(double azimuth);

    bool isAzimuthLocked() const { return m_azimuthLocked; }
    void setAzimuthLocked(bool locked) { m_azimuthLocked = locked; }
    QBindable<bool> azimuthLockedBinding() { return &m_azimuthLocked; }

    bool isPitchLocked() const { return m_pitchLocked; }
    void setPitchLocked(bool locked) { m_pitchLocked = locked; }
    QBindable<bool> pitchLockedBinding() { return &m_pitchLocked; }

    double pitch() const;
    void setPitch(double pitch);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

    cwScene* scene() const;
    void setScene(cwScene* scene);

    Q_INVOKABLE void centerOn(QVector3D point, bool animate = false);

    int startDragDistance() const;

    QPlane3D gridPlane() const { return m_gridPlane.value(); }
    void setGridPlane(const QPlane3D& gridPlane) { m_gridPlane = gridPlane; }
    QBindable<QPlane3D> bindableGridPlane() { return &m_gridPlane; }

    Q_INVOKABLE cwRayTriangleHit pick(QPointF qtViewPoint) const;

    Q_INVOKABLE void zoomTo(const QBox3D& box);

signals:
    void cameraRotationChanged();
    void azimuthChanged();
    void pitchChanged();
    void cameraChanged();
    void sceneChanged();
    void gridPlaneChanged();

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

    void updateViewMatrixFromAnimation(QVariant matrix);

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwBaseTurnTableInteraction, bool, m_azimuthLocked, false);
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwBaseTurnTableInteraction, bool, m_pitchLocked, false);
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwBaseTurnTableInteraction,
                                         QPlane3D, m_gridPlane,
                                         QPlane3D(),
                                         &cwBaseTurnTableInteraction::gridPlaneChanged);

    QVector3D LastMouseGlobalPosition; //For panning
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

    cwMatrix4x4Animation* ViewMatrixAnimation;

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
