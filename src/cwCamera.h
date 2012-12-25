#ifndef CWCAMERA_H
#define CWCAMERA_H

//Our includes
#include "cwProjection.h"

//Qt includes
#include <QObject>
#include <QRect>
#include <QMatrix4x4>

class cwCamera : public QObject
{
    Q_OBJECT
public:
    explicit cwCamera(QObject *parent = 0);

    void setViewport(QRect viewport);
    QRect viewport() const;

    Q_INVOKABLE void setCustomProjection(QMatrix4x4 matrix);
    void setProjection(cwProjection projection);
    cwProjection projection() const;

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
    QVector3D mapNormalizeScreenToGLViewport(const QVector3D& point) const;
    QPoint mapToGLViewport(QPoint qtViewportPoint) const;
    QPointF mapToQtViewport(QPointF glViewportPoint) const;

signals:
    void viewportChanged();
    void projectionChanged();
    void viewChanged();

public slots:

private:
    QRect Viewport;
    QMatrix4x4 ViewMatrix;
    QMatrix4x4 ViewProjectionMatrix;
    cwProjection Projection;


    //Projection matrix
    bool Perspective; //This camera is using a perspective projection
    double Left;
    double Right;
    double Bottom;
    double Top;
    double Near;
    double Far;


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

#endif // CWCAMERA_H
