/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPOINTCLOUDCLOD_H
#define CWPOINTCLOUDCLOD_H

//Std includes
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

//Qt includes
#include <QtGlobal>

//Our includes
#include "cwPointCloudShading.h"
#include "cwPointOctree.h"

/**
 * The continuous level of detail rule PointCloud.vert thins the drawn cut
 * with. The cut is additive and steps a whole level at a time, so without
 * thinning the drawn density quarters the moment a level leaves it. Here every
 * point carries a rank in [0, 1) that depends on its quantized position alone,
 * and a node's points draw while their rank is under the node's weight, which
 * falls from 1 to 0 over the octave between the node's own sample spacing and
 * twice it, so a level fades out over the scales that lead to its departure.
 *
 * Every function here is pure and matches PointCloud.vert value for value, so
 * a test can predict exactly which points the GPU draws. Changing anything in
 * this header means changing the shader in the same way.
 */
namespace cw::clod {

    //! The rank's spatially regular steps, the two octant digits below
    constexpr int kRankSteps = cw::octree::kChildCount * cw::octree::kChildCount;

    //! The rank of each sub-octant of a cell, so the first points kept are
    //! corner-opposite and the survivors stay spread over the parent grid
    //! rather than clumping into one corner of it.
    constexpr std::array<int, cw::octree::kChildCount> kOctantOrder = {0, 4, 6, 2, 3, 7, 5, 1};

    //! The quantized steps per cell of a node's sample grid: a point's cell
    //! index along an axis is its quantized coordinate divided by this.
    constexpr int kQuantStepsPerCell = (int(cw::octree::kQuantMax) + 1)
                                       / cw::octree::kSampleGridResolution;

    //! The bits of a quantized coordinate that sit inside one cell
    constexpr int kSubCellBits = [] {
        int bits = 0;
        for (int steps = kQuantStepsPerCell; steps > 1; steps >>= 1) {
            bits++;
        }
        return bits;
    }();
    constexpr quint32 kSubCellMask = quint32(kQuantStepsPerCell) - 1u;

    static_assert(1 << kSubCellBits == kQuantStepsPerCell,
                  "A cell is a power of two quantization steps wide");
    //PointCloud.vert carries these three as literals
    static_assert(kQuantStepsPerCell == 512, "PointCloud.vert spells this out");
    static_assert(kSubCellBits == 9, "PointCloud.vert spells this out");
    static_assert(kRankSteps == 64, "PointCloud.vert spells this out");

    /**
     * A value in [0, 1) mixed from the 27 bits that say where a point sits
     * inside its own cell: x's low nine bits, then y's, then z's. The two
     * octant digits give the rank its regular steps and this breaks the ties
     * inside a step, which is what keeps the thinning continuous and free of
     * visible structure under EDL.
     */
    inline float subCellHash01(const cw::octree::QuantizedPoint& point)
    {
        constexpr quint32 kMixA = 0x27D4EB2Du;
        constexpr quint32 kMixB = 0x165667B1u;
        constexpr int kShiftA = 15;
        constexpr int kShiftB = 13;
        //24 bits of mantissa, so the quotient is exact and stays under 1
        constexpr int kKeptBits = 24;
        constexpr int kDropBits = 32 - kKeptBits;
        constexpr float kKeptScale = 1.0f / float(1u << kKeptBits);

        quint32 hash = (quint32(point.x) & kSubCellMask)
                       | ((quint32(point.y) & kSubCellMask) << kSubCellBits)
                       | ((quint32(point.z) & kSubCellMask) << (2 * kSubCellBits));
        hash *= kMixA;
        hash ^= hash >> kShiftA;
        hash *= kMixB;
        hash ^= hash >> kShiftB;
        return float(hash >> kDropBits) * kKeptScale;
    }

    /**
     * Where @a point falls in the thinning order of its node, in [0, 1). The
     * order is the sub-octant of the point's parent cell first, then the
     * sub-octant of its grandparent cell, then the hash above. Ranks are a
     * property of the point alone, so the set drawn at one weight is a subset
     * of the set drawn at any larger weight and zooming never flickers a point
     * off and back on.
     */
    inline float rank(const cw::octree::QuantizedPoint& point)
    {
        const quint32 cellX = quint32(point.x) / kQuantStepsPerCell;
        const quint32 cellY = quint32(point.y) / kQuantStepsPerCell;
        const quint32 cellZ = quint32(point.z) / kQuantStepsPerCell;

        const int parentOctant = int((cellX & 1u) | ((cellY & 1u) << 1)
                                     | ((cellZ & 1u) << 2));
        const int grandparentOctant = int(((cellX >> 1) & 1u) | (((cellY >> 1) & 1u) << 1)
                                          | (((cellZ >> 1) & 1u) << 2));

        const float steps = float(kOctantOrder.at(std::size_t(parentOctant))
                                      * cw::octree::kChildCount
                                  + kOctantOrder.at(std::size_t(grandparentOctant)));
        return (steps + subCellHash01(point)) / float(kRankSteps);
    }

    /**
     * The share of a node's points that draw where the cut is aiming for
     * @a targetSpacing meters between points and the node's own sample spacing
     * is @a nodeSpacing meters: 1 while the node is at or coarser than the
     * target, 0 once the target is a full level finer, and linear in level
     * between. A node is resident exactly while its weight is above zero
     * somewhere in view, so the cut needs no rule of its own: the level that
     * leaves it has already faded to nothing.
     */
    inline double weight(double nodeSpacing, double targetSpacing)
    {
        constexpr double kOctave = 2.0;
        return std::clamp(std::log2(kOctave * nodeSpacing / targetSpacing), 0.0, 1.0);
    }

    /**
     * weight() as @a mode reshapes it: the ramp itself for Continuous, a step
     * that keeps 1 and drops everything under it to 0 for Discrete — so a level
     * draws whole until its ramp starts, then not at all — and 1 everywhere
     * for Off. Discrete never draws a point Continuous drops, and Off draws
     * every point either one does.
     */
    inline double weight(double nodeSpacing, double targetSpacing,
                         cw::pointcloud::ThinningMode mode)
    {
        switch (mode) {
        case cw::pointcloud::ThinningMode::Discrete:
            // The weight reaches 1 exactly when nodeSpacing >= targetSpacing.
            // Both twins compare that in float, without log2, so a node at the
            // octave boundary draws whole or not at all on the same side as
            // the GPU instead of the whole node flipping on a rounding error.
            return float(nodeSpacing) >= float(targetSpacing) ? 1.0 : 0.0;
        case cw::pointcloud::ThinningMode::Off:
            return 1.0;
        case cw::pointcloud::ThinningMode::Continuous:
            break;
        }
        return weight(nodeSpacing, targetSpacing);
    }

    /**
     * The spacing the cut is aiming for at a vertex where a meter covers
     * @a pixelsPerMeter pixels, given the cloud's refine threshold
     * @a sseThresholdPx and the sample spacing @a rootSpacing of its root. The
     * root is the coarsest level the file has, so once the view is zoomed out
     * past it there is no coarser level left to carry the surface and the
     * target stops widening — without the floor the root itself would thin
     * away and the cloud would vanish.
     */
    inline double targetSpacing(double sseThresholdPx, double pixelsPerMeter,
                                double rootSpacing)
    {
        return std::min(sseThresholdPx / pixelsPerMeter, rootSpacing);
    }

    //! Whether @a point of a node whose sample spacing is @a nodeSpacing draws
    //! where the cut is aiming for @a targetSpacing meters between points.
    inline bool drawn(const cw::octree::QuantizedPoint& point, double nodeSpacing,
                      double targetSpacing)
    {
        return double(rank(point)) < weight(nodeSpacing, targetSpacing);
    }

    //! drawn() under thinning @a mode
    inline bool drawn(const cw::octree::QuantizedPoint& point, double nodeSpacing,
                      double targetSpacing, cw::pointcloud::ThinningMode mode)
    {
        return double(rank(point)) < weight(nodeSpacing, targetSpacing, mode);
    }

    /**
     * The spacing a node sizes and thins by under @a source, given its grid
     * spacing @a gridSpacing and the spacing @a measuredSpacing the builder
     * measured from its own points (0 where it measured nothing). Grid is the
     * grid spacing alone; Measured is the larger of the two, so a node sparser
     * than its grid answers for the gap it really has and a denser one keeps
     * its grid. Local thins and cuts as Measured does. PointCloud.vert takes
     * the same maximum.
     */
    inline double drawSpacing(double gridSpacing, double measuredSpacing,
                              cw::pointcloud::SpacingSource source)
    {
        if (source == cw::pointcloud::SpacingSource::Grid) {
            return gridSpacing;
        }
        return std::max(gridSpacing, measuredSpacing);
    }

    /**
     * The native spacing, in meters, of a point whose reserved word holds
     * @a code (cw::octree::encodeLocalSpacing): an absolute spacing whatever
     * the grid, and @a gridSpacing, its node's, for an unmeasured code.
     * PointCloud.vert decodes qpos.w the same way.
     */
    inline double localSpacing(quint16 code, double gridSpacing)
    {
        return cw::octree::decodeLocalSpacing(code, gridSpacing);
    }

    static_assert(cw::octree::kMinLocalSpacingUnits == 0.005
                      && cw::octree::kMaxLocalSpacingUnits == 64.0
                      && cw::octree::kUnmeasuredLocalSpacingCode == 0
                      && cw::octree::kQuantMax == 65535,
                  "PointCloud.vert spells this out");

    /**
     * The spacing a sprite's world-space floor covers under @a source, given
     * the floor its node draws with this frame @a drawnFloor (revealedFloor()
     * of the floor ramp, finestDrawnSpacings() or requestedCutSpacings() once
     * it settles) and the point's own @a nativeSpacing (localSpacing()). Grid
     * and Measured cover the drawn floor. Local covers the larger of the two,
     * so a point whose node's floor a tiny dense descendant lifted still
     * covers the data's spacing around it. PointCloud.vert takes the same
     * maximum.
     */
    inline double sizeSpacing(double drawnFloor, double nativeSpacing,
                              cw::pointcloud::SpacingSource source)
    {
        if (source == cw::pointcloud::SpacingSource::Local) {
            return std::max(drawnFloor, nativeSpacing);
        }
        return drawnFloor;
    }

    //! The frames from @a since to @a frame, the age the reveal ramp reads:
    //! the unsigned difference of the two truncated to 32 bits, as
    //! PointCloud.vert takes it, so a @a since after @a frame wraps to an age
    //! long past every ramp just as it does on the GPU
    constexpr quint32 revealAge(quint64 frame, quint64 since)
    {
        return quint32(frame) - quint32(since);
    }

    /**
     * The share of its weight a node draws @a age frames after it arrived,
     * under a reveal ramp of @a revealFrames frames: (age + 1) / revealFrames,
     * capped at 1, so the arrival frame draws 1 / revealFrames of what the
     * weight keeps and frame revealFrames - 1 draws all of it. 1 at every age
     * while @a revealFrames is 0. PointCloud.vert scales its weight the same
     * way.
     */
    inline double revealScale(quint32 age, int revealFrames)
    {
        if (revealFrames <= 0) {
            return 1.0;
        }
        return std::min(1.0, (double(age) + 1.0) / double(revealFrames));
    }

    /**
     * The side, in pixels, of a sprite whose settled side is @a sizePx, @a age
     * frames into a Grow arrival under a reveal ramp of @a revealFrames
     * frames (cw::pointcloud::RevealMode::Grow): one pixel growing to
     * @a sizePx with the sprite's area linear in revealScale(age,
     * revealFrames), so side = sqrt(1 + (sizePx^2 - 1) * revealScale), and
     * exactly @a sizePx from frame revealFrames - 1 on and at every age while
     * @a revealFrames is 0. PointCloud.vert grows its sprites the same way.
     */
    inline double grownSize(double sizePx, quint32 age, int revealFrames)
    {
        const double scale = revealScale(age, revealFrames);
        if (scale >= 1.0 || sizePx <= cw::pointcloud::kMinPointSizePx) {
            return sizePx;
        }
        const double minArea = cw::pointcloud::kMinPointSizePx * cw::pointcloud::kMinPointSizePx;
        return std::sqrt(minArea + (sizePx * sizePx - minArea) * scale);
    }

    /**
     * The floor a node sizes by @a age frames after its floor moved from
     * @a previousFloor to @a floor, under a reveal ramp of @a revealFrames
     * frames: the two interpolated in log space by revealScale(age,
     * revealFrames), so a floor halves or doubles by equal ratios frame to
     * frame and a rise takes as many frames as a fall. @a floor itself while
     * @a revealFrames is 0 or the node has no previous floor.
     */
    inline double revealedFloor(double previousFloor, double floor, quint32 age,
                                int revealFrames)
    {
        const double scale = revealScale(age, revealFrames);
        if (scale >= 1.0 || previousFloor <= 0.0 || floor <= 0.0) {
            return floor;
        }
        return std::exp2(std::log2(previousFloor)
                         + (std::log2(floor) - std::log2(previousFloor)) * scale);
    }

    //! The share of its weight a node that left the cut @a age frames ago
    //! still draws: the arrival's revealScale() run backwards, so the frame it
    //! leaves draws 1 - 1 / revealFrames and frame revealFrames - 1 draws
    //! nothing. PointCloud.vert fades a departing node the same way.
    inline double departureScale(quint32 age, int revealFrames)
    {
        return 1.0 - revealScale(age, revealFrames);
    }

    //! The age a fade-in takes over at from a node drawing @a share of its
    //! weight, so its first frame draws one step past that share rather than
    //! starting again from 1 / @a revealFrames
    inline quint32 fadeInAgeFor(double share, int revealFrames)
    {
        if (revealFrames <= 0) {
            return 0u;
        }
        return quint32(std::lround(std::clamp(share, 0.0, 1.0) * double(revealFrames)));
    }

    //! The age a fade-out takes over at from a node drawing @a share of its
    //! weight, so its first frame draws one step under that share:
    //! departureScale() at that age is @a share less 1 / @a revealFrames
    inline quint32 fadeOutAgeFor(double share, int revealFrames)
    {
        return quint32(std::max(revealFrames, 0)) - fadeInAgeFor(share, revealFrames);
    }

    //! Whether a ramp @a age frames old still has frames to draw under a
    //! reveal ramp of @a revealFrames frames, which is what keeps the frame
    //! loop drawing: revealScale() is still under 1.
    inline bool revealing(quint32 age, int revealFrames)
    {
        return revealScale(age, revealFrames) < 1.0;
    }

    //! The sample spacing of a node whose quantization step — the w of its
    //! instance's nodeOriginScale — is @a quantizationStep meters.
    constexpr double nodeSpacingFromQuantizationStep(double quantizationStep)
    {
        return quantizationStep * double(cw::octree::kQuantMax)
               / double(cw::octree::kSampleGridResolution);
    }
}

#endif // CWPOINTCLOUDCLOD_H
