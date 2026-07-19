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

class cwRayHit {
    Q_GADGET
    QML_VALUE_TYPE(cwRayHit)

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

public:
    //! A hit location in one coordinate space: the point, the surface normal
    //! there, and the ray-depth (projectedDistance) at which the ray reaches
    //! it. A cwRayHit is built from one Sample in model space and one in
    //! world space, so the factories below take these instead of six loose
    //! vectors.
    struct Sample {
        QVector3D point;
        QVector3D normal;
        double t = std::numeric_limits<double>::quiet_NaN();
    };

    // A miss: hit() is false and every field holds its unset sentinel.
    cwRayHit() = default;

    // Named factories are the only writers of the fields below; each encodes
    // one primitive kind's hit contract, so a hit is either fully built or not
    // built at all. cwRayHit owns its own invariants — no caller reaches in.

    //! Model-space triangle hit from the Möller–Trumbore solve. The
    //! world-space and object fields stay unset until completedToWorld()
    //! fills them; hit() is already true. A miss is a default cwRayHit.
    static cwRayHit triangleModelHit(const Sample& model, float u, float v);

    //! This model-space triangle hit with its world-space and object fields
    //! filled in. The exact triangle pick calls it after mapping the model
    //! hit to world space; the model fields (u/v/pointModel/normalModel/
    //! tModel) carry through unchanged.
    cwRayHit completedToWorld(cwRenderObject* object, qulonglong objectId,
                              int firstIndex, const Sample& world) const;

    //! A hit on a point or a line segment — a primitive with no parametric
    //! surface, so u/v stay NaN and both normals face the camera (the caller
    //! passes the reversed ray direction). model/world are the snapped
    //! location and its depths in each space. Built in one call.
    static cwRayHit pointLikeHit(cwRenderObject* object, qulonglong objectId,
                                 int firstIndex,
                                 const Sample& model, const Sample& world);

    // Read accessors only
    bool hit() const { return m_hit; }

    //! The hit's model-space {point, normal, t} as a whole; the flat
    //! accessors below read its fields.
    const Sample& modelSample() const { return m_model; }
    //! The hit's world-space {point, normal, t} as a whole.
    const Sample& worldSample() const { return m_world; }

    float tModel() const { return float(m_model.t); }
    float u() const { return m_u; }
    float v() const { return m_v; }
    const QVector3D& pointModel() const { return m_model.point; }
    const QVector3D& normalModel() const { return m_model.normal; }

    double tWorld() const { return m_world.t; }
    const QVector3D& pointWorld() const { return m_world.point; }
    const QVector3D& normalWorld() const { return m_world.normal; }

    cwRenderObject* object() const { return m_object; }
    qulonglong objectId() const { return m_objectId; }
    int firstIndex() const { return m_firstIndex; }

private:
    // Written only by the factories above; everyone else reads via the
    // public getters. u/v are the triangle's parametric surface coordinates —
    // a model-space extra with no world-space or point/line analogue, so they
    // sit beside m_model rather than inside the Sample.
    bool m_hit = false;

    Sample m_model;
    float m_u = std::numeric_limits<float>::quiet_NaN();
    float m_v = std::numeric_limits<float>::quiet_NaN();

    Sample m_world;

    QPointer<cwRenderObject> m_object = nullptr;
    qulonglong m_objectId = 0;
    int m_firstIndex = -1;
};

inline cwRayHit cwRayHit::triangleModelHit(const Sample& model, float u, float v)
{
    cwRayHit hit;
    hit.m_hit = true;
    hit.m_model = model;
    hit.m_u = u;
    hit.m_v = v;
    return hit;
}

inline cwRayHit cwRayHit::completedToWorld(cwRenderObject* object, qulonglong objectId,
                                           int firstIndex, const Sample& world) const
{
    cwRayHit hit = *this;
    hit.m_world = world;
    hit.m_object = object;
    hit.m_objectId = objectId;
    hit.m_firstIndex = firstIndex;
    return hit;
}

inline cwRayHit cwRayHit::pointLikeHit(cwRenderObject* object, qulonglong objectId,
                                       int firstIndex,
                                       const Sample& model, const Sample& world)
{
    // u/v stay NaN — a point or a line has no parametric surface coordinate.
    cwRayHit hit;
    hit.m_hit = true;
    hit.m_model = model;
    hit.m_world = world;
    hit.m_object = object;
    hit.m_objectId = objectId;
    hit.m_firstIndex = firstIndex;
    return hit;
}

Q_DECLARE_METATYPE(cwRayHit)
