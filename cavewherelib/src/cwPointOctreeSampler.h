// cwPointOctreeSampler.h
#pragma once

//Qt includes
#include <QVector>
#include <QVector3D>

//Std includes
#include <array>
#include <optional>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPointOctree.h"
#include "qbox3d.h"

/**
 * The additive sampler that turns a bag of points into an octree: a point
 * lives in exactly one node, and a parent holds the points that fill the gaps
 * between its children's points. Everything here is pure, so it runs on any
 * cwConcurrent worker.
 */
namespace cw::octree {

    //Addresses a node by its cell in the 2^level grid over the root cube
    struct Cell {
        int level = 0;
        quint32 x = 0;
        quint32 y = 0;
        quint32 z = 0;
    };

    CAVEWHERE_LIB_EXPORT QBox3D cellBounds(const Cell& cell, const QVector3D& rootMin, double rootSize);

    //octant bit 0 is x, bit 1 is y, bit 2 is z
    CAVEWHERE_LIB_EXPORT Cell childCell(const Cell& parent, int octant);

    /**
     * The octant of parentBounds that holds point. A point on a face, on the
     * max corner, or outside the box clamps to the nearest octant, so every
     * point names a real child.
     */
    CAVEWHERE_LIB_EXPORT int octantOf(const QVector3D& point, const QBox3D& parentBounds);

    /**
     * Overlays a resolution^3 grid on bounds, removes the point nearest the
     * center of every occupied cell from points, and returns the removed
     * points. Both the sample and the survivors keep the order they had in
     * points, so the same input always gives the same answer.
     */
    CAVEWHERE_LIB_EXPORT QVector<QVector3D> gridSample(const QBox3D& bounds,
                                                       int resolution,
                                                       QVector<QVector3D>& points);

    /**
     * The pull-up step: one grid sample of the union of the children's points
     * at the parent's spacing. Each child vector drops the points the parent
     * took.
     */
    CAVEWHERE_LIB_EXPORT QVector<QVector3D> sampleUp(const QBox3D& parentBounds,
                                                     const QVector<QVector<QVector3D>*>& childrenPoints);

    /**
     * The spacing between @a points, in meters, measured over the xy extent
     * they occupy. A node's grid spacing is what its level promises; this is
     * what its points hold, which is wider where the data is sparser than the
     * grid. 0 when there is nothing to measure: fewer than two points, or
     * points stacked on one spot of the xy plane.
     */
    CAVEWHERE_LIB_EXPORT float measureNodeSpacing(const QVector<QVector3D>& points);

    struct SampledNode {
        Cell cell;
        QVector<QVector3D> points;

        //Indices into the vector buildSubtree() returns, -1 when the octant is empty
        std::array<int, kChildCount> children = {-1, -1, -1, -1, -1, -1, -1, -1};
    };

    /**
     * Builds the subtree rooted at root out of points, which must all lie
     * inside root's bounds. The nodes come back depth first with the root at
     * index 0, and every input point lands in exactly one of them.
     *
     * A child the parent's sample drained is dropped, so a node whose children
     * were all drained comes back childless while holding up to
     * kSampleGridResolution^3 points, which can exceed leafMaxPoints.
     *
     * A node that sampled first keeps the points its grid left behind as a
     * leaf while they number fewer than @a minSplitPoints (kMinSplitPoints);
     * 0 splits every leftover.
     */
    CAVEWHERE_LIB_EXPORT QVector<SampledNode> buildSubtree(QVector<QVector3D> points,
                                                           const Cell& root,
                                                           const QVector3D& rootMin,
                                                           double rootSize,
                                                           int leafMaxPoints = kLeafMaxPoints,
                                                           int minSplitPoints = kMinSplitPoints);

    /**
     * The native spacing (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html section 14.5,
     * WP13) of every point of every node of @a nodes, a subtree as
     * buildSubtree() returns it with its root at index 0: one vector per node,
     * one spacing per point in the node's order, in the octree's units.
     *
     * A point's native spacing is the distance r_k to its
     * kLocalSpacingNeighbors-th nearest distinct neighbor among every point of
     * every node of @a nodes, times kLocalSpacingSurfaceFactor. The octree is
     * additive, so those nodes together are the raw data of the subtree's
     * cell: a point measures the data around it whatever level it sits at.
     * Bit-identical points count once, so a stack of duplicate returns reads
     * as one neighbor rather than a dense patch. Neighbor information stops at
     * the subtree's own cell: a point beside one of its faces finds its k-th
     * neighbor at most sqrt(2) farther than it would with the cell's
     * neighbors there (half the disc), and one at a corner of two faces at
     * most twice as far.
     *
     * Each spacing is clamped to [kMinLocalSpacingUnits, localSpacingCap()]
     * of its node's grid spacing, or of @a firstNodeCapLevel's grid for the
     * root when given; a point with fewer than k distinct neighbors inside
     * that cap's reach takes the cap. Nodes shallower than
     * @a firstCodedLevel come back empty and still count as neighbors.
     * @a rootMin and @a rootSize are the octree root cube's.
     */
    CAVEWHERE_LIB_EXPORT QVector<QVector<float>> estimateNativeSpacing(
        const QVector<SampledNode>& nodes, const QVector3D& rootMin, double rootSize,
        int firstCodedLevel = 0, std::optional<int> firstNodeCapLevel = std::nullopt);

    //! The codes (encodeLocalSpacing) of @a spacings, each held to @a cap first
    CAVEWHERE_LIB_EXPORT QVector<quint16> encodeLocalSpacings(const QVector<float>& spacings,
                                                              double cap);
}
