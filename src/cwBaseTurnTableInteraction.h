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
class cwInersecter;
class cwCamera;
class cwMatrix4x4Animation;

//Qt 3D
#include <QPlane3D>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QComponent>

//Qt includes
#include <QTimer>
#include <QPointer>

class cwBaseTurnTableInteraction : public cwInteraction
{
    Q_OBJECT

    Q_PROPERTY(QQuaternion rotation READ rotation NOTIFY rotationChanged)
    Q_PROPERTY(double azimuth READ azimuth WRITE setAzimuth NOTIFY azimuthChanged)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY pitchChanged)
    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(int startDragDistance READ startDragDistance CONSTANT)
    Q_PROPERTY(cwInersecter* inersecter READ inersecter WRITE setInersecter NOTIFY inersecterChanged)
    Q_PROPERTY(bool pitchLocked READ pitchLocked WRITE setPitchLocked NOTIFY pitchLockedChanged)
    Q_PROPERTY(bool azimuthLocked READ azimuthLocked WRITE setAzimuthLocked NOTIFY azimuthLockedChanged)

public:
    explicit cwBaseTurnTableInteraction(QQuickItem *parent = 0);

    QQuaternion rotation() const;

    double azimuth() const;
    void setAzimuth(double azimuth);

    double pitch() const;
    void setPitch(double pitch);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

    cwInersecter* inersecter() const;
    void setInersecter(cwInersecter* inersecter);

    bool pitchLocked() const;
    void setPitchLocked(bool pitchLocked);

    bool azimuthLocked() const;
    void setAzimuthLocked(bool azimuthLocked);

    void setGridPlane(const QPlane3D& plan);

    Q_INVOKABLE void centerOn(QVector3D point, bool animate = false);

    int startDragDistance() const;

signals:
    void rotationChanged();
    void azimuthChanged();
    void pitchChanged();
    void cameraChanged();
    void sceneChanged();
    void inersecterChanged();
    void pitchLockedChanged();
    void azimuthLockedChanged();

public slots:

    void startPanning(QPoint currentMousePos);
    void pan(QPoint currentMousePos);

    void startRotating(QPoint currentMousePos);
    void rotate(QPoint currentMousePos);

    void zoom(QPoint position, int delta);

private slots:
    void rotateLastPosition();
    void zoomLastPosition();
    void translateLastPosition();

    void resetView();

    void updateViewMatrixFromAnimation(QVariant matrix);
private:
    QVector3D LastMouseGlobalPosition; //For panning
    QPlane3D PanPlane;
    QPointF LastMousePosition; //For rotation
    QQuaternion CurrentRotation;
    float Pitch;
    float Azimuth;

    bool PitchLocked = false;
    bool AzimuthLocked = false;

    QPlane3D LastDitchRotationPlane;

    const static float DefaultPitch;
    const static float DefaultAzimuth;

    QTimer* RotationInteractionTimer;
    QPoint TimeoutRotationPosition;

    QTimer* ZoomInteractionTimer;
    QPoint ZoomPosition;
    int ZoomDelta;
    double ZoomLevel; //This is for orthoginal projections, This is in pixel / meter

    QTimer* TranslateTimer;
    QPoint TranslatePosition;

    QPointer<cwCamera> Camera; //!<
    QPointer<cwInersecter> Inersecter; //!<

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



};

/**
* Returns the azimuth of the turn table
*/
inline double cwBaseTurnTableInteraction::azimuth() const {
    return Azimuth;
}

/**
* Returns the pitch of the turn table
*/
inline double cwBaseTurnTableInteraction::pitch() const {
    return Pitch;
}

/**
* Locks the pitch such that it can't change and is readonly
*/
inline bool cwBaseTurnTableInteraction::pitchLocked() const {
    return PitchLocked;
}


/**
* Locks the azimuth such that it can't change and is readonly
*/
inline bool cwBaseTurnTableInteraction::azimuthLocked() const {
    return AzimuthLocked;
}



#endif // CWTURNTABLEINTERACTION_H
