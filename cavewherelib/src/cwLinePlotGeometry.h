/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWLINEPLOTGEOMETRY_H
#define CWLINEPLOTGEOMETRY_H

#include "CaveWhereLibExport.h"
#include "Monad/Result.h"
#include "cwCavingRegionData.h"
#include "cwSurveyNetwork.h"

#include <QVector>
#include <QVector3D>
#include <QUuid>

/**
 * \brief Generate the 3D line-plot geometry for a caving region.
 *
 * Iterates the region's caves, trips and survey chunks; produces a vector of
 * station positions laid out as a non-indexed line list and per-cave
 * length/depth values.
 *
 * A trip attached to an external centerline owns no cwSurveyChunk — its shot
 * topology exists only in the solved survey network. For those scopes the
 * segments are read from `network` instead (coordinates still come from the
 * cave's position lookup, so both paths share the world-origin-relative
 * space). Pass an empty network when there are no external scopes.
 *
 * Vertices are de-shared per shot: each drawn shot owns its own two endpoint
 * vertices (points[2i] = from, points[2i+1] = to), so a station reused by
 * consecutive shots — or shared as a tie-in between trips — is duplicated, once
 * per shot. Collapsing a shot's two vertices therefore affects exactly that
 * shot, with no shared-vertex bookkeeping.
 *
 * Each trip's vertices are emitted contiguously; tripVertexRanges[i] gives the
 * [start, count) span of running trip i, and tripUuids[i] maps that running id
 * back to a stable cwTripData::id so callers can re-attach it to a live trip
 * without relying on list position. A running id is assigned to every trip in
 * cave->trip iteration order, even trips that emit no geometry (count 0), so
 * both tables are the total trip count.
 *
 * Pure compute — no file I/O, no Qt object machinery. Caller invokes from
 * any thread; only reads the const region snapshot.
 */
class CAVEWHERE_LIB_EXPORT cwLinePlotGeometry
{
public:
    cwLinePlotGeometry() = delete;

    class CaveLengthAndDepth {
    public:
        CaveLengthAndDepth() : Depth(-1), Length(-1) {}
        CaveLengthAndDepth(double length, double depth) : Depth(depth), Length(length) {}

        double length() const { return Length; }
        double depth() const { return Depth; }

    private:
        double Depth;
        double Length;
    };

    // A contiguous span of vertices in Result::points, [start, start + count).
    // One per trip (running-id indexed); count is even (2 per drawn shot) and
    // 0 for a trip that emitted no geometry.
    struct VertexRange {
        int start = 0;
        int count = 0;
    };

    struct Result {
        QVector<QVector3D> points;              // 2 per drawn shot (non-indexed line list)
        QVector<VertexRange> tripVertexRanges;  // running trip id -> span in points
        QVector<QUuid> tripUuids;               // running trip id -> stable cwTripData::id
        QVector<CaveLengthAndDepth> cavesLengthAndDepths;
    };

    static Monad::Result<Result> generate(const cwCavingRegionData& region,
                                          const cwSurveyNetwork& network = cwSurveyNetwork());
};

#endif // CWLINEPLOTGEOMETRY_H
