#ifndef CWNOTECAMERA_H
#define CWNOTECAMERA_H

//Qt includes
#include <QQmlEngine>

//Our includes
#include "cwCamera.h"
#include "cwImage.h"

class cwNoteCamera : public cwCamera
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NoteCamera)

    Q_PROPERTY(double rotation READ rotation WRITE setRotation NOTIFY rotationChanged)

public:
    explicit cwNoteCamera(QObject *parent = nullptr);

    void setRotation(double degrees);
    double rotation() const;

    QPointF mapQtViewportToNote(QPoint qtViewportCoordinate) const;
    QPointF mapNoteToQtViewport(QPointF mapNote) const;

    Q_INVOKABLE void updateRotationCenter();


signals:
    void rotationChanged();
    void modelMatrixChanged();


private:
    double m_rotation;
    QPointF m_rotationCenter;
    QMatrix4x4 m_rotationModelMatrix;
    cwImage m_image;

    void resize(const QSize& windowSize);
};


/**
  Gets the rotation of the image in degress.
  */
inline double cwNoteCamera::rotation() const {
    return m_rotation;
}

#endif // CWNOTECAMERA_H
