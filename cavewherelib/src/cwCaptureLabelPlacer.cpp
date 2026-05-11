/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwCaptureLabelPlacer.h"

// Qt includes
#include <QDebug>
#include <QtMath>

// Std includes
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

// Felzenszwalb's algorithm computes parabola intersections via
// ((f[q]+q*q) - (f[v]+v*v)) / (2*(q-v)). With true +infinity inputs this
// produces inf-inf -> NaN and corrupts the lower envelope. Use a large but
// finite sentinel for "no obstacle here" — large enough that any squared
// distance the DT can produce stays well below it, but finite enough for
// arithmetic to remain well-defined.
constexpr float kLargeSquaredDist = 1.0e18f;

inline int cellIndex(int x, int y, int w)
{
    return y * w + x;
}

// Felzenszwalb & Huttenlocher 1D squared distance transform. Given an input
// array f (already initialized to "0 at sites, infinity elsewhere" semantics
// after taking f as the squared-distance lower envelope), writes the lower
// envelope of parabolas rooted at each cell into d. O(n).
void felzenszwalb1D(const float* f, float* d, int n,
                    int* v, float* z)
{
    int k = 0;
    v[0] = 0;
    z[0] = -kLargeSquaredDist;
    z[1] =  kLargeSquaredDist;

    for(int q = 1; q < n; q++) {
        // Intersection of parabolas rooted at q and v[k]:
        // s = ((f[q]+q*q) - (f[v[k]]+v[k]*v[k])) / (2(q - v[k]))
        float s;
        while(true) {
            const float num = (f[q] + float(q) * q)
                            - (f[v[k]] + float(v[k]) * v[k]);
            const float den = 2.0f * float(q - v[k]);
            s = num / den;
            if(s <= z[k]) {
                k--;
            } else {
                break;
            }
        }
        k++;
        v[k] = q;
        z[k] = s;
        z[k + 1] = kLargeSquaredDist;
    }

    k = 0;
    for(int q = 0; q < n; q++) {
        while(z[k + 1] < float(q)) {
            k++;
        }
        const float dq = float(q - v[k]);
        d[q] = dq * dq + f[v[k]];
    }
}

void distanceTransform2D(QVector<float>& dt, int w, int h)
{
    const int maxDim = qMax(w, h);
    QVector<float> tmp(maxDim);
    QVector<float> out(maxDim);
    QVector<int>   v(maxDim);
    QVector<float> z(maxDim + 1);

    // Rows
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            tmp[x] = dt[cellIndex(x, y, w)];
        }
        felzenszwalb1D(tmp.constData(), out.data(), w, v.data(), z.data());
        for(int x = 0; x < w; x++) {
            dt[cellIndex(x, y, w)] = out[x];
        }
    }

    // Columns
    for(int x = 0; x < w; x++) {
        for(int y = 0; y < h; y++) {
            tmp[y] = dt[cellIndex(x, y, w)];
        }
        felzenszwalb1D(tmp.constData(), out.data(), h, v.data(), z.data());
        for(int y = 0; y < h; y++) {
            dt[cellIndex(x, y, w)] = out[y];
        }
    }

    // Take sqrt to convert squared distance to Euclidean.
    for(int i = 0, n = dt.size(); i < n; i++) {
        dt[i] = std::sqrt(dt[i]);
    }
}

QPointF closestPointOnRect(const QRectF& rect, const QPointF& p)
{
    return QPointF(qBound(rect.left(),   p.x(), rect.right()),
                   qBound(rect.top(),    p.y(), rect.bottom()));
}

} // anonymous namespace

bool cwCaptureLabelPlacer::segmentIntersectsRect(const QLineF& seg, const QRectF& rect)
{
    if(rect.contains(seg.p1()) || rect.contains(seg.p2())) {
        return true;
    }
    const QLineF edges[4] = {
        QLineF(rect.topLeft(),     rect.topRight()),
        QLineF(rect.topRight(),    rect.bottomRight()),
        QLineF(rect.bottomRight(), rect.bottomLeft()),
        QLineF(rect.bottomLeft(),  rect.topLeft()),
    };
    QPointF dummy;
    for(const QLineF& edge : edges) {
        if(seg.intersects(edge, &dummy) == QLineF::BoundedIntersection) {
            return true;
        }
    }
    return false;
}

cwCaptureLabelPlacer::cwCaptureLabelPlacer() = default;

QFont cwCaptureLabelPlacer::scaledFont(const QFont& base, int dpi)
{
    QFont f = base;
    const int px = qMax(1, qRound(base.pointSizeF() * dpi * PointsToPixelsAt72Dpi));
    f.setPixelSize(px);
    return f;
}

void cwCaptureLabelPlacer::setObstacleBounds(const QRectF& parentBounds, qreal cellSizePaperPx)
{
    m_bounds = parentBounds;
    m_cellSize = cellSizePaperPx > 0.0 ? cellSizePaperPx : 2.0;
    m_maskW = qMax(1, qCeil(parentBounds.width()  / m_cellSize));
    m_maskH = qMax(1, qCeil(parentBounds.height() / m_cellSize));
    // Clear cells start at a large finite "no obstacle" sentinel; addTileAlpha
    // and addObstacleRect drop entries to 0 before finalize() runs the DT.
    m_dt.fill(kLargeSquaredDist, m_maskW * m_maskH);
    m_finalized = false;
}

void cwCaptureLabelPlacer::setViewportBounds(const QRectF& viewportBounds)
{
    m_viewportBounds = viewportBounds;
}

void cwCaptureLabelPlacer::setLabelMarginPaperPx(qreal margin)
{
    m_labelMargin = qMax(qreal(0.0), margin);
}

void cwCaptureLabelPlacer::setAlphaThreshold(int threshold)
{
    m_alphaThreshold = threshold;
}

void cwCaptureLabelPlacer::addTileAlpha(const QImage& tileImage,
                                        const QPointF& tilePosParent,
                                        qreal tileScale)
{
    if(m_maskW <= 0 || m_maskH <= 0) {
        return;
    }
    if(tileImage.isNull() || tileScale <= 0.0) {
        return;
    }

    const QImage img = (tileImage.format() == QImage::Format_ARGB32
                        || tileImage.format() == QImage::Format_ARGB32_Premultiplied)
                           ? tileImage
                           : tileImage.convertToFormat(QImage::Format_ARGB32);

    const int iw = img.width();
    const int ih = img.height();
    const qreal invCell = 1.0 / m_cellSize;
    const qreal boundsLeft = m_bounds.left();
    const qreal boundsTop  = m_bounds.top();

    for(int py = 0; py < ih; py++) {
        const qreal parentY = tilePosParent.y() + py * tileScale;
        const int cellY = qFloor((parentY - boundsTop) * invCell);
        if(cellY < 0 || cellY >= m_maskH) {
            continue;
        }
        const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(py));
        const int rowBase = cellY * m_maskW;
        for(int px = 0; px < iw; px++) {
            if(qAlpha(row[px]) <= m_alphaThreshold) {
                continue;
            }
            const qreal parentX = tilePosParent.x() + px * tileScale;
            const int cellX = qFloor((parentX - boundsLeft) * invCell);
            if(cellX < 0 || cellX >= m_maskW) {
                continue;
            }
            m_dt[rowBase + cellX] = 0.0f;
        }
    }
}

void cwCaptureLabelPlacer::addObstacleRect(const QRectF& rectParent)
{
    if(m_maskW <= 0 || m_maskH <= 0) {
        return;
    }
    const qreal invCell = 1.0 / m_cellSize;
    const int x0 = qBound(0, qFloor((rectParent.left()   - m_bounds.left()) * invCell), m_maskW);
    const int y0 = qBound(0, qFloor((rectParent.top()    - m_bounds.top())  * invCell), m_maskH);
    const int x1 = qBound(0, qCeil ((rectParent.right()  - m_bounds.left()) * invCell), m_maskW);
    const int y1 = qBound(0, qCeil ((rectParent.bottom() - m_bounds.top())  * invCell), m_maskH);
    for(int y = y0; y < y1; y++) {
        const int rowBase = y * m_maskW;
        for(int x = x0; x < x1; x++) {
            m_dt[rowBase + x] = 0.0f;
        }
    }
}

void cwCaptureLabelPlacer::finalize()
{
    if(m_maskW <= 0 || m_maskH <= 0) {
        m_finalized = true;
        return;
    }
    distanceTransform2D(m_dt, m_maskW, m_maskH);
    m_finalized = true;
}

void cwCaptureLabelPlacer::clearPlacements()
{
    m_placedLabels.clear();
    m_lineObstacles.clear();
}

void cwCaptureLabelPlacer::addLineObstacle(const QLineF& segment, qreal thicknessPaperPx)
{
    if(segment.p1() == segment.p2()) {
        return;
    }
    m_lineObstacles.append({segment, thicknessPaperPx});
}

cwCaptureLabelPlacer::Placement cwCaptureLabelPlacer::placeLabel(const LabelRequest& request)
{
    Placement result;

    if(!m_finalized || m_maskW <= 0 || m_maskH <= 0) {
        return result;
    }
    if(request.size.width() <= 0.0 || request.size.height() <= 0.0) {
        return result;
    }

    const qreal halfW = request.size.width()  * 0.5;
    const qreal halfH = request.size.height() * 0.5;
    const qreal halfDiag = std::hypot(halfW, halfH);
    const qreal requiredClearanceParent = halfDiag + m_labelMargin;
    const qreal requiredClearanceCells  = requiredClearanceParent / m_cellSize;

    const int anchorCellX = qFloor((request.anchorPos.x() - m_bounds.left()) / m_cellSize);
    const int anchorCellY = qFloor((request.anchorPos.y() - m_bounds.top())  / m_cellSize);
    const int maxRadius = m_maskW + m_maskH;

    const QRectF clampRect = m_viewportBounds.isEmpty() ? m_bounds : m_viewportBounds;
    const qreal collisionPad = m_labelMargin;

    auto tryCell = [&](int cx, int cy) -> bool {
        if(cx < 0 || cy < 0 || cx >= m_maskW || cy >= m_maskH) {
            return false;
        }
        if(m_dt[cellIndex(cx, cy, m_maskW)] < requiredClearanceCells) {
            return false;
        }

        const QPointF center(m_bounds.left() + (cx + 0.5) * m_cellSize,
                             m_bounds.top()  + (cy + 0.5) * m_cellSize);
        const QRectF candidate(center - QPointF(halfW, halfH), request.size);

        // Must fit entirely inside the viewport bounds — clamping would
        // truncate the label, which we'd rather drop than render clipped.
        if(!clampRect.contains(candidate)) {
            return false;
        }

        // Avoid leader-line obstacles registered after finalize. Inflate the
        // candidate by half the leader's thickness plus the standard label
        // margin so the rejected zone is "rect with margin" vs "segment with
        // thickness".
        for(const LineObstacle& line : m_lineObstacles) {
            const qreal expand = line.thickness * 0.5 + m_labelMargin;
            const QRectF expandedCandidate = candidate.adjusted(-expand, -expand,
                                                                 expand,  expand);
            if(cwCaptureLabelPlacer::segmentIntersectsRect(line.segment, expandedCandidate)) {
                return false;
            }
        }

        // Avoid previously placed labels.
        const QRectF collisionRect = candidate.adjusted(-collisionPad, -collisionPad,
                                                         collisionPad,  collisionPad);
        if(!m_placedLabels.addRect(collisionRect.toAlignedRect())) {
            return false;
        }

        result.placed = true;
        result.labelRect = candidate;
        result.leaderEnd = request.anchorPos;
        result.leaderStart = closestPointOnRect(candidate, request.anchorPos);
        return true;
    };

    if(tryCell(anchorCellX, anchorCellY)) {
        return result;
    }

    for(int r = 1; r <= maxRadius; r++) {
        // Top and bottom edges of the ring (full width).
        for(int dx = -r; dx <= r; dx++) {
            if(tryCell(anchorCellX + dx, anchorCellY - r)) return result;
            if(tryCell(anchorCellX + dx, anchorCellY + r)) return result;
        }
        // Left and right edges of the ring (excluding corners already covered).
        for(int dy = -r + 1; dy <= r - 1; dy++) {
            if(tryCell(anchorCellX - r, anchorCellY + dy)) return result;
            if(tryCell(anchorCellX + r, anchorCellY + dy)) return result;
        }
    }

    return result;
}
