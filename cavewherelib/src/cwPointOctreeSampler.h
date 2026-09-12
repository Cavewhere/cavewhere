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
     */
    CAVEWHERE_LIB_EXPORT QVector<SampledNode> buildSubtree(QVector<QVector3D> points,
                                                           const Cell& root,
                                                           const QVector3D& rootMin,
                                                           double rootSize,
                                                           int leafMaxPoints = kLeafMaxPoints);
}
