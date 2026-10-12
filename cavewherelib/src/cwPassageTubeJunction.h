/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPASSAGETUBEJUNCTION_H
#define CWPASSAGETUBEJUNCTION_H

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPassageTubeBuilder.h"
#include "cwPassageTubeMesh.h"

//Qt includes
#include <QPointF>
#include <QVector>
#include <QVector3D>

//Std includes
#include <optional>

/**
 * The stitch pass that merges the arms of each junction into the chain that
 * passes through it (research §3.3.3), between ring fitting and meshing.
 *
 * Per junction: the through chain gets denser rings near the junction; each
 * of its columns grows as far as it stays inside an arm's fill (the arm's
 * section carried from the through's near wall to its far wall); each arm is
 * cut where it enters the through tube or a fill, and the cut points become
 * its mouth; the through quads under the mouth are removed and the hole's
 * edge is zipped to the mouth.
 *
 * It works on ring radii only, so rings from LRUD, splays or scraps all merge
 * the same way. Pure compute over value snapshots: callable from any thread.
 */
class CAVEWHERE_LIB_EXPORT cwPassageTubeJunction
{
public:
    cwPassageTubeJunction() = delete;

    using Ring = cwPassageTubeBuilder::Ring;

    /**
     * A chain's rings read as a solid. A point lies between the two rings
     * whose planes bound it along the chain, and inside when it falls within
     * their polygons of radii, interpolated linearly between bins and between
     * the two rings as the mesh lofts them. With a nonzero extendTangent,
     * points past the last ring along it belong to the last ring, so the tube
     * carries on.
     */
    struct RingTube {
        const QVector<Ring>* rings = nullptr;
        QVector3D extendTangent;

        bool inside(const QVector3D& point, double tolerance = 0.0) const;

        // How far the point lies outside the wall, negative inside, or
        // nullopt when it lies past either end.
        std::optional<double> wallDistance(const QVector3D& point) const;

        // The nearer of the two rings bounding the point, or -1 when it lies
        // past either end.
        int nearestRing(const QVector3D& point) const;

        // The point's offsets along ring's right and up, in its plane.
        QPointF localCoordinates(const QVector3D& point, int ring) const;
    };

    static void stitchJunctions(const QVector<cwPassageTubeBuilder::Leg>& legs,
                                QVector<cwPassageTubeBuilder::Chain>& chains,
                                const cwPassageTubeBuilder::Options& options,
                                QVector<cwPassageTubeMesh::Warning>& warnings);
};

#endif // CWPASSAGETUBEJUNCTION_H
