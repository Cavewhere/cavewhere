#ifndef CWCAMERA_H
#define CWCAMERA_H

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

    void setProjectionMatrix(QMatrix4x4 matrix);
    QMatrix4x4 projectionMatrix() const;

    void setViewMatrix(QMatrix4x4 matrix);
    QMatrix4x4 viewMatrix() const;

    QMatrix4x4 viewProjectionMatrix();

    QVector3D unProject(QPoint point, float viewDepth) const;
    QVector3D unProject(QPoint point, float viewDepth, QMatrix4x4 modelMatrix) const;
    QVector3D unProject(QPoint point, float viewDepth, QMatrix4x4 viewMatrix, QMatrix4x4 modelMatrix) const;

//    QVector3D project(QVector3D )

    static QVector3D mapNormalizeScreenToGLViewport(const QVector3D& point, const QRect& viewport);
    QVector3D mapNormalizeScreenToGLViewport(const QVector3D& point) const;
    QPoint mapToGLViewport(QPoint point) const;

signals:
    void viewportChanged();
    void projectionChanged();
    void viewChanged();

public slots:

private:
    QRect Viewport;
    QMatrix4x4 ProjectionMatrix;
    QMatrix4x4 ViewMatrix;
    QMatrix4x4 ViewProjectionMatrix;
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
inline QMatrix4x4 cwCamera::projectionMatrix() const {
    return ProjectionMatrix;
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

/**
  Convenience method

  Same as calling unProject(point, viewDepth, camera->viewMatrix(), modelMatrix)
  */
inline QVector3D cwCamera::unProject(QPoint point, float viewDepth, QMatrix4x4 modelMatrix) const {
    return unProject(point, viewDepth, viewMatrix(), modelMatrix);
}

#endif // CWCAMERA_H
