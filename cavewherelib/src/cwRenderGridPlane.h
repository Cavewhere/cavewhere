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
#include <QMatrix4x4>
#include <QObjectBindableProperty>
#include <QPlane3D>

class cwRenderGridPlane : public cwRenderObject {
    Q_OBJECT

    Q_PROPERTY(double extent READ extent WRITE setExtent NOTIFY extentChanged BINDABLE bindableExtent)
    Q_PROPERTY(QPlane3D plane READ plane WRITE setPlane NOTIFY planeChanged BINDABLE bindablePlane)

    friend class cwRHIGridPlane;

public:
    enum class UpdateFlag {
        None = 0,
        ModelMatrix = 0x1,
    };

    cwRenderGridPlane(QObject *parent = nullptr);

    double extent() const { return m_extent.value(); }
    void setExtent(const double& extent) { m_extent = extent; }
    QBindable<double> bindableExtent() { return &m_extent; }

    QPlane3D plane() const { return m_plane.value(); }
    void setPlane(const QPlane3D& plane) { m_plane = plane; }
    QBindable<QPlane3D> bindablePlane() { return &m_plane; }

signals:
    void extentChanged();
    void planeChanged();

protected:
    virtual cwRHIObject* createRHIObject() override;

private:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(
        cwRenderGridPlane,
        QPlane3D, m_plane,
        QPlane3D(QVector3D(0.0, 0.0, -75.0), QVector3D(0.0, 0.0, 1.0)),
        &cwRenderGridPlane::planeChanged);
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwRenderGridPlane,
                                         double, m_extent,
                                         3000.0,
                                         &cwRenderGridPlane::extentChanged);

    QProperty<QMatrix4x4> m_modelMatrixProperty;
    QProperty<QMatrix4x4> m_scaleMatrixProperty;
    cwTracked<QMatrix4x4> m_modelMatrix;
    cwTracked<QMatrix4x4> m_scaleMatrix;
    QPropertyNotifier m_modelMatrixNotifier;
    QPropertyNotifier m_scaleMatrixNotifier;

    //Camera connection
    QMetaObject::Connection m_sceneConnection;
    QMetaObject::Connection m_viewMatrixConnection;
    QMetaObject::Connection m_projectionConnection;
};



#endif // CWGLPLANE_H
