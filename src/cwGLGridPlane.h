/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLPLANE_H
#define CWGLPLANE_H

//Our includes
#include "cwGLObject.h"

//Qt includes
#include <QPlane3D>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>


class cwGLGridPlane : public cwGLObject {
    Q_OBJECT

    Q_PROPERTY(double extent READ extent WRITE setExtent NOTIFY extentChanged)


public:
    cwGLGridPlane(QObject *parent = nullptr);

    virtual void initialize();
    virtual void draw();

    QPlane3D plane() const;
    void setPlane(QPlane3D plane);

    double extent() const;
    void setExtent(double extent);

private:
    QPlane3D Plane;
    double Extent; //!< The max rendering area of the plane

    QOpenGLShaderProgram* Program;
    QOpenGLBuffer TriangleVertexBuffer;

    QMatrix4x4 ModelMatrix;

    //Shader attributes
    int vVertex;
    int UniformModelViewProjectionMatrix;
    int UniformModelMatrix;
    int UniformDevicePixelRatio;

    void initializeGeometry();
    void initializeShaders();
    void updateModelMatrix();

signals:
    void extentChanged();

};

/**
    Gets extent, the max rendering geometry of the plane
*/
inline double cwGLGridPlane::extent() const {
    return Extent;
}

/**
 * @brief cwGLGridPlane::plane
 * @return The plane that this class is rendering
 */
inline QPlane3D cwGLGridPlane::plane() const {
    return Plane;
}

#endif // CWGLPLANE_H
