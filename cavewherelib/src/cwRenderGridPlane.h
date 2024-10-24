/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLPLANE_H
#define CWGLPLANE_H

//Our includes
#include "cwRenderObject.h"
#include "cwTracked.h"

//Qt includes
#include <QPlane3D>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>


class cwRenderGridPlane : public cwRenderObject {
    Q_OBJECT

    Q_PROPERTY(double extent READ extent WRITE setExtent NOTIFY extentChanged)
    friend class cwRHIGridPlane;

public:
    enum class UpdateFlag {
        None = 0,
        ModelMatrix = 0x1,
    };

    cwRenderGridPlane(QObject *parent = nullptr);

    QPlane3D plane() const;
    void setPlane(QPlane3D plane);

    double extent() const;
    void setExtent(double extent);

    // QMatrix4x4 modelMatrix() const { return m_modelMatrix.value(); }

protected:
    virtual cwRHIObject* createRHIObject() override;

private:
    QPlane3D m_plane;
    double m_extent; //!< The max rendering area of the plane
    cwTracked<QMatrix4x4> m_modelMatrix;

    //Camera connection
    QMetaObject::Connection m_sceneConnection;
    QMetaObject::Connection m_viewMatrixConnection;
    QMetaObject::Connection m_projectionConnection;

    void updateModelMatrix();

signals:
    void extentChanged();

};

/**
    Gets extent, the max rendering geometry of the plane
*/
inline double cwRenderGridPlane::extent() const {
    return m_extent;
}

/**
 * @brief cwRenderGridPlane::plane
 * @return The plane that this class is rendering
 */
inline QPlane3D cwRenderGridPlane::plane() const {
    return m_plane;
}

#endif // CWGLPLANE_H
