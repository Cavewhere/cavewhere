/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSPATIALHASHGRID2D_H
#define CWSPATIALHASHGRID2D_H

// Qt includes
#include <QHash>
#include <QLineF>
#include <QList>
#include <QRectF>
#include <QtGlobal>

// Std includes
#include <cmath>

// Our includes
#include "cwGlobals.h"

/**
 * \brief A uniform spatial hash over a 2D plane for broad-phase collision
 *        queries.
 *
 * The grid maps caller-defined integer item ids (typically indices into a
 * parallel array the caller owns) to the cells their bounding box or segment
 * covers. It is a pure broad-phase accelerator: query() / anyOf() return the
 * ids whose cells overlap a query region, and the caller performs the exact
 * geometry test. The grid never stores geometry itself, so the same instance
 * accelerates rectangle-vs-rectangle, segment-vs-rectangle, or any other
 * exact test the caller wants to run on the reported candidates.
 *
 * Only occupied cells consume memory (a QHash of buckets), so a sparse plot on
 * a large plane costs O(items), not O(area) — the reason this is a hash grid
 * rather than a dense array.
 *
 * Cell size should be chosen roughly on the scale of a typical query region:
 * too small and items span many cells (more buckets, more dedup work), too
 * large and each cell holds many items (the query degrades toward a linear
 * scan). Within an order of magnitude of the query size is fine.
 *
 * query() and anyOf() dedup via a per-item generation stamp, so an item that
 * spans several of the queried cells is reported exactly once per call. This
 * matters for counting queries (e.g. tallying crossings), where a double
 * visit would corrupt the count. The dedup state is mutable, so the grid is
 * not safe to query from multiple threads concurrently.
 */
class CAVEWHERE_LIB_EXPORT cwSpatialHashGrid2D
{
public:
    explicit cwSpatialHashGrid2D(qreal cellSize);

    qreal cellSize() const { return m_cellSize; }
    bool isEmpty() const { return m_buckets.isEmpty(); }

    // Profiling: total cell coordinates walked across all query()/anyOf() calls
    // (i.e. hash probes, whether or not the cell is occupied). Comparing this to
    // the caller's exact-test count reveals whether query cost is dominated by
    // iterating the region's cells or by the exact tests on found items.
    qint64 cellsVisited() const { return m_cellsVisited; }
    void resetCellsVisited() { m_cellsVisited = 0; }

    // Empties all buckets but keeps the hash and per-bucket capacity, so a
    // rebuilt grid (e.g. a per-frame declutter) avoids re-growing. Stale visit
    // stamps stay valid because every query bumps the generation counter.
    void clear();

    // Sizes the per-item dedup array up front so inserts of ids < itemCount
    // don't reallocate it. Optional; inserts grow it on demand otherwise.
    void reserve(int itemCount);

    // Registers itemId in every cell its bounding box overlaps.
    void insert(int itemId, const QRectF& bbox);

    // Registers itemId in every cell the segment passes through (a grid
    // traversal, not the segment's filled bounding box), so a long diagonal
    // segment occupies O(length / cellSize) cells rather than O((length /
    // cellSize)^2).
    void insert(int itemId, const QLineF& segment);

    // Visits every distinct item id whose cells overlap `region`, calling
    // fn(int itemId) once per id. The caller runs the exact test inside fn.
    template<class Fn>
    void query(const QRectF& region, Fn&& fn) const
    {
        const quint32 generation = ++m_queryGeneration;
        forEachCell(region, [&](const QList<int>& bucket) {
            for(int itemId : bucket) {
                if(m_visitStamp[itemId] == generation) {
                    continue;
                }
                m_visitStamp[itemId] = generation;
                fn(itemId);
            }
        });
    }

    // Like query(), but stops at the first id for which pred(int itemId)
    // returns true and returns true; returns false if no id satisfies pred.
    template<class Pred>
    bool anyOf(const QRectF& region, Pred&& pred) const
    {
        const quint32 generation = ++m_queryGeneration;
        const int x0 = cellCoord(region.left());
        const int x1 = cellCoord(region.right());
        const int y0 = cellCoord(region.top());
        const int y1 = cellCoord(region.bottom());
        for(int cellY = y0; cellY <= y1; ++cellY) {
            for(int cellX = x0; cellX <= x1; ++cellX) {
                ++m_cellsVisited;
                const auto it = m_buckets.constFind(cellKey(cellX, cellY));
                if(it == m_buckets.constEnd()) {
                    continue;
                }
                for(int itemId : *it) {
                    if(m_visitStamp[itemId] == generation) {
                        continue;
                    }
                    m_visitStamp[itemId] = generation;
                    if(pred(itemId)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

private:
    static quint64 cellKey(int cellX, int cellY)
    {
        // Reinterpret each 32-bit signed cell coordinate into the two halves of
        // a 64-bit key. This is a bijection, so distinct cells never collide.
        return (quint64(quint32(cellX)) << 32) | quint64(quint32(cellY));
    }

    int cellCoord(qreal value) const
    {
        return int(std::floor(value / m_cellSize));
    }

    void ensureStamp(int itemId);

    // Invokes fn(const QList<int>&) for each occupied bucket overlapping region.
    template<class Fn>
    void forEachCell(const QRectF& region, Fn&& fn) const
    {
        const int x0 = cellCoord(region.left());
        const int x1 = cellCoord(region.right());
        const int y0 = cellCoord(region.top());
        const int y1 = cellCoord(region.bottom());
        for(int cellY = y0; cellY <= y1; ++cellY) {
            for(int cellX = x0; cellX <= x1; ++cellX) {
                ++m_cellsVisited;
                const auto it = m_buckets.constFind(cellKey(cellX, cellY));
                if(it == m_buckets.constEnd()) {
                    continue;
                }
                fn(*it);
            }
        }
    }

    qreal m_cellSize;
    QHash<quint64, QList<int>> m_buckets;
    mutable QList<quint32>     m_visitStamp;      // per-item last-visited generation
    // Bumped once per query() / anyOf(). A stamp equal to the current value
    // means "already visited this query". Wraps after 2^32 queries on one grid
    // instance, where a stale stamp could alias generation 0 — unreachable in
    // practice (a grid is rebuilt per export and sees millions, not billions,
    // of queries).
    mutable quint32            m_queryGeneration = 0;
    mutable qint64             m_cellsVisited = 0; // profiling: cells walked by queries
};

#endif // CWSPATIALHASHGRID2D_H
