// cwPointOctreeSampler.h
#pragma once

//Qt includes
#include <QVector>
#include <QVector3D>

//Std includes
#include <array>

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

    //! What measureNativeSpacing() finds over one xy column of chunks
    struct NativeSpacingEstimate {
        //! One entry per subtree, then one vector per node, one spacing per
        //! point in the node's order
        QVector<QVector<QVector<float>>> spacings;
        //! The column's ceiling
        double ceiling = kMaxLocalSpacingUnits;
        //! The coded points that took the ceiling
        qint64 cappedPoints = 0;
    };

    /**
     * The native spacing (plans/POINT_CLOUD_EVAL_BENCH_PLAN.html section 14.5,
     * WP13, WP15 and WP16) of every point of every node of @a subtrees, each
     * a subtree as buildSubtree() returns it with its root at index 0, in the
     * octree's units. @a subtrees are the pass B chunks of one xy column and
     * @a neighbors the points of the cloud around it, its halo, which count as
     * neighbors and are not coded themselves.
     *
     * A point's native spacing is the distance r_k, along kNativeSpacingAxes
     * (the horizontal plane), to its kLocalSpacingNeighbors-th nearest
     * neighbor at a distinct position there among every point of
     * @a subtrees and @a neighbors, times kLocalSpacingSurfaceFactor. The
     * octree is additive, so the subtrees' nodes together are the raw data of
     * their cells: a point measures the data around it whatever level it sits
     * at. Points at one horizontal position count once, so a stack of
     * duplicate returns, or of returns down one pulse, reads as one neighbor
     * rather than a dense patch. Neighbor information stops where the column
     * and its halo end: a point measures exactly as a search over the whole
     * cloud would whenever its k-th neighbor lies inside that.
     *
     * The column's ceiling is kLoneSpacingMedianRatio times the median spacing
     * of its coded points that found k neighbors (kMaxLocalSpacingUnits where
     * none did): a point with fewer than k distinct neighbors, or one wider
     * than the ceiling, takes it. Every spacing is then clamped to
     * [kMinLocalSpacingUnits, kMaxLocalSpacingUnits]. @a rootMin and
     * @a rootSize are the octree root cube's.
     */
    CAVEWHERE_LIB_EXPORT NativeSpacingEstimate measureNativeSpacing(
        const QVector<const QVector<SampledNode>*>& subtrees, const QVector<QVector3D>& neighbors,
        const QVector3D& rootMin, double rootSize);

    //! The codes (encodeLocalSpacing) of @a spacings
    CAVEWHERE_LIB_EXPORT QVector<quint16> encodeLocalSpacings(const QVector<float>& spacings);
}
