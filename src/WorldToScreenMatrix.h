// WorldToScreenMatrix.h
#pragma once

//Qt includes
#include <QObject>
#include <QMatrix4x4>
#include <QPointF>
#include <QVector3D>
#include <QQmlEngine>
#include <QObjectBindableProperty>


//CaveWhere includes
#include "cwScale.h"


class WorldToScreenMatrix : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(cwScale* scale READ scale CONSTANT)
    Q_PROPERTY(double pixelDensity READ pixelDensity WRITE setPixelDensity NOTIFY pixelDensityChanged BINDABLE bindablePixelDensity)
    Q_PROPERTY(QMatrix4x4 matrix READ matrix NOTIFY matrixChanged);

public:
    explicit WorldToScreenMatrix(QObject* parent = nullptr);

    // Scale accessor
    cwScale* scale() const { return m_scale; }

    // Pixel density accessor
    double pixelDensity() const { return m_pixelDensity; }
    void setPixelDensity(double density) { m_pixelDensity = density; }
    QBindable<double> bindablePixelDensity() { return &m_pixelDensity; }

    // Matrix accessor
    QMatrix4x4 matrix() const { return m_matrix; }

signals:
    void scaleChanged();
    void pixelDensityChanged();
    void matrixChanged();

private:
    cwScale* m_scale;
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(WorldToScreenMatrix, double, m_pixelDensity, 96.0, &WorldToScreenMatrix::pixelDensityChanged);
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(WorldToScreenMatrix, QMatrix4x4, m_matrix, QMatrix4x4(), &WorldToScreenMatrix::matrixChanged);

    //private bindable
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(WorldToScreenMatrix, double, m_currentScale, 1.0);
};
