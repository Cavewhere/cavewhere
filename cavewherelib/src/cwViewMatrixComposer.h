#ifndef CWVIEWMATRIXCOMPOSER_H
#define CWVIEWMATRIXCOMPOSER_H

//Qt includes
#include <QObject>
#include <QMatrix4x4>
#include <QQmlEngine>

//Our includes
#include "cwGlobals.h"
class cwCamera;

class CAVEWHERE_LIB_EXPORT cwViewMatrixComposer : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ViewMatrixComposer)

    Q_PROPERTY(cwCamera* camera READ camera WRITE setCamera NOTIFY cameraChanged)

public:
    explicit cwViewMatrixComposer(QObject* parent = nullptr);

    cwCamera* camera() const;
    void setCamera(cwCamera* camera);

    void setBaseViewMatrix(const QMatrix4x4& matrix);
    QMatrix4x4 baseViewMatrix() const;

    void setHeadOffset(const QMatrix4x4& offset);

signals:
    void cameraChanged();

private:
    void compose();

    cwCamera* m_camera = nullptr;
    QMatrix4x4 m_baseViewMatrix;
    QMatrix4x4 m_headOffset; // identity when head tracking is off
};

inline cwCamera* cwViewMatrixComposer::camera() const
{
    return m_camera;
}

inline QMatrix4x4 cwViewMatrixComposer::baseViewMatrix() const
{
    return m_baseViewMatrix;
}

#endif // CWVIEWMATRIXCOMPOSER_H
