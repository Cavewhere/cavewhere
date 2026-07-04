/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwSpatialHashGrid2D.h"

// Qt includes
#include <QtGlobal>

// Std includes
#include <cmath>
#include <limits>

namespace {
// A traversal that never terminates on its own is a bug (degenerate segment,
// NaN); cap the cell walk at a generous multiple of the Manhattan cell span so
// a malformed segment can't spin forever.
constexpr int kTraversalSlack = 2;
}

cwSpatialHashGrid2D::cwSpatialHashGrid2D(qreal cellSize) :
    m_cellSize(cellSize > 0.0 ? cellSize : 1.0)
{
}

void cwSpatialHashGrid2D::clear()
{
    // Keep the hash and each bucket's capacity — a rebuilt grid reuses them.
    // Visit stamps stay untouched because query() bumps the generation, so
    // every stamp left over from before the clear is already stale.
    for(auto it = m_buckets.begin(); it != m_buckets.end(); ++it) {
        it.value().clear();
    }
}

void cwSpatialHashGrid2D::reserve(int itemCount)
{
    if(itemCount > m_visitStamp.size()) {
        m_visitStamp.resize(itemCount, 0);
    }
}

void cwSpatialHashGrid2D::ensureStamp(int itemId)
{
    if(itemId >= m_visitStamp.size()) {
        m_visitStamp.resize(itemId + 1, 0);
    }
}

void cwSpatialHashGrid2D::insert(int itemId, const QRectF& bbox)
{
    ensureStamp(itemId);

    const QRectF normalized = bbox.normalized();
    const int x0 = cellCoord(normalized.left());
    const int x1 = cellCoord(normalized.right());
    const int y0 = cellCoord(normalized.top());
    const int y1 = cellCoord(normalized.bottom());

    for(int cellY = y0; cellY <= y1; ++cellY) {
        for(int cellX = x0; cellX <= x1; ++cellX) {
            m_buckets[cellKey(cellX, cellY)].append(itemId);
        }
    }
}

void cwSpatialHashGrid2D::insert(int itemId, const QLineF& segment)
{
    ensureStamp(itemId);

    // Amanatides & Woo grid traversal: walk cell-by-cell from p1 to p2,
    // always stepping across whichever axis boundary (x or y) is nearer,
    // visiting exactly the cells the segment passes through.
    const QPointF p1 = segment.p1();
    const QPointF p2 = segment.p2();

    int cellX = cellCoord(p1.x());
    int cellY = cellCoord(p1.y());
    const int endCellX = cellCoord(p2.x());
    const int endCellY = cellCoord(p2.y());

    const qreal deltaX = p2.x() - p1.x();
    const qreal deltaY = p2.y() - p1.y();

    const int stepX = deltaX > 0.0 ? 1 : -1;
    const int stepY = deltaY > 0.0 ? 1 : -1;

    constexpr qreal infinity = std::numeric_limits<qreal>::infinity();

    // Parametric distance (t in [0,1] along the segment) to the next cell
    // boundary on each axis, and the t-step between successive boundaries.
    qreal tMaxX = infinity;
    qreal tDeltaX = infinity;
    if(deltaX != 0.0) {
        const qreal nextBoundaryX = (deltaX > 0.0 ? (cellX + 1) : cellX) * m_cellSize;
        tMaxX = (nextBoundaryX - p1.x()) / deltaX;
        tDeltaX = m_cellSize / std::abs(deltaX);
    }

    qreal tMaxY = infinity;
    qreal tDeltaY = infinity;
    if(deltaY != 0.0) {
        const qreal nextBoundaryY = (deltaY > 0.0 ? (cellY + 1) : cellY) * m_cellSize;
        tMaxY = (nextBoundaryY - p1.y()) / deltaY;
        tDeltaY = m_cellSize / std::abs(deltaY);
    }

    const int maxSteps = (std::abs(endCellX - cellX) + std::abs(endCellY - cellY))
                         + kTraversalSlack;

    for(int step = 0; step <= maxSteps; ++step) {
        m_buckets[cellKey(cellX, cellY)].append(itemId);
        if(cellX == endCellX && cellY == endCellY) {
            break;
        }
        if(tMaxX < tMaxY) {
            cellX += stepX;
            tMaxX += tDeltaX;
        } else {
            cellY += stepY;
            tMaxY += tDeltaY;
        }
    }
}
