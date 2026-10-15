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
        DecimationPreview = 8
    };

    constexpr int kThinningModeCount = int(ThinningMode::Off) + 1;
    constexpr int kSizeModeCount = int(SizeMode::OwnSpacing) + 1;
    constexpr int kFloorModeCount = int(FloorMode::Off) + 1;
    constexpr int kDebugViewCount = int(DebugView::DecimationPreview) + 1;

    constexpr bool kDefaultShowNodeBoxes = false;

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

    constexpr Rgb levelColor(int level)
    {
        const int count = int(kLevelPalette.size());
        return kLevelPalette[std::size_t(((level % count) + count) % count)];
    }

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
    static_assert(int(DebugView::Off) == 0 && int(DebugView::Level) == 1
                      && int(DebugView::Node) == 2 && int(DebugView::Rank) == 3
                      && int(DebugView::Weight) == 4 && int(DebugView::SpriteSize) == 5
                      && int(DebugView::FloorRatio) == 6 && int(DebugView::ResidencyAge) == 7
                      && int(DebugView::DecimationPreview) == 8,
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
    float fixedPointSizePx = float(cw::pointcloud::kDefaultFixedPointSizePx);
    float maxPointSizePx = float(cw::pointcloud::kDefaultMaxPointSizePx);
    cw::pointcloud::DebugView debugView = cw::pointcloud::DebugView::Off;
    bool showNodeBoxes = cw::pointcloud::kDefaultShowNodeBoxes;

    bool operator==(const cwPointCloudShading& other) const = default;
};

#endif // CWPOINTCLOUDSHADING_H
