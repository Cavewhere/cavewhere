/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAMERA_H
#define CWCAMERA_H

//Our includes
#include "cwProjection.h"
#include "cwGlobals.h"

//Qt includes
#include <QObject>
#include <QRect>
#include <QMatrix4x4>
#include <QQmlEngine>

class CAVEWHERE_LIB_EXPORT cwCamera : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Camera)

    //Only valid with ortho projection
    Q_PROPERTY(double pixelsPerMeter READ pixelsPerMeter NOTIFY pixelsPerMeterChanged)
    Q_PROPERTY(double zoomScale READ zoomScale WRITE setZoomScale NOTIFY zoomScaleChanged)
    Q_PROPERTY(QMatrix4x4 viewMatrix READ viewMatrix WRITE setViewMatrix NOTIFY viewMatrixChanged)
    Q_PROPERTY(double devicePixelRatio READ devicePixelRatio WRITE setDevicePixelRatio NOTIFY devicePixelRatioChanged)
    Q_PROPERTY(QVector3D position READ position NOTIFY positionChanged FINAL)
    Q_PROPERTY(double defaultZoomScale READ defaultZoomScale CONSTANT)

public:
    explicit cwCamera(QObject *parent = 0);

    double zoomScale() const;
    void setZoomScale(double zoomScale);

    double devicePixelRatio() const;
    void setDevicePixelRatio(double devicePixelRatio);

    void setViewport(QRect viewport);
    QRect viewport() const;

    Q_INVOKABLE void setCustomProjection(QMatrix4x4 matrix);
    void setProjection(cwProjection projection);
    cwProjection projection() const;
    QMatrix4x4 projectionMatrix() const;

    void setViewMatrix(QMatrix4x4 matrix);
    QMatrix4x4 viewMatrix() const;

    QMatrix4x4 viewProjectionMatrix();

    QVector3D unProject(QPoint point, float viewDepth) const;
    QVector3D unProject(QPoint point, float viewDepth, QMatrix4x4 modelMatrix) const;
    QVector3D unProject(QPoint point, float viewDepth, QMatrix4x4 viewMatrix, QMatrix4x4 modelMatrix) const;

    QPointF project(QVector3D point) const;
    QPointF project(QVector3D point, QMatrix4x4 modelMatrix) const;
    QPointF project(QVector3D point, QMatrix4x4 viewMatrix, QMatrix4x4 modelMatrix) const;

    static QVector3D mapNormalizeScreenToGLViewport(const QVector3D& point, const QRect& viewport);
    Q_INVOKABLE QVector3D mapNormalizeScreenToGLViewport(const QVector3D& point) const;
    QPoint mapToGLViewport(QPoint qtViewportPoint) const;
    QPointF mapToQtViewport(QPointF glViewportPoint) const;

    //Utility functions
    double pixelsPerMeter() const; //Only valid with ortho projection
    cwProjection orthoProjectionDefault() const;
    cwProjection perspectiveProjectionDefault() const;

    QVector3D position() const;

    double defaultZoomScale() const { return 0.5; }

signals:
    void viewportChanged();
    void projectionChanged();
    void viewMatrixChanged();
    void pixelsPerMeterChanged();
    void zoomScaleChanged();
    void devicePixelRatioChanged();

    void positionChanged();

public slots:

private:
    QRect Viewport;
    QMatrix4x4 ViewMatrix;
    QMatrix4x4 ViewProjectionMatrix;
    cwProjection Projection;
    double DevicePixelRatio = 1.0; //!<

    double ZoomScale; //!<

    bool ViewProjectionMatrixIsDirty;
};

Q_DECLARE_METATYPE(cwCamera*)

/**
  Sets the viewport for the camera
  */
inline void cwCamera::setViewport(QRect viewport) {
    Viewport = viewport;
    emit viewportChanged();
}

/**
  Gets the viewport for the camera
  */
inline QRect cwCamera::viewport() const {
    return Viewport;
}



/**
  Gets the projection matrix for the camera
  */
inline cwProjection cwCamera::projection() const {
    return Projection;
}

inline QMatrix4x4 cwCamera::projectionMatrix() const
{
    return projection().matrix();
}

/**
  Gets the view matrix for the camera
  */
inline QMatrix4x4 cwCamera::viewMatrix() const {
    return ViewMatrix;
}

/**
  Convenience method

  Same as calling unProject(point, viewDepth, QMatrix4x4())
  */
inline QVector3D cwCamera::unProject(QPoint point, float viewDepth) const {
    return unProject(point, viewDepth, QMatrix4x4());
}

inline QPointF cwCamera::project(QVector3D point) const
{
    return project(point, viewMatrix(), QMatrix4x4());
}

inline QPointF cwCamera::project(QVector3D point, QMatrix4x4 modelMatrix) const
{
    return project(point, viewMatrix(), modelMatrix);
}



/**
  Convenience method

  Same as calling unProject(point, viewDepth, camera->viewMatrix(), modelMatrix)
  */
inline QVector3D cwCamera::unProject(QPoint point, float viewDepth, QMatrix4x4 modelMatrix) const {
    return unProject(point, viewDepth, viewMatrix(), modelMatrix);
}

/**
* @brief cwCamera::zoomScale
* @return
*/
inline double cwCamera::zoomScale() const {
    return ZoomScale;
}

/**
*
*/
inline double cwCamera::devicePixelRatio() const {
    return DevicePixelRatio;
}
#endif // CWCAMERA_H
