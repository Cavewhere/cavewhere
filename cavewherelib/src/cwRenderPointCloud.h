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
    //! Sprite side as a fraction of the sample spacing the cut refines to.
    //!
    //! Grid sampling leaves at most one point per occupied cell of side
    //! `spacing`, and gl_PointSize is a side length, so 1.5 covers a cell and
    //! half of each neighbor's. PointCloud.vert works the spacing out per
    //! vertex from the vertex's own clip depth and the view's refine
    //! threshold, so a far tile drawn at a coarse level covers its own
    //! cell just as a near tile covers its finer one. The cut is additive — a
    //! refined region draws its coarse ancestors too — and depth is what tells
    //! those ancestors apart: an ancestor point sitting among refined ones is
    //! at the same small depth they are, so it gets their small spacing rather
    //! than its node's own coarse one. Where the point budget or a node still
    //! streaming holds the cloud coarser than the threshold describes, the
    //! coverage applies to the finest spacing actually drawn instead.
    //!
    //! The density under a sprite is continuous: each node's points thin
    //! against the same threshold (cw::clod), fading out over the octave that
    //! leads to their level leaving the cut, so the points a sprite of this
    //! coverage has to cover sit about one target spacing apart at every
    //! scale.
    //!
    //! This is the only point-size knob: coverage is what predicts whether the
    //! surface reads solid or shows holes, so the shader has no separate tuned
    //! world radius competing with it. Above 1 the sprites of neighboring
    //! cells overlap, which is what closes the holes an irregular cloud leaves
    //! at exactly one cell per sprite.
    constexpr float kDefaultSpacingCoverage = 1.5f;
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
    //! are drawn from the spacing rule and overlap into a solid-looking wall,
    //! so that gap is invisible: the surface reads as watertight and picks
    //! through. Above the threshold the near wall yields an exact hit.
    //!
    //! Nothing catches a miss any more: the near-miss fallback that used to
    //! paper over sub-threshold radii is gone (it snapped picks to points the
    //! ray never touched, which made leads unclickable — see
    //! cwLeadView::isOccluded). This constant is the only thing keeping a
    //! cloud pickable where it is drawn.
    //!
    //! Deliberately NOT tied to spacingCoverage, which describes the drawn
    //! footprint but is tuned live (P + mouse wheel, see
    //! PointCloud.vert) while this is baked into the BVH box padding at
    //! addObject time — tracking it would rebuild the whole BVH per wheel
    //! tick. Staying above the threshold keeps picks watertight at every
    //! coverage instead.
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
    float spacingCoverage() const;
    void setSpacingCoverage(float spacingCoverage);

protected:
    cwRHIObject* createRHIObject() override;

private:
    // Cheap per-cloud knobs uploaded as a small uniform, never as vertex
    // data. A change here re-uploads the UBO but leaves the node buffers
    // untouched. Real field compare so a no-op set is a no-op.
    struct RenderState {
        // Sprite side as a fraction of the sample spacing the cut refines to
        // at each vertex's depth — the whole sizing rule, and the only
        // point-size knob. Tuned at runtime by P+wheel in the 3D view (clamped
        // on the scene-node). This is appearance slot 0 — the live view and a
        // plain capture both render with it.
        float spacingCoverage = cw::pointcloud::kDefaultSpacingCoverage;

        bool operator!=(const RenderState& other) const {
            return spacingCoverage != other.spacingCoverage;
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

inline float cwRenderPointCloud::spacingCoverage() const
{
    return m_renderState.value().spacingCoverage;
}

#endif // CWRENDERPOINTCLOUD_H
