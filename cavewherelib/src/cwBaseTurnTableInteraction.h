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
class cwMatrix4x4Animation;

//Qt 3D
#include <QPlane3D>
#include <qbox3d.h>

//Qt includes
#include <QTimer>
#include <QPointer>
#include <QQmlEngine>
#include <QQuaternion>

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

    int startDragDistance() const;

    QPlane3D gridPlane() const { return m_gridPlane; }
    void setGridPlane(const QPlane3D& gridPlane);

    Q_INVOKABLE cwRayHit pick(QPointF qtViewPoint) const;

    Q_INVOKABLE void zoomTo(const QBox3D& box);

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
    bool m_azimuthLocked = false;
    bool m_pitchLocked = false;
    QPlane3D m_gridPlane;

    QVector3D m_center; //!< World-space orbit/look-at center, used as the pivot for rotation
    bool m_centerLocked = false; //!< When true, startRotating/startPanning don't move m_center

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
