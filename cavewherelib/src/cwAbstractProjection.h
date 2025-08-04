/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWABSTRACTPROJECTION_H
#define CWABSTRACTPROJECTION_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QMatrix4x4>

//Our includes
#include "cwProjection.h"
// #include "cw3dRegionViewer.h"
class cw3dRegionViewer;

class cwAbstractProjection : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AbstractProjection)
    QML_UNCREATABLE("AbstractProjection is a base class for all projection classes. It can't be created")

    Q_PROPERTY(cw3dRegionViewer* viewer READ viewer WRITE setViewer NOTIFY viewerChanged)
    Q_PROPERTY(double nearPlane READ nearPlane WRITE setNearPlane NOTIFY nearPlaneChanged)
    Q_PROPERTY(double farPlane READ farPlane WRITE setFarPlane NOTIFY farPlaneChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QMatrix4x4 matrix READ matrix NOTIFY matrixChanged)


public:
    explicit cwAbstractProjection(QObject *parent = 0);

    cw3dRegionViewer* viewer() const;
    void setViewer(cw3dRegionViewer* viewer);

    double nearPlane() const;
    void setNearPlane(double nearPlane);

    double farPlane() const;
    void setFarPlane(double farPlane);

    bool enabled() const;
    void setEnabled(bool enabled);

    QMatrix4x4 matrix() const;

protected:
    virtual cwProjection calculateProjection() = 0;

    cwProjection projection() const;

    void setPrivateNearPlane(double nearPlane);
    void setPrivateFarPlane(double farPlane);

signals:
    void viewerChanged();
    void nearPlaneChanged();
    void farPlaneChanged();
    void enabledChanged();
    void matrixChanged();

protected slots:
    void updateProjection();

private:
    cw3dRegionViewer* Viewer;

    double NearPlane;
    double FarPlane;

    bool Enabled;

    cwProjection Projection;


};

#include "cw3dRegionViewer.h"

/**
 * @brief cwAbstractProjection::projection
 * @return
 */
inline cwProjection cwAbstractProjection::projection() const
{
    return Projection;
}

inline void cwAbstractProjection::setPrivateNearPlane(double nearPlane)
{
    NearPlane = nearPlane;
}

inline void cwAbstractProjection::setPrivateFarPlane(double farPlane)
{
    FarPlane = farPlane;
}

/**
 * @brief cwAbstractProjection::viewer
 * @return
 */
inline cw3dRegionViewer* cwAbstractProjection::viewer() const {
    return Viewer;
}

/**
 * @brief cwAbstractProjection::nearPlane
 * @return
 */
inline double cwAbstractProjection::nearPlane() const {
    return NearPlane;
}

/**
 * @brief cwAbstractProjection::farPlane
 * @return
 */
inline double cwAbstractProjection::farPlane() const {
    return FarPlane;
}

/**
 * @brief cwAbstractProjection::enabled
 * @return True if this abstract projection changes the view
 */
inline bool cwAbstractProjection::enabled() const {
    return Enabled;
}

/**
 * @brief cwAbstractProjection::matrix
 * @return The matrix of the projection
 */
inline QMatrix4x4 cwAbstractProjection::matrix() const {
    return projection().matrix();
}
#endif // CWABSTRACTPROJECTION_H
