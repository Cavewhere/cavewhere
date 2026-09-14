/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRENDERPOINTCLOUD_H
#define CWRENDERPOINTCLOUD_H

// Our includes
#include "cwPointOctreePickSet.h"
#include "cwPointOctreeSource.h"
#include "cwRenderObject.h"
#include "cwTracked.h"

// Qt includes
#include <QHash>
#include <QQmlEngine>
#include <QVector3D>

// Std includes
#include <memory>

namespace cw::pointcloud {
    //! World-space sprite radius in meters, the floor every sprite draws at.
    //!
    //! The shader scales by the physical viewport height alone, so this is a
    //! true world radius on every display density. It reads as twice the old
    //! default because the old one was doubled again by devicePixelRatio.
    constexpr float kDefaultWorldRadius = 2.58f;

    //! Sprite radius as a fraction of the drawn node's sample spacing.
    //!
    //! Grid sampling leaves at most one point per occupied cell of side
    //! `spacing`, and gl_PointSize is a side length, so 0.75 narrows the gap a
    //! coarse cut shows to a quarter of a spacing while overdrawing modestly
    //! where a parent sits in a refined region.
    constexpr float kDefaultSpacingCoverage = 0.75f;
}

class cwRenderPointCloud : public cwRenderObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RenderPointCloud)

    friend class cwRHIPointCloud;
    friend struct CwRenderPointCloudTestAccess;

public:
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

    // The octree the renderer streams nodes out of. Re-publishing an equal
    // source is a no-op, so the render side keeps every resident node.
    void setOctree(const cwPointOctreeSource& source);

    void clear();

    const cwPointOctreeSource& octree() const;
    qint64 pointCount() const;
    QVector3D bboxMin() const;
    QVector3D bboxMax() const;
    float meanSpacingXY() const;
    float worldRadius() const;
    void setWorldRadius(float worldRadius);
    float spacingCoverage() const;
    void setSpacingCoverage(float spacingCoverage);

protected:
    cwRHIObject* createRHIObject() override;

private:
    // Cheap per-cloud knobs uploaded as a small uniform, never as vertex
    // data. A change here re-uploads the UBO but leaves the node buffers
    // untouched. Real field compare so a no-op set is a no-op.
    struct RenderState {
        // World-space sprite radius in meters. A fixed default produces
        // consistent sprite sizes across clouds; tuned at runtime by P+wheel
        // in the 3D view (clamped on the scene-node) and by sink_repatcher
        // --point-radius for offline renders. This is appearance slot 0 — the
        // live view and a plain capture both render with it.
        float worldRadius = cw::pointcloud::kDefaultWorldRadius;

        // Sprite radius as a fraction of the drawn node's sample spacing. The
        // shader takes the larger of this and worldRadius, so it only adds
        // size where the cut is coarse.
        float spacingCoverage = cw::pointcloud::kDefaultSpacingCoverage;

        bool operator!=(const RenderState& other) const {
            return worldRadius != other.worldRadius
                || spacingCoverage != other.spacingCoverage;
        }
    };

    cwTracked<cwPointOctreeSource> m_source;
    cwTracked<RenderState> m_renderState;

    // What the cloud is picked against: the nodes the render thread has
    // resident. Shared with the cwRHIPointCloud that publishes into it, so a
    // publish after this object is gone lands on a set nothing reads.
    std::shared_ptr<cwPointOctreePickSet> m_pickSet =
        std::make_shared<cwPointOctreePickSet>();
};

inline const cwPointOctreeSource& cwRenderPointCloud::octree() const
{
    return m_source.value();
}

inline float cwRenderPointCloud::worldRadius() const
{
    return m_renderState.value().worldRadius;
}

inline float cwRenderPointCloud::spacingCoverage() const
{
    return m_renderState.value().spacingCoverage;
}

#endif // CWRENDERPOINTCLOUD_H
