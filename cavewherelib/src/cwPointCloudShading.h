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
        //! The world-space term floors the threshold term
        Node = 0,
        //! The threshold term alone
        Off = 1
    };

    constexpr int kThinningModeCount = int(ThinningMode::Off) + 1;
    constexpr int kSizeModeCount = int(SizeMode::OwnSpacing) + 1;
    constexpr int kFloorModeCount = int(FloorMode::Off) + 1;

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
}

/**
 * One cloud's shader-side knobs, as the GUI thread hands them to the render
 * thread.
 */
struct cwPointCloudShading {
    cw::pointcloud::ThinningMode thinningMode = cw::pointcloud::ThinningMode::Continuous;
    cw::pointcloud::SizeMode sizeMode = cw::pointcloud::SizeMode::Coverage;
    cw::pointcloud::FloorMode floorMode = cw::pointcloud::FloorMode::Node;
    float fixedPointSizePx = float(cw::pointcloud::kDefaultFixedPointSizePx);
    float maxPointSizePx = float(cw::pointcloud::kDefaultMaxPointSizePx);

    bool operator==(const cwPointCloudShading& other) const = default;
};

#endif // CWPOINTCLOUDSHADING_H
