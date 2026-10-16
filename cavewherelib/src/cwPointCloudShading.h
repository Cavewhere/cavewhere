/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPOINTCLOUDSHADING_H
#define CWPOINTCLOUDSHADING_H

//Qt includes
#include <QtGlobal>

//Std includes
#include <array>

/**
 * The shader-side point cloud knobs: which thinning and sizing rule
 * PointCloud.vert applies. cwRenderingSettings persists them, the scene node
 * hands them to every cwRenderPointCloud, and the per-cloud uniform carries
 * them to the shader. The defaults are the renderer's own rules, so a view at
 * the defaults draws exactly what it drew before the knobs existed.
 */
namespace cw::pointcloud {

    //! How a node's points thin as the target spacing widens past the node's own
    enum class ThinningMode : int {
        //! The weight ramps from 1 to 0 over the octave (cw::clod::weight)
        Continuous = 0,
        //! The weight steps from 1 to 0 the moment the ramp leaves 1, so a
        //! level leaves all at once
        Discrete = 1,
        //! Every resident point draws, whatever the target spacing
        Off = 2
    };

    //! What a sprite's side is measured from
    enum class SizeMode : int {
        //! spacingCoverage of the wider of the node's floor and the threshold
        Coverage = 0,
        //! fixedPointSizePx, whatever the spacing
        FixedPixels = 1,
        //! Coverage of the node's own sample spacing in place of its floor
        OwnSpacing = 2
    };

    //! Whether the per-node floor takes part in sizing
    enum class FloorMode : int {
        //! The world-space term floors the threshold term. Each node's floor
        //! is the finest spacing drawn under it, except that a node whose
        //! children are still streaming keeps its own spacing until they land
        //! (cw::octree::finestDrawnLevels).
        Node = 0,
        //! The threshold term alone
        Off = 1
    };

    //! Which spacing a node sizes and thins by. The cut always refines on the
    //! grid spacing.
    enum class SpacingSource : int {
        //! The node's grid spacing, its cube over kSampleGridResolution
        Grid = 0,
        //! The larger of the grid spacing and the spacing the builder measured
        //! from the node's own points (cwPointOctreeNode::measuredSpacing), so
        //! a node sparser than its grid sizes and thins by the gap it really has
        Measured = 1,
        //! Measured for how a node thins and floors, while each sprite covers
        //! at least the native spacing the builder measured around its own
        //! point (cw::octree::QuantizedPoint::reserved, cw::clod::localSpacing,
        //! cw::clod::sizeSpacing) (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html
        //! section 14.5, WP13)
        Local = 2
    };

    //! What an arrival does while the reveal ramp (revealFrames) runs, and
    //! what a drawn node's floor is taken from
    //! (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html section 14.4, WP12)
    enum class RevealMode : int {
        //! An arrival fades its weight in, and an ancestor whose children are
        //! still streaming keeps its own spacing until they land, so the
        //! surface stays closed and the ancestor's sprites shrink once they do
        Cover = 0,
        //! Every drawn node floors at the finest spacing of the cut the forest
        //! asked for under it, drawn or still streaming, so sprites keep their
        //! target size and the missing children show as holes; an arrival
        //! draws all its kept points at once and grows each sprite from one
        //! pixel to its size
        Grow = 1
    };

    //! What PointCloud.vert colors each point by. Every view but Off answers
    //! one question about the octree or the decimation
    //! (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html section 8).
    enum class DebugView : int {
        //! Flat white, the renderer as it ships
        Off = 0,
        //! kLevelPalette by the node's level
        Level = 1,
        //! A hash of the node's index, so node boundaries show
        Node = 2,
        //! A gray ramp over the point's rank, the dither order
        Rank = 3,
        //! Red at weight 0 to green at weight 1
        Weight = 4,
        //! A ramp over the sprite side the sizing rule chose
        SpriteSize = 5,
        //! A ramp over the node's own spacing against its floor
        FloorRatio = 6,
        //! White the frame a node arrives, fading to gray
        ResidencyAge = 7,
        //! Kept points as Off draws them, thinned ones dim at one pixel
        DecimationPreview = 8,
        //! A diverging ramp over how much of the point's own native spacing
        //! its sprite covers on screen, white where it covers exactly
        //! spacingCoverage of it
        SpriteDensity = 9
    };

    constexpr int kThinningModeCount = int(ThinningMode::Off) + 1;
    constexpr int kSizeModeCount = int(SizeMode::OwnSpacing) + 1;
    constexpr int kFloorModeCount = int(FloorMode::Off) + 1;
    constexpr int kSpacingSourceCount = int(SpacingSource::Local) + 1;
    constexpr int kDebugViewCount = int(DebugView::SpriteDensity) + 1;
    constexpr int kRevealModeCount = int(RevealMode::Grow) + 1;

    constexpr bool kDefaultShowNodeBoxes = false;

    //! Measured, because a node sparser than its grid otherwise draws sprites
    //! sized for points it does not have, and deep levels lit less of the
    //! ground (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html WP10)
    constexpr SpacingSource kDefaultSpacingSource = SpacingSource::Measured;

    //! One color per octree level, cycling past the last. The first six are the
    //! level colors of plans/POINT_CLOUD_CLOD_FIGURES.html. PointCloud.vert
    //! spells the same table out for the level view, and the node boxes read it
    //! here, so a box and the points inside it agree.
    struct Rgb {
        float red;
        float green;
        float blue;
    };
    constexpr std::array<Rgb, 8> kLevelPalette = {{
        {0x6f / 255.0f, 0x8f / 255.0f, 0xf0 / 255.0f},
        {0x43 / 255.0f, 0xbd / 255.0f, 0xe0 / 255.0f},
        {0x4e / 255.0f, 0xc9 / 255.0f, 0x8a / 255.0f},
        {0xbc / 255.0f, 0xd2 / 255.0f, 0x3c / 255.0f},
        {0xf2 / 255.0f, 0xb7 / 255.0f, 0x3a / 255.0f},
        {0xf3 / 255.0f, 0x80 / 255.0f, 0x4f / 255.0f},
        {0xb0 / 255.0f, 0x7c / 255.0f, 0xf0 / 255.0f},
        {0xe0 / 255.0f, 0x70 / 255.0f, 0xc0 / 255.0f}
    }};

    //! A node box's color while its node is resident outside the cut, which is
    //! what eviction takes from
    constexpr Rgb kEvictableNodeBoxColor {1.0f, 0.15f, 0.15f};

    //! The sprite density view's ramp ends: a sprite covering
    //! kDensityViewOctaves octaves less of its native spacing than
    //! spacingCoverage asks for reads kUnderCoveredColor (holes), one covering
    //! that many octaves more reads kOverCoveredColor, and one covering exactly
    //! spacingCoverage reads white. PointCloud.vert spells all three out.
    constexpr double kDensityViewOctaves = 2.0;
    constexpr Rgb kUnderCoveredColor {0.85f, 0.2f, 0.15f};
    constexpr Rgb kOverCoveredColor {0.2f, 0.4f, 0.95f};

    constexpr Rgb levelColor(int level)
    {
        const int count = int(kLevelPalette.size());
        return kLevelPalette[std::size_t(((level % count) + count) % count)];
    }

    //! The frames an arriving node fades in over, and that a node's floor
    //! takes to move from its previous value to its new one, in log space
    //! (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html WP11). 0 shows an arrival
    //! whole and moves a floor on the frame it changes.
    constexpr int kDefaultRevealFrames = 0;
    constexpr int kMinRevealFrames = 0;
    constexpr int kMaxRevealFrames = 60;

    //! Cover, the rule WP11 shipped with
    constexpr RevealMode kDefaultRevealMode = RevealMode::Cover;

    constexpr double kMinPointSizePx = 1.0;
    constexpr double kMaxPointSizePx = 64.0;
    constexpr double kDefaultMaxPointSizePx = kMaxPointSizePx;
    constexpr double kDefaultFixedPointSizePx = 3.0;

    //PointCloud.vert spells the modes out as integers
    static_assert(int(ThinningMode::Continuous) == 0 && int(ThinningMode::Discrete) == 1
                      && int(ThinningMode::Off) == 2,
                  "PointCloud.vert spells this out");
    static_assert(int(SizeMode::Coverage) == 0 && int(SizeMode::FixedPixels) == 1
                      && int(SizeMode::OwnSpacing) == 2,
                  "PointCloud.vert spells this out");
    static_assert(int(FloorMode::Node) == 0 && int(FloorMode::Off) == 1,
                  "PointCloud.vert spells this out");
    static_assert(int(SpacingSource::Grid) == 0 && int(SpacingSource::Measured) == 1
                      && int(SpacingSource::Local) == 2,
                  "PointCloud.vert spells this out");
    static_assert(int(RevealMode::Cover) == 0 && int(RevealMode::Grow) == 1,
                  "PointCloud.vert spells this out");
    static_assert(int(DebugView::Off) == 0 && int(DebugView::Level) == 1
                      && int(DebugView::Node) == 2 && int(DebugView::Rank) == 3
                      && int(DebugView::Weight) == 4 && int(DebugView::SpriteSize) == 5
                      && int(DebugView::FloorRatio) == 6 && int(DebugView::ResidencyAge) == 7
                      && int(DebugView::DecimationPreview) == 8
                      && int(DebugView::SpriteDensity) == 9,
                  "PointCloud.vert spells this out");
}

/**
 * One cloud's shader-side knobs, as the GUI thread hands them to the render
 * thread. showNodeBoxes is the one that never reaches the per-cloud uniform: it
 * switches the cloud's node box overlay on.
 */
struct cwPointCloudShading {
    cw::pointcloud::ThinningMode thinningMode = cw::pointcloud::ThinningMode::Continuous;
    cw::pointcloud::SizeMode sizeMode = cw::pointcloud::SizeMode::Coverage;
    cw::pointcloud::FloorMode floorMode = cw::pointcloud::FloorMode::Node;
    cw::pointcloud::SpacingSource spacingSource = cw::pointcloud::kDefaultSpacingSource;
    float fixedPointSizePx = float(cw::pointcloud::kDefaultFixedPointSizePx);
    float maxPointSizePx = float(cw::pointcloud::kDefaultMaxPointSizePx);
    cw::pointcloud::DebugView debugView = cw::pointcloud::DebugView::Off;
    int revealFrames = cw::pointcloud::kDefaultRevealFrames;
    cw::pointcloud::RevealMode revealMode = cw::pointcloud::kDefaultRevealMode;
    bool showNodeBoxes = cw::pointcloud::kDefaultShowNodeBoxes;

    bool operator==(const cwPointCloudShading& other) const = default;
};

#endif // CWPOINTCLOUDSHADING_H
