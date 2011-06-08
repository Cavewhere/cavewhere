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

    QVector3D unProject(QPoint point, float viewDepth, QMatrix4x4 modelMatrix = QMatrix4x4()) const;

    QPoint mapToGLViewport(QPoint point) const;

signals:

public slots:

private:
    QRect Viewport;
    QMatrix4x4 ProjectionMatrix;
    QMatrix4x4 ViewMatrix;

};

/**
  Sets the viewport for the camera
  */
inline void cwCamera::setViewport(QRect viewport) {
    Viewport = viewport;
}

/**
  Gets the viewport for the camera
  */
inline QRect cwCamera::viewport() const {
    return Viewport;
}

/**
  Sets the projection matrix for the camera
  */
inline void cwCamera::setProjectionMatrix(QMatrix4x4 matrix) {
    ProjectionMatrix = matrix;
}

/**
  Gets the projection matrix for the camera
  */
inline QMatrix4x4 cwCamera::projectionMatrix() const {
    return ProjectionMatrix;
}

/**
  Sets the view matrix for the camera
  */
inline void cwCamera::setViewMatrix(QMatrix4x4 matrix) {
    ViewMatrix = matrix;
}

/**
  Gets the view matrix for the camera
  */
inline QMatrix4x4 cwCamera::viewMatrix() const {
    return ViewMatrix;
}

#endif // CWCAMERA_H
