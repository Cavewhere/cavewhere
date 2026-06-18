/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERPOINTCLOUD_H
#define CWRENDERPOINTCLOUD_H

// Our includes
#include "cwGeometry.h"
#include "cwRenderObject.h"
#include "cwTracked.h"

// Qt includes
#include <QHash>
#include <QQmlEngine>
#include <QVector3D>

class cwRenderPointCloud : public cwRenderObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderPointCloud)

    friend class cwRHIPointCloud;
    friend struct CwRenderPointCloudTestAccess;

public:
    // Bundle of fields handed across the model→render boundary in one shot.
    // Mirrors cwLazLoadResult / cwLazLayer's load-time outputs that the
    // renderer consumes together.
    struct GeometryData {
        cwGeometry geometry;
        QVector3D bboxMin;
        QVector3D bboxMax;
        float meanSpacingXY = 0.0f;
    };

    explicit cwRenderPointCloud(QObject* parent = nullptr);

    void setGeometry(GeometryData data);

    void clear();

    qsizetype pointCount() const;
    QVector3D bboxMin() const;
    QVector3D bboxMax() const;
    float pointSize() const;
    void setPointSize(float pointSize);
    float meanSpacingXY() const;
    float worldRadius() const;
    void setWorldRadius(float worldRadius);

    // Per-appearance-slot world-radius override for offscreen render jobs
    // (cwOffscreenRenderParameters::appearanceSlot). Slot 0 is always the live
    // worldRadius() — only slots >= 1 are overridable. A slot with no override
    // renders at the live radius, so an unset slot changes nothing. An offscreen
    // render consumer sets a slot, then issues a job selecting it; the on-screen
    // view (slot 0) is unaffected.
    void setWorldRadiusOverride(int slot, float worldRadius);
    void clearWorldRadiusOverride(int slot);

protected:
    cwRHIObject* createRHIObject() override;

private:
    // Geometry and its derived bounds, tracked separately from the cheap
    // render knobs below. Re-staging the (potentially multi-GB) vertex
    // buffer is gated on THIS tracker changing, so tuning a uniform —
    // world radius, point size — never re-uploads geometry. setGeometry() and
    // clear() are the only callers that touch it.
    struct GeometryState {
        cwGeometry geometry;
        QVector3D bboxMin;
        QVector3D bboxMax;
        // Mean planar inter-point spacing in meters (sqrt(area / N)). Drives
        // per-cloud point radius in PointCloud.vert. 0 until first load.
        float meanSpacingXY = 0.0f;

        // Coarse "always changed": setGeometry runs per LAZ load (rare), so
        // a real compare would be a pointless multi-GB memcmp. cwTracked
        // re-stages only when this tracker is setValue'd, which is exactly
        // the geometry-changed edge.
        bool operator!=(const GeometryState& /*other*/) const { return true; }
    };

    // Cheap per-cloud knobs uploaded as a small uniform, never as vertex
    // data. A change here re-uploads the UBO but leaves the vertex buffer
    // untouched. Real field compare so a no-op set is a no-op.
    struct RenderState {
        float pointSize = 2.0f;

        // World-space sprite radius in meters. A fixed default produces
        // consistent sprite sizes across clouds; tuned at runtime by P+wheel
        // in the 3D view (clamped on the scene-node) and by sink_repatcher
        // --point-radius for offline renders. This is appearance slot 0 — the
        // live view and a plain capture both render with it.
        float worldRadius = 1.29f;

        // Per-appearance-slot world-radius overrides, keyed by slot index >= 1
        // (slot 0 is always the live worldRadius above). An offscreen job that
        // selects slot N renders the cloud at the radius stored here for N, or
        // the live radius when N has no entry. Sparse: only set slots appear.
        QHash<int, float> worldRadiusOverrides;

        bool operator!=(const RenderState& other) const {
            return pointSize != other.pointSize
                || worldRadius != other.worldRadius
                || worldRadiusOverrides != other.worldRadiusOverrides;
        }
    };

    cwTracked<GeometryState> m_geometry;
    cwTracked<RenderState> m_renderState;
};

inline qsizetype cwRenderPointCloud::pointCount() const
{
    return m_geometry.value().geometry.vertexCount();
}

inline QVector3D cwRenderPointCloud::bboxMin() const
{
    return m_geometry.value().bboxMin;
}

inline QVector3D cwRenderPointCloud::bboxMax() const
{
    return m_geometry.value().bboxMax;
}

inline float cwRenderPointCloud::pointSize() const
{
    return m_renderState.value().pointSize;
}

inline float cwRenderPointCloud::meanSpacingXY() const
{
    return m_geometry.value().meanSpacingXY;
}

inline float cwRenderPointCloud::worldRadius() const
{
    return m_renderState.value().worldRadius;
}

#endif // CWRENDERPOINTCLOUD_H
