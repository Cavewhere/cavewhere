#pragma once

// Std
#include <limits>

// Qt
#include <QVector3D>
#include <QPointer>
#include <QtGlobal>
#include <QMetaType>
#include <QQmlEngine>

//Our include
#include "cwRenderObject.h"

class cwRayTriangleHit {
    Q_GADGET
    QML_VALUE_TYPE(cwRayTriangleHit)

    // Model-space
    Q_PROPERTY(bool hit READ hit CONSTANT)
    Q_PROPERTY(float tModel READ tModel CONSTANT)
    Q_PROPERTY(float u READ u CONSTANT)
    Q_PROPERTY(float v READ v CONSTANT)
    Q_PROPERTY(QVector3D pointModel READ pointModel CONSTANT)
    Q_PROPERTY(QVector3D normalModel READ normalModel CONSTANT)

    // World-space
    Q_PROPERTY(double tWorld READ tWorld CONSTANT)
    Q_PROPERTY(QVector3D pointWorld READ pointWorld CONSTANT)
    Q_PROPERTY(QVector3D normalWorld READ normalWorld CONSTANT)

    // Object info
    Q_PROPERTY(cwRenderObject* object READ object CONSTANT)
    Q_PROPERTY(qulonglong objectId READ objectId CONSTANT)
    Q_PROPERTY(int firstIndex READ firstIndex CONSTANT)

    friend class cwGeometryItersecter;

public:
    // Constructors
    cwRayTriangleHit() = default;

    // Read accessors only
    bool hit() const { return m_hit; }

    float tModel() const { return m_tModel; }
    float u() const { return m_u; }
    float v() const { return m_v; }
    const QVector3D& pointModel() const { return m_pointModel; }
    const QVector3D& normalModel() const { return m_normalModel; }

    double tWorld() const { return m_tWorld; }
    const QVector3D& pointWorld() const { return m_pointWorld; }
    const QVector3D& normalWorld() const { return m_normalWorld; }

    cwRenderObject* object() const { return m_object; }
    qulonglong objectId() const { return m_objectId; }
    int firstIndex() const { return m_firstIndex; }

    // Public fields for direct initialization
    bool m_hit = false;

    float m_tModel = std::numeric_limits<float>::quiet_NaN();
    float m_u = std::numeric_limits<float>::quiet_NaN();
    float m_v = std::numeric_limits<float>::quiet_NaN();
    QVector3D m_pointModel {};
    QVector3D m_normalModel {};

    double m_tWorld = std::numeric_limits<double>::quiet_NaN();
    QVector3D m_pointWorld {};
    QVector3D m_normalWorld {};

    QPointer<cwRenderObject> m_object = nullptr;
    qulonglong m_objectId = 0;
    int m_firstIndex = -1;
};

Q_DECLARE_METATYPE(cwRayTriangleHit)
