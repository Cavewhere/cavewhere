/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWWORLDTOSCREENMATRIX_H
#define CWWORLDTOSCREENMATRIX_H

//Qt includes
#include <QMatrix4x4>
#include <QObject>
#include <QObjectBindableProperty>
#include <QPointF>
#include <QQmlEngine>
#include <QVector3D>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwScale.h"

class CAVEWHERE_LIB_EXPORT cwWorldToScreenMatrix : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WorldToScreenMatrix)

    Q_PROPERTY(cwScale* scale READ scale CONSTANT)
    Q_PROPERTY(double pixelDensity READ pixelDensity WRITE setPixelDensity NOTIFY pixelDensityChanged BINDABLE bindablePixelDensity)
    Q_PROPERTY(QMatrix4x4 matrix READ matrix NOTIFY matrixChanged)

public:
    explicit cwWorldToScreenMatrix(QObject *parent = nullptr);

    cwScale *scale() const { return m_scale; }

    double pixelDensity() const { return m_pixelDensity; }
    void   setPixelDensity(double density) { m_pixelDensity = density; }
    QBindable<double> bindablePixelDensity() { return &m_pixelDensity; }

    QMatrix4x4 matrix() const { return m_matrix; }

signals:
    void scaleChanged();
    void pixelDensityChanged();
    void matrixChanged();

private:
    cwScale *m_scale;
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwWorldToScreenMatrix, double, m_pixelDensity, 96.0, &cwWorldToScreenMatrix::pixelDensityChanged);
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwWorldToScreenMatrix, QMatrix4x4, m_matrix, QMatrix4x4(), &cwWorldToScreenMatrix::matrixChanged);

    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(cwWorldToScreenMatrix, double, m_currentScale, 1.0);
};

#endif // CWWORLDTOSCREENMATRIX_H
