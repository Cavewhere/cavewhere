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

//Qt includes
#include <QObject>
#include <QRect>
#include <QMatrix4x4>
#include <QCamera>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QCameraLens>


class cwCamera : public Qt3DCore::QEntity
{
    Q_OBJECT

    //Only valid with ortho projection
    Q_PROPERTY(double pixelsPerMeter READ pixelsPerMeter NOTIFY pixelsPerMeterChanged)
    Q_PROPERTY(double zoomScale READ zoomScale WRITE setZoomScale NOTIFY zoomScaleChanged)

    //Only valid with perspective projections
    Q_PROPERTY(double fieldOfView READ fieldOfView WRITE setFieldOfView NOTIFY fieldOfViewChanged)

    //Camera settings
    Q_PROPERTY(QMatrix4x4 viewMatrix READ viewMatrix WRITE setViewMatrix NOTIFY viewMatrixChanged)
    Q_PROPERTY(QRect viewport READ viewport WRITE setViewport NOTIFY viewportChanged)
    Q_PROPERTY(ProjectionType projectionType READ projectionType WRITE setProjectionType NOTIFY projectionTypeChanged)

    //For animating between projection matrixes
    Q_PROPERTY(QMatrix4x4 orthProjectionMatrix READ orthProjectionMatrix NOTIFY orthProjectionMatrixChanged)
    Q_PROPERTY(QMatrix4x4 perspectiveProjectionMatrix READ perspectiveProjectionMatrix NOTIFY perspectiveProjectionMartixChanged)

public:
    enum ProjectionType {
        Perspective,
        Orthoginal,
        Custom
    };

    Q_ENUM(ProjectionType)

    explicit cwCamera(Qt3DCore::QNode *parent = 0);

    double zoomScale() const;
    void setZoomScale(double zoomScale);

    double fieldOfView() const;
    void setFieldOfView(double fieldOfView);

    void setViewport(QRect viewport);
    QRect viewport() const;

    Q_INVOKABLE void setCustomProjection(QMatrix4x4 matrix);
    void setProjection(cwProjection projection);
    cwProjection projection() const;
    QMatrix4x4 projectionMatrix() const;

    ProjectionType projectionType() const;
    void setProjectionType(ProjectionType projectionType);

    void setViewMatrix(QMatrix4x4 matrix);
    QMatrix4x4 viewMatrix() const;

    QMatrix4x4 viewProjectionMatrix() const;

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
    cwProjection orthoProjection() const;
    cwProjection perspectiveProjection() const;
    QMatrix4x4 orthProjectionMatrix() const;
    QMatrix4x4 perspectiveProjectionMatrix() const;

signals:
    void viewportChanged();
    void projectionChanged();
    void viewMatrixChanged();
    void pixelsPerMeterChanged();
    void zoomScaleChanged();
    void projectionTypeChanged();
    void orthProjectionMatrixChanged();
    void perspectiveProjectionMartixChanged();
    void fieldOfViewChanged();

public slots:

private:
    QRect Viewport;
    QMatrix4x4 ViewMatrix;
    mutable QMatrix4x4 ViewProjectionMatrix;
    cwProjection Projection;

    //For ortho projections
    double ZoomScale; //!<

    //For prespective projections
    double FieldOfView = 55.0; //In degrees

    mutable bool ViewProjectionMatrixIsDirty;

    Qt3DCore::QTransform* Transform;
    Qt3DRender::QCameraLens* CameraLens;

    void updateProjection(ProjectionType type);

};

Q_DECLARE_METATYPE(cwCamera*)



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
 * @brief cwPerspectiveProjection::fieldOfView
 * @return The field of view in degrees
 */
inline double cwCamera::fieldOfView() const {
    return FieldOfView;
}

/**
 * @brief cwCamera::orthProjectionMatrix
 *
 * Returns the matrix version of othrProjcetion()
 */
inline QMatrix4x4 cwCamera::orthProjectionMatrix() const
{
    return orthoProjection().matrix();
}

/**
* @brief class::perspectiveProjection
*
* Returns the matrix version of perspectiveProjection()
*/
inline QMatrix4x4 cwCamera::perspectiveProjectionMatrix() const {
    return perspectiveProjection().matrix();
}

#endif // CWCAMERA_H
