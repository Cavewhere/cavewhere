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

    //! Pick sphere radius, as a multiple of meanSpacingXY.
    //!
    //! Must stay above 1/sqrt(2) ~= 0.707: on a grid of spacing s the worst
    //! case ray passes through a cell centre, s*sqrt(2)/2 from the nearest
    //! point, so anything smaller leaves a gap the ray slips through — and it
    //! then hits some unoccluded point far down the ray instead. The splats
    //! are drawn from worldRadius and overlap into a solid-looking wall, so
    //! that gap is invisible: the surface reads as watertight and picks
    //! through. Above the threshold the near wall yields an exact hit.
    //!
    //! Nothing catches a miss any more: the near-miss fallback that used to
    //! paper over sub-threshold radii is gone (it snapped picks to points the
    //! ray never touched, which made leads unclickable — see
    //! cwLeadView::isOccluded). This constant is the only thing keeping a
    //! cloud pickable where it is drawn.
    //!
    //! Deliberately NOT tied to worldRadius, which would match the drawn
    //! footprint exactly but is tuned live (P + mouse wheel, see
    //! PointCloud.vert) while this is baked into the BVH box padding at
    //! addObject time — tracking it would rebuild the whole BVH per wheel
    //! tick. Staying above the threshold keeps picks watertight at every
    //! worldRadius instead.
    //! 1.0 rather than the bare 0.707 threshold: meanSpacingXY is a mean over
    //! an irregular cloud, so local spacing runs above it, and a wall met at a
    //! grazing angle stretches the effective gap further still. The margin
    //! covers both.
    static constexpr float PointPickRadiusScale = 1.0f;

    // Binds the threshold where the constant is declared, so a retune below
    // 1/sqrt(2) fails the build rather than silently reopening the gaps.
    // 0.7071068f rather than M_SQRT1_2: that macro is not in the C++ standard
    // (MSVC hides it behind _USE_MATH_DEFINES, which this project never sets).
    static_assert(PointPickRadiusScale >= 0.7071068f,
                  "A pick sphere below sqrt(2)/2 of the point spacing cannot "
                  "cover a grid cell's centre, so some ray through a "
                  "solid-looking cloud will miss every point in it.");

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

        bool operator!=(const RenderState& other) const {
            return pointSize != other.pointSize
                || worldRadius != other.worldRadius;
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
