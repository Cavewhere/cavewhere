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

#include <QVector>
#include <QVector3D>
#include <QUuid>

/**
 * \brief Generate the 3D line-plot geometry for a caving region.
 *
 * Iterates the region's caves, trips and survey chunks; produces a vector of
 * station positions, a vector of indices that draw line segments between
 * those positions, and per-cave length/depth values.
 *
 * Vertices are de-shared per trip: each trip emits its own copy of the
 * stations it uses (a tie-in station shared by two trips appears as two
 * vertices), so every vertex belongs to exactly one trip. Each vertex carries
 * a dense running trip id (tripIds, parallel to points); tripUuids maps that
 * running id back to a stable cwTripData::id so callers can re-attach it to a
 * live trip without relying on list position. A running id is assigned to
 * every trip in cave->trip iteration order, even trips that emit no geometry,
 * so tripUuids.size() is the total trip count.
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

    struct Result {
        QVector<QVector3D> points;
        QVector<unsigned int> indices;
        QVector<quint32> tripIds;   // parallel to points: running trip id per vertex
        QVector<QUuid> tripUuids;   // running trip id -> stable cwTripData::id
        QVector<CaveLengthAndDepth> cavesLengthAndDepths;
    };

    static Monad::Result<Result> generate(const cwCavingRegionData& region);
};

#endif // CWLINEPLOTGEOMETRY_H
