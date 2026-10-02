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
#include "cwStationPositionLookup.h"
#include "cwSurveyNetwork.h"

#include <QHash>
#include <QVector>
#include <QVector3D>
#include <QUuid>

/**
 * \brief Generate the 3D line-plot geometry for a caving region.
 *
 * Walks the region's survey tree pre-order — each node's own trips, then its
 * child nodes — and produces a vector of station positions laid out as a
 * non-indexed line list and per-node length/depth values. A node's length and
 * depth fold over its whole subtree: a Folder's length is the sum of its
 * caves', and its depth spans from the highest station below it to the lowest.
 *
 * A trip attached to an external centerline owns no cwSurveyChunk — its shot
 * topology exists only in the solved survey network. For those scopes the
 * segments are read from `network` instead (coordinates still come from the
 * node's position lookup, so both paths are in the project's local
 * projection frame). Pass an empty network when there are no external scopes.
 *
 * Vertices are de-shared per shot: each drawn shot owns its own two endpoint
 * vertices (points[2i] = from, points[2i+1] = to), so a station reused by
 * consecutive shots — or shared as a tie-in between trips — is duplicated, once
 * per shot. Collapsing a shot's two vertices therefore affects exactly that
 * shot, with no shared-vertex bookkeeping.
 *
 * Each leg has exactly one owner. A nested external scope's stations also
 * carry its parent scope's prefix, so a station claimed by a longer scope
 * prefix in the same node belongs to that innermost scope; a leg tying two
 * scopes together is drawn by whichever of them the node reaches first. Per-node
 * length and depth therefore count each leg once, as does per-trip visibility.
 *
 * Each trip's vertices are emitted contiguously — centerline segments first,
 * then the trip's splay segments (station position -> solved splay tip, also
 * two vertices per segment). tripVertexRanges[i] gives the full [start, count)
 * span of running trip i, and tripSplayVertexRanges[i] the splay sub-span at
 * its tail, so hiding a trip covers its splays and a splay-only toggle can
 * address just the tail. tripUuids[i] maps that running id back to a stable
 * cwTripData::id so callers can re-attach it to a live trip without relying on
 * list position. A running id is assigned to every trip in the walk's order,
 * even trips that emit no geometry (count 0), so all three tables are the
 * total trip count.
 *
 * `splayTipsByNode` is keyed by node id, and each node's tips by the
 * node-local canonical station name, landing only in the node that owns the
 * station. The survey data stores splays per station occurrence, so a name
 * shared between trips merges its tips into one bucket. Attribution rule: a
 * station's tips belong to the first native trip holding an occurrence of that
 * station with splays, falling back to the first native trip holding the
 * station at all; a station no native trip holds goes to the external scope
 * that owns it (tips from an external .svx include have no stored splays to
 * match). Splays never count toward length or depth.
 *
 * Pure compute — no file I/O, no Qt object machinery. Caller invokes from
 * any thread; only reads the const region snapshot.
 */
class CAVEWHERE_LIB_EXPORT cwLinePlotGeometry
{
public:
    cwLinePlotGeometry() = delete;

    class LengthAndDepth {
    public:
        LengthAndDepth() : Depth(0.0), Length(0.0) {}
        LengthAndDepth(double length, double depth) : Depth(depth), Length(length) {}

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
        QVector<QVector3D> points;              // 2 per drawn segment (non-indexed line list)
        QVector<VertexRange> tripVertexRanges;  // running trip id -> full span in points
        QVector<VertexRange> tripSplayVertexRanges; // running trip id -> splay tail of that span
        QVector<QUuid> tripUuids;               // running trip id -> stable cwTripData::id
        QHash<QUuid, LengthAndDepth> nodeLengthAndDepths; // every node, keyed by cwCaveData::id
    };

    static Monad::Result<Result> generate(
        const cwCavingRegionData& region,
        const cwSurveyNetwork& network = cwSurveyNetwork(),
        const QHash<QUuid, cwSplayTipsByStation>& splayTipsByNode = {});
};

#endif // CWLINEPLOTGEOMETRY_H
