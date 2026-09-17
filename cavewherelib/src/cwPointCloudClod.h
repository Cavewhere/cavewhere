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

    //! The sample spacing of a node whose quantization step — the w of its
    //! instance's nodeOriginScale — is @a quantizationStep meters.
    constexpr double nodeSpacingFromQuantizationStep(double quantizationStep)
    {
        return quantizationStep * double(cw::octree::kQuantMax)
               / double(cw::octree::kSampleGridResolution);
    }
}

#endif // CWPOINTCLOUDCLOD_H
