/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Our includes
#include "cwCaptureLabelPlacer.h"
#include "cwDebug.h"

// Qt includes
#include <QDebug>
#include <QHash>
#include <QtMath>

// Std includes
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>

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

    // Billions of cells pass through here per export; QVector::operator[]'s
    // per-access detach check is too expensive at that scale, so the loops
    // below stay on raw pointers.
    float* dtData = dt.data();
    float* tmpData = tmp.data();
    float* outData = out.data();
    int* vData = v.data();
    float* zData = z.data();

    // Rows (contiguous in memory)
    for(int y = 0; y < h; y++) {
        float* row = dtData + qsizetype(y) * w;
        std::memcpy(tmpData, row, size_t(w) * sizeof(float));
        felzenszwalb1D(tmpData, outData, w, vData, zData);
        std::memcpy(row, outData, size_t(w) * sizeof(float));
    }

    // Columns (stride of one row)
    for(int x = 0; x < w; x++) {
        const float* src = dtData + x;
        for(int y = 0; y < h; y++, src += w) {
            tmpData[y] = *src;
        }
        felzenszwalb1D(tmpData, outData, h, vData, zData);
        float* dst = dtData + x;
        for(int y = 0; y < h; y++, dst += w) {
            *dst = outData[y];
        }
    }

    // Take sqrt to convert squared distance to Euclidean.
    for(qsizetype i = 0, n = dt.size(); i < n; i++) {
        dtData[i] = std::sqrt(dtData[i]);
    }
}

QPointF closestPointOnRect(const QRectF& rect, const QPointF& p)
{
    return QPointF(qBound(rect.left(),   p.x(), rect.right()),
                   qBound(rect.top(),    p.y(), rect.bottom()));
}

// Score-function tunables. Centerline-avoidance dominates: a single soft
// crossing outweighs ~200 paper-px of extra leader length. Direction bias
// is mild — DT-gradient direction is a tiebreaker, not a hard preference.
constexpr qreal kSoftCrossPenalty = 200.0; // paper-px per crossing
constexpr qreal kDirectionPenalty = 80.0;  // paper-px per radian

// labelSizeUpperBound slack: glyph ink can overhang the summed advances at
// both ends (bearings, italic swashes), and stacked diacritics can exceed
// the nominal ascent+descent. Two extra max-advance glyph widths and a
// doubled line height stay comfortably above any real font's overhang while
// remaining tiny relative to the viewport the bound is culled against —
// over-estimating only shrinks the cull's savings, never its correctness.
constexpr qsizetype kSizeBoundSlackGlyphs = 2;
constexpr qreal     kSizeBoundHeightFactor = 2.0;

// How many extra spiral rings to scan after the first hit. Bigger = more
// candidates compared, more time. 3 rings keeps the candidate list small
// (~24 cells) while giving the scorer enough diversity to find a winner.
constexpr int   kCandidateWindow = 3;

// Cap for the failed-placement spiral. Without a cap the spiral runs to the
// full page diagonal (m_maskW + m_maskH cells); on a whole-cave export that is
// hundreds of thousands of cells per hopeless anchor, each ring rescanning the
// obstacle lists — this is the export hang (150-mile Fisher Ridge). A label is
// only useful near its anchor, so bound the search to this many required-
// clearance radii out. Large enough to clear typical passage-width obstacle
// bands, small enough that a buried anchor gives up in O(label size), not
// O(page size).
constexpr int   kMaxSearchClearanceMultiple = 8;

// Candidate-spiral coarsening. The spiral samples one candidate per DT cell
// (~2 paper-px) — far finer than placement needs, since a label is many cells
// across and nudging it a couple of cells is imperceptible. Stepping the spiral
// coarsens the candidate grid and cuts the candidate cells (cellsTried) and the
// exact tests behind them by ~stepX*stepY. The step is set per axis to a fixed
// fraction of the label's extent on that axis, so a wide text label coarsens
// more along its long (horizontal) axis while its short axis — the one that
// actually has to fit inside a gap — stays finely sampled. Floored at 1 (a tiny
// label gets no coarsening) and capped so a very large label can't step clean
// over a narrow feasible band. DT clearance and every collision test stay exact
// on whichever cells are sampled, so no placed label ever overlaps another;
// coarsening can only shift a label within a step or, rarely, drop a marginal
// one whose only feasible position fell between samples.
constexpr qreal kCandidateStepLabelFraction = 0.35;
constexpr int   kMaxCandidateStep = 8;

// Floor for the broad-phase grid cell, in distance-transform cells. Keeps a
// degenerate (tiny) label from producing an absurdly fine grid whose cells hold
// almost nothing; 4 DT cells is still far below a typical label, so it only
// bites the degenerate case.
constexpr qreal kMinGridCellDtCells = 4.0;

// Default side length of a DT placement window, in DT *cells* (see
// setPlacementWindowSize). Cells are anchored to paper px (2 paper px each),
// so this is scale- and coordinate-space-independent — a preview, whose local
// unit spans many paper px, gets the same paper-space window as an export.
// 2048 cells = 4096 paper px ≈ 13.7 in at 300 DPI: big enough that a typical
// single-sheet export fits in ONE window (placement is then identical to the
// unwindowed algorithm), small enough that a whole-cave export's per-window
// DT stays tens of MB instead of the page's hundreds of GB.
constexpr int kDefaultPlacementWindowCells = 2048;

// zlib's fastest level, for the per-tile inked-cell masks. Cave ink is sparse,
// so the bitmaps are dominated by zero runs — higher levels only add CPU on
// the placement worker for the same order of shrinkage.
constexpr int kMaskCompressionLevel = 1;

// Coarsened candidate step along one axis, from the label's extent on that
// axis. Shared by the placeLabel spiral and spiralReachCells so the window
// halo always covers the spiral's true footprint.
inline int candidateStepForExtent(qreal labelExtent, qreal cellSize)
{
    return qBound(1,
                  qRound(labelExtent / cellSize * kCandidateStepLabelFraction),
                  kMaxCandidateStep);
}

} // anonymous namespace

bool cwCaptureLabelPlacer::segmentsCross(const QLineF& a, const QLineF& b,
                                         QPointF* outIntersection)
{
    // Endpoints that coincide should not count as a crossing — a leader
    // ending at the same point as another's endpoint (or touching it) is
    // a T-touch, not an X-cross.
    constexpr qreal kEndpointEpsilonSq = 1.0e-12;
    auto sqDist = [](const QPointF& p, const QPointF& q) {
        const qreal dx = p.x() - q.x();
        const qreal dy = p.y() - q.y();
        return dx * dx + dy * dy;
    };

    QPointF intersection;
    if(a.intersects(b, &intersection) != QLineF::BoundedIntersection) {
        return false;
    }
    if(sqDist(intersection, a.p1()) < kEndpointEpsilonSq) return false;
    if(sqDist(intersection, a.p2()) < kEndpointEpsilonSq) return false;
    if(sqDist(intersection, b.p1()) < kEndpointEpsilonSq) return false;
    if(sqDist(intersection, b.p2()) < kEndpointEpsilonSq) return false;
    if(outIntersection != nullptr) {
        *outIntersection = intersection;
    }
    return true;
}

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

QRectF cwCaptureLabelPlacer::viewportCullRect(const QRectF& viewportBounds,
                                              const QSizeF& labelSize,
                                              qreal labelMargin)
{
    // Clamp like setLabelMarginPaperPx does: a negative margin would shrink a
    // caller-side cull rect below the placer's own (clamped) cull rect and
    // silently drop anchors the placer would keep.
    const qreal clearance = std::hypot(labelSize.width() * 0.5,
                                       labelSize.height() * 0.5)
                            + qMax(0.0, labelMargin);
    return viewportBounds.adjusted(-clearance, -clearance, clearance, clearance);
}

QSizeF cwCaptureLabelPlacer::labelSizeUpperBound(const QFontMetricsF& metrics,
                                                 qsizetype textLength)
{
    // Guard against a degenerate maxWidth() (broken/bitmap-less font): the
    // line height is always positive for a usable font and no glyph's advance
    // meaningfully exceeds it by more than the slack already allows.
    const qreal glyphWidth = qMax(metrics.maxWidth(), metrics.height());
    return QSizeF(glyphWidth * qreal(textLength + kSizeBoundSlackGlyphs),
                  metrics.height() * kSizeBoundHeightFactor);
}

void cwCaptureLabelPlacer::setObstacleBounds(const QRectF& parentBounds, qreal cellSizePaperPx)
{
    m_bounds = parentBounds;
    m_cellSize = cellSizePaperPx > 0.0 ? cellSizePaperPx : 2.0;
    m_maskW = qMax(1, qCeil(parentBounds.width()  / m_cellSize));
    m_maskH = qMax(1, qCeil(parentBounds.height() / m_cellSize));
    // No page-sized allocation happens here (or anywhere): obstacle sources
    // are recorded and rasterized into window-sized DT grids on demand — see
    // ensureWindowFor. m_maskW/H only define the logical cell space.
    m_dt.clear();
    m_tileSources.clear();
    m_obstacleRects.clear();
    m_windowLoaded = false;
    m_finalized = false;
    m_gridCellSizeHint = 0.0;
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

void cwCaptureLabelPlacer::setPlacementWindowSize(qreal windowBoundsUnits)
{
    m_windowSize = windowBoundsUnits;
    m_windowLoaded = false;
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

    // Normalize the format once at record time so the mask scan reads QRgb
    // scan lines directly. Tiles from grabToImage are already
    // ARGB32_Premultiplied, so this conversion (a deep copy) only fires for
    // foreign formats.
    const QImage img = (tileImage.format() == QImage::Format_ARGB32
                        || tileImage.format() == QImage::Format_ARGB32_Premultiplied)
                           ? tileImage
                           : tileImage.convertToFormat(QImage::Format_ARGB32);

    TileAlphaSource tile;
    tile.image      = img;
    tile.sizePixels = img.size();
    tile.pos        = tilePosParent;
    tile.scale      = tileScale;
    appendTileSource(std::move(tile));
}

void cwCaptureLabelPlacer::addCompressedTileAlpha(const cwCompressedImage& tileImage,
                                                  const QPointF& tilePosParent,
                                                  qreal tileScale)
{
    if(m_maskW <= 0 || m_maskH <= 0) {
        return;
    }
    if(tileImage.isNull() || tileImage.size().isEmpty() || tileScale <= 0.0) {
        return;
    }

    TileAlphaSource tile;
    tile.compressed = tileImage;
    tile.sizePixels = tileImage.size();
    tile.pos        = tilePosParent;
    tile.scale      = tileScale;
    appendTileSource(std::move(tile));
}

void cwCaptureLabelPlacer::appendTileSource(TileAlphaSource&& tile)
{
    // Cell-space bbox of the tile, clamped to the page: the min/max cells any
    // of its pixels can map to (pixel px lands at pos + px * scale, so the
    // extremes are px = 0 and px = width-1). Window builds use it to skip
    // non-overlapping tiles without touching their pixels or mask.
    const qreal invCell = 1.0 / m_cellSize;
    const qreal left = m_bounds.left();
    const qreal top  = m_bounds.top();
    tile.cellX0 = qBound(0, qFloor((tile.pos.x() - left) * invCell), m_maskW);
    tile.cellY0 = qBound(0, qFloor((tile.pos.y() - top)  * invCell), m_maskH);
    tile.cellX1 = qBound(0, qFloor((tile.pos.x() + (tile.sizePixels.width()  - 1) * tile.scale
                                    - left) * invCell) + 1, m_maskW);
    tile.cellY1 = qBound(0, qFloor((tile.pos.y() + (tile.sizePixels.height() - 1) * tile.scale
                                    - top)  * invCell) + 1, m_maskH);

    m_tileSources.append(std::move(tile));
    m_windowLoaded = false;
}

void cwCaptureLabelPlacer::addObstacleRect(const QRectF& rectParent)
{
    if(m_maskW <= 0 || m_maskH <= 0) {
        return;
    }
    m_obstacleRects.append(rectParent);
    m_windowLoaded = false;
}

void cwCaptureLabelPlacer::finalize()
{
    m_stats.gridCells = qint64(m_maskW) * qint64(m_maskH);
    // The distance transform is built lazily per placement window (see
    // ensureWindowFor); finalize just marks the obstacle sources complete.
    m_finalized = true;
}

void cwCaptureLabelPlacer::clearPlacements()
{
    m_placedLabels.clear();
    m_lineObstacles.clear();
    m_softLineObstacles.clear();
    m_placedLabelGrid.reset();
    m_softObstacleGrid.reset();
    m_lineObstacleGrid.reset();
    m_maxLineObstacleThickness = 0.0;
    m_gridsBuilt = false;
    m_gridCellSizeHint = 0.0;
}

void cwCaptureLabelPlacer::ensureGridsBuilt(const QSizeF& labelSize)
{
    if(m_gridsBuilt) {
        return;
    }

    // Size cells on the scale of a query region — roughly a label. Both queries
    // (placed-label collision and soft-obstacle scoring) probe a region on the
    // order of the label plus its margins, so a cell that size keeps each query
    // to a handful of cells while holding few items per cell. Prefer the batch
    // median from placeAll: the fallback (the first placed request) is biased
    // large because leads — the largest labels — place first, and oversized
    // cells make every station-label query wade through dozens of neighbors.
    // Floor at a small multiple of the distance-transform cell so a degenerate
    // label can't produce an absurdly fine grid.
    const qreal representativeExtent = m_gridCellSizeHint > 0.0
        ? m_gridCellSizeHint
        : qMax(labelSize.width(), labelSize.height());
    const qreal gridCellSize = qMax(representativeExtent,
                                    kMinGridCellDtCells * m_cellSize);

    m_placedLabelGrid = std::make_unique<cwSpatialHashGrid2D>(gridCellSize);
    m_softObstacleGrid = std::make_unique<cwSpatialHashGrid2D>(gridCellSize);
    m_lineObstacleGrid = std::make_unique<cwSpatialHashGrid2D>(gridCellSize);

    // All soft obstacles are registered before the first placement, so the
    // soft grid is complete once built.
    m_softObstacleGrid->reserve(m_softLineObstacles.size());
    for(int i = 0; i < m_softLineObstacles.size(); i++) {
        m_softObstacleGrid->insert(i, m_softLineObstacles.at(i).segment);
    }

    // Defensive: normally nothing is placed yet, but re-index anything that is
    // so the grids mirror m_placedLabels / m_lineObstacles exactly.
    for(int i = 0; i < m_placedLabels.size(); i++) {
        m_placedLabelGrid->insert(i, m_placedLabels.at(i));
    }
    for(int i = 0; i < m_lineObstacles.size(); i++) {
        m_lineObstacleGrid->insert(i, m_lineObstacles.at(i).segment);
    }

    m_gridsBuilt = true;
}

int cwCaptureLabelPlacer::windowCellSpan() const
{
    if(!m_windowSize.has_value()) {
        // Default: a fixed span in cells, scale-independent (see the constant).
        return kDefaultPlacementWindowCells;
    }
    if(*m_windowSize <= 0.0) {
        // Windowing disabled: one window spans the whole page.
        return qMax(1, qMax(m_maskW, m_maskH));
    }
    return qMax(1, qCeil(*m_windowSize / m_cellSize));
}

int cwCaptureLabelPlacer::spiralReachCells(const QSizeF& labelSize) const
{
    // Mirror of placeLabel's spiral geometry. The farthest cell the spiral can
    // sample is maxRing coarse rings out, and each ring's offset on an axis is
    // ring * candidateStep on that axis — so on the coarser axis the reach
    // overshoots the fine-cell radius by up to maxStep/minStep. The DT read at
    // any sampled cell must additionally be exact up to the label's required
    // clearance, so obstacles that far beyond the sample must be ingested too.
    const qreal halfDiag = std::hypot(labelSize.width() * 0.5,
                                      labelSize.height() * 0.5);
    const qreal requiredClearanceParent = halfDiag + m_labelMargin;
    const int cappedRadius = qCeil(requiredClearanceParent * kMaxSearchClearanceMultiple
                                   / m_cellSize) + 1;
    const int maxRadius = qMin(m_maskW + m_maskH, cappedRadius);

    const int stepX = candidateStepForExtent(labelSize.width(), m_cellSize);
    const int stepY = candidateStepForExtent(labelSize.height(), m_cellSize);
    const int minStep = qMin(stepX, stepY);
    const int maxRing = (maxRadius + minStep - 1) / minStep;
    const int spiralCells = maxRing * qMax(stepX, stepY);

    const int clearanceCells = qCeil(requiredClearanceParent / m_cellSize);
    return spiralCells + clearanceCells + 1;
}

void cwCaptureLabelPlacer::ensureWindowFor(int anchorCellX, int anchorCellY, int reachCells)
{
    const int span = windowCellSpan();
    const int maxCol = qMax(0, (m_maskW - 1) / span);
    const int maxRow = qMax(0, (m_maskH - 1) / span);
    const int col = qBound(0, anchorCellX / span, maxCol);
    const int row = qBound(0, anchorCellY / span, maxRow);

    // The window is its core cell range plus a halo covering the spiral reach
    // (and, during placeAll, the whole batch's largest reach, so one build
    // serves every label routed to this window).
    const int halo = qMax(reachCells, m_batchHaloCells);
    const int needX0 = qBound(0, col * span - halo,       m_maskW);
    const int needX1 = qBound(0, (col + 1) * span + halo, m_maskW);
    const int needY0 = qBound(0, row * span - halo,       m_maskH);
    const int needY1 = qBound(0, (row + 1) * span + halo, m_maskH);

    if(m_windowLoaded
       && needX0 >= m_dtX0 && needX1 <= m_dtX1
       && needY0 >= m_dtY0 && needY1 <= m_dtY1) {
        return;
    }
    buildWindow(needX0, needY0, needX1, needY1);
}

void cwCaptureLabelPlacer::buildWindow(int x0, int y0, int x1, int y1)
{
    m_dtX0 = x0;
    m_dtY0 = y0;
    m_dtX1 = x1;
    m_dtY1 = y1;

    const int w = x1 - x0;
    const int h = y1 - y0;
    Q_ASSERT(w > 0 && h > 0);

    // fill() only reallocates when the window outgrows the vector's capacity,
    // so consecutive same-shaped windows reuse the buffer.
    m_dt.fill(kLargeSquaredDist, w * h);

    for(TileAlphaSource& tile : m_tileSources) {
        // Bbox rejection first: tiles that never intersect a built window are
        // never scanned or decompressed at all (zoomed exports build only the
        // viewport's windows).
        if(tile.cellX1 <= x0 || tile.cellX0 >= x1
           || tile.cellY1 <= y0 || tile.cellY0 >= y1) {
            continue;
        }
        ensureTileMask(tile);
        rasterizeTileMask(tile);
    }
    for(const QRectF& rect : std::as_const(m_obstacleRects)) {
        rasterizeObstacleRect(rect);
    }

    distanceTransform2D(m_dt, w, h);
    m_windowLoaded = true;

    const qint64 cells = qint64(w) * qint64(h);
    m_stats.windowsBuilt++;
    m_stats.windowCellsBuilt += cells;
    m_stats.maxWindowCells = qMax(m_stats.maxWindowCells, cells);
}

void cwCaptureLabelPlacer::ensureTileMask(TileAlphaSource& tile)
{
    if(tile.maskBuilt) {
        return;
    }
    tile.maskBuilt = true;

    const int maskW = tile.cellX1 - tile.cellX0;
    const int maskH = tile.cellY1 - tile.cellY0;

    // Materialize a pixel view: the raw handle when present, otherwise the
    // decompressed image (decompress() warns and returns null on a corrupt
    // payload). Both are scoped to this scan; only the (compressed) cell mask
    // survives it.
    QImage view = tile.image;
    if(view.isNull() && !tile.compressed.isNull()) {
        view = tile.compressed.decompress();
    }
    if(view.format() != QImage::Format_ARGB32
       && view.format() != QImage::Format_ARGB32_Premultiplied
       && !view.isNull()) {
        // Foreign format on the compressed path (addTileAlpha normalizes the
        // raw path on record): convert so the alpha scan can read QRgb rows.
        view = view.convertToFormat(QImage::Format_ARGB32);
    }

    if(view.isNull() || maskW <= 0 || maskH <= 0) {
        tile.image = QImage();
        tile.compressed = cwCompressedImage();
        return;
    }

    QByteArray bits((qsizetype(maskW) * maskH + 7) / 8, '\0');
    uchar* bitData = reinterpret_cast<uchar*>(bits.data());

    // Local copies of every loop invariant: the store through bitData (a
    // uchar*, which aliases everything) would otherwise force the compiler to
    // reload the members each iteration of this every-pixel-of-every-tile scan.
    const qreal invCell = 1.0 / m_cellSize;
    const qreal boundsLeft = m_bounds.left();
    const qreal boundsTop  = m_bounds.top();
    const int alphaThreshold = m_alphaThreshold;
    const int cellX0 = tile.cellX0;
    const int cellX1 = tile.cellX1;
    const int cellY0 = tile.cellY0;
    const int cellY1 = tile.cellY1;
    const qreal posX = tile.pos.x();
    const qreal posY = tile.pos.y();
    const qreal scale = tile.scale;
    const int iw = view.width();
    const int ih = view.height();
    for(int py = 0; py < ih; py++) {
        const qreal parentY = posY + py * scale;
        const int cellY = qFloor((parentY - boundsTop) * invCell);
        if(cellY < cellY0 || cellY >= cellY1) {
            continue;
        }
        const QRgb* row = reinterpret_cast<const QRgb*>(view.constScanLine(py));
        const qint64 rowBase = qint64(cellY - cellY0) * maskW;
        for(int px = 0; px < iw; px++) {
            if(qAlpha(row[px]) <= alphaThreshold) {
                continue;
            }
            const qreal parentX = posX + px * scale;
            const int cellX = qFloor((parentX - boundsLeft) * invCell);
            if(cellX < cellX0 || cellX >= cellX1) {
                continue;
            }
            const qint64 bit = rowBase + (cellX - cellX0);
            bitData[bit >> 3] |= uchar(1u << (bit & 7));
        }
    }

    tile.maskBits = qCompress(bits, kMaskCompressionLevel);

    // The mask replaces the pixels; release both sources (implicitly shared —
    // an export scene item may still hold the compressed blob for painting).
    tile.image = QImage();
    tile.compressed = cwCompressedImage();

    m_stats.tileMasksBuilt++;
    m_stats.tileMaskBytes += tile.maskBits.size();
}

void cwCaptureLabelPlacer::rasterizeTileMask(const TileAlphaSource& tile)
{
    // Window ∩ tile bbox, in global cell coords.
    const int x0 = qMax(m_dtX0, tile.cellX0);
    const int x1 = qMin(m_dtX1, tile.cellX1);
    const int y0 = qMax(m_dtY0, tile.cellY0);
    const int y1 = qMin(m_dtY1, tile.cellY1);
    if(x0 >= x1 || y0 >= y1 || tile.maskBits.isEmpty()) {
        return;
    }

    const int maskW = tile.cellX1 - tile.cellX0;
    const int maskH = tile.cellY1 - tile.cellY0;
    const QByteArray bits = qUncompress(tile.maskBits);
    if(bits.size() != (qsizetype(maskW) * maskH + 7) / 8) {
        return;
    }
    const uchar* bitData = reinterpret_cast<const uchar*>(bits.constData());
    // Write through a raw pointer: QVector::operator[] re-checks for a detach
    // on every store, and this loop runs over every window ∩ tile cell.
    float* dt = m_dt.data();

    const int dtW = m_dtX1 - m_dtX0;
    for(int y = y0; y < y1; y++) {
        const qint64 rowBase = qint64(y - tile.cellY0) * maskW;
        const int dtRow = (y - m_dtY0) * dtW;
        for(int x = x0; x < x1; x++) {
            const qint64 bit = rowBase + (x - tile.cellX0);
            const uchar byte = bitData[bit >> 3];
            if(byte == 0) {
                // Masks are sparse: skip the rest of this all-clear byte.
                x += 7 - int(bit & 7);
                continue;
            }
            if(byte & (1u << (bit & 7))) {
                dt[dtRow + (x - m_dtX0)] = 0.0f;
            }
        }
    }
}

void cwCaptureLabelPlacer::rasterizeObstacleRect(const QRectF& rectParent)
{
    const int dtW = m_dtX1 - m_dtX0;
    const qreal invCell = 1.0 / m_cellSize;
    const int x0 = qBound(m_dtX0, qFloor((rectParent.left()   - m_bounds.left()) * invCell), m_dtX1);
    const int y0 = qBound(m_dtY0, qFloor((rectParent.top()    - m_bounds.top())  * invCell), m_dtY1);
    const int x1 = qBound(m_dtX0, qCeil ((rectParent.right()  - m_bounds.left()) * invCell), m_dtX1);
    const int y1 = qBound(m_dtY0, qCeil ((rectParent.bottom() - m_bounds.top())  * invCell), m_dtY1);
    for(int y = y0; y < y1; y++) {
        const int rowBase = (y - m_dtY0) * dtW;
        for(int x = x0; x < x1; x++) {
            m_dt[rowBase + (x - m_dtX0)] = 0.0f;
        }
    }
}

cwCaptureLabelPlacer::Placement cwCaptureLabelPlacer::placeLabel(const LabelRequest& request)
{
    Placement result;
    m_stats.placeCalls++;

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

    // Bound the spiral so a hopeless anchor gives up in O(label size) rather
    // than spiraling across the whole page. The full-page radius is kept as a
    // hard ceiling for small pages where it is already smaller than the cap.
    const int cappedRadius = qCeil(requiredClearanceParent * kMaxSearchClearanceMultiple
                                   / m_cellSize) + 1;
    const int maxRadius = qMin(m_maskW + m_maskH, cappedRadius);

    // Coarsen candidate spacing per axis (see kCandidateStepLabelFraction).
    // Candidate centers step by candidateStepX/Y cells, but each sampled cell's
    // DT clearance and collision tests remain exact, so this only thins the
    // sampling — it never relaxes a placement's correctness.
    const int candidateStepX = candidateStepForExtent(request.size.width(), m_cellSize);
    const int candidateStepY = candidateStepForExtent(request.size.height(), m_cellSize);
    // Spiral radius in coarse rings: divide by the smaller step so both axes
    // still reach at least the full (capped) fine-cell search radius.
    const int minStep = qMin(candidateStepX, candidateStepY);
    const int maxRing = (maxRadius + minStep - 1) / minStep;

    const QRectF clampRect = m_viewportBounds.isEmpty() ? m_bounds : m_viewportBounds;
    const qreal collisionPad = m_labelMargin;

    // Viewport cull: an anchor outside the export viewport (plus a label-
    // diagonal margin, for anchors sitting right on the edge) cannot be given a
    // readable in-viewport label, so skip it before the spiral search. On a
    // whole-cave export viewportBounds == bounds so nothing is culled; this only
    // bites when exporting a zoomed sub-region. buildLabelRequests applies the
    // same rect (via a conservative size bound) before measuring, so most
    // culled anchors never even reach here.
    const QRectF cullRect = viewportCullRect(clampRect, request.size, m_labelMargin);
    if(!cullRect.contains(request.anchorPos)) {
        m_stats.culledByViewport++;
        return result;
    }

    // Build the broad-phase grids now that a representative label size is known
    // (deferred past the cull so fully-culled exports never build them).
    ensureGridsBuilt(request.size);

    // Make sure the DT window covering this anchor (plus the spiral's reach)
    // is loaded. Deferred past the cull so culled anchors — which on a zoomed
    // export are scattered across the whole page — never trigger a build.
    ensureWindowFor(anchorCellX, anchorCellY, spiralReachCells(request.size));
    const int dtW = m_dtX1 - m_dtX0;

    // Preferred leader direction = DT gradient at the anchor. Points along
    // the direction of fastest growth: perpendicular to nearby passage walls
    // for a mid-passage anchor, along the passage axis at a passage end.
    auto dtAt = [&](int x, int y) -> float {
        x = qBound(m_dtX0, x, m_dtX1 - 1);
        y = qBound(m_dtY0, y, m_dtY1 - 1);
        return m_dt[cellIndex(x - m_dtX0, y - m_dtY0, dtW)];
    };
    QPointF preferredDir(0.0, -1.0);
    if(anchorCellX >= m_dtX0 && anchorCellX < m_dtX1
       && anchorCellY >= m_dtY0 && anchorCellY < m_dtY1) {
        const float gx = dtAt(anchorCellX + 1, anchorCellY) - dtAt(anchorCellX - 1, anchorCellY);
        const float gy = dtAt(anchorCellX, anchorCellY + 1) - dtAt(anchorCellX, anchorCellY - 1);
        const qreal gmag = std::hypot(qreal(gx), qreal(gy));
        if(gmag > 1.0e-6) {
            preferredDir = QPointF(qreal(gx) / gmag, qreal(gy) / gmag);
        }
    }

    struct Candidate {
        QRectF  labelRect;
        QPointF leaderStart;
    };
    QVector<Candidate> candidates;

    auto tryCell = [&](int cx, int cy) -> bool {
        // The window's halo covers the whole spiral, so any cell outside it is
        // also outside the page bounds — the same cells the unwindowed
        // placement rejected here.
        if(cx < m_dtX0 || cy < m_dtY0 || cx >= m_dtX1 || cy >= m_dtY1) {
            return false;
        }
        m_stats.cellsTried++;
        if(m_dt[cellIndex(cx - m_dtX0, cy - m_dtY0, dtW)] < requiredClearanceCells) {
            return false;
        }
        m_stats.dtClearedCells++;

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
        const QPointF candidateLeaderStart = closestPointOnRect(candidate, request.anchorPos);
        const QLineF  candidateLeader(candidateLeaderStart, request.anchorPos);
        // Broad-phase region: the candidate padded by the largest possible
        // per-line expand (covers segmentIntersectsRect for every line) united
        // with the leader's bounds (covers segmentsCross, whose crossing point
        // lies on the leader). Any line that could reject this cell has cells
        // inside this region, so the exact per-line tests below stay identical.
        const qreal maxLineExpand = m_maxLineObstacleThickness * 0.5 + m_labelMargin;
        const QRectF lineQueryRegion =
            candidate.adjusted(-maxLineExpand, -maxLineExpand, maxLineExpand, maxLineExpand)
                .united(QRectF(candidateLeader.p1(), candidateLeader.p2()).normalized());
        const bool hitsLineObstacle =
            m_lineObstacleGrid->anyOf(lineQueryRegion, [&](int lineId) {
                m_stats.lineObstacleChecks++;
                const LineObstacle& line = m_lineObstacles.at(lineId);
                const qreal expand = line.thickness * 0.5 + m_labelMargin;
                const QRectF expandedCandidate = candidate.adjusted(-expand, -expand,
                                                                     expand,  expand);
                if(cwCaptureLabelPlacer::segmentIntersectsRect(line.segment, expandedCandidate)) {
                    return true;
                }
                // The label rect can be clear of the existing leader while the
                // new leader still crosses it. Reject that case so leaders
                // don't visually intersect each other in the export.
                return cwCaptureLabelPlacer::segmentsCross(candidateLeader, line.segment);
            });
        if(hitsLineObstacle) {
            return false;
        }

        // Avoid previously placed labels. This must be a read-only test that
        // doesn't commit, since later candidates may score better than this one.
        // The spatial hash reduces this from an O(placed) scan per candidate to
        // the few placed labels whose cells overlap the candidate; the exact
        // intersects() test below is identical to the old brute-force one, so
        // placement output is unchanged (see LABEL_PLACER_SCALING_PLAN).
        const QRectF collisionRect = candidate.adjusted(-collisionPad, -collisionPad,
                                                         collisionPad,  collisionPad);
        const bool collidesWithPlaced =
            m_placedLabelGrid->anyOf(collisionRect, [&](int placedId) {
                m_stats.placedLabelChecks++;
                return m_placedLabels.at(placedId).intersects(collisionRect);
            });
        if(collidesWithPlaced) {
            return false;
        }

        candidates.append({candidate, candidateLeaderStart});
        return true;
    };

    int firstHitRadius = -1;
    auto windowExhausted = [&](int r) {
        return firstHitRadius >= 0 && r > firstHitRadius + kCandidateWindow;
    };

    tryCell(anchorCellX, anchorCellY);
    if(!candidates.isEmpty()) {
        firstHitRadius = 0;
    }

    for(int r = 1; r <= maxRing; r++) {
        if(windowExhausted(r)) {
            break;
        }
        const int offX = r * candidateStepX;
        const int offY = r * candidateStepY;
        for(int dx = -r; dx <= r; dx++) {
            const int cx = anchorCellX + dx * candidateStepX;
            tryCell(cx, anchorCellY - offY);
            tryCell(cx, anchorCellY + offY);
        }
        for(int dy = -r + 1; dy <= r - 1; dy++) {
            const int cy = anchorCellY + dy * candidateStepY;
            tryCell(anchorCellX - offX, cy);
            tryCell(anchorCellX + offX, cy);
        }
        if(firstHitRadius < 0 && !candidates.isEmpty()) {
            firstHitRadius = r;
        }
    }

    if(candidates.isEmpty()) {
        m_stats.noCandidate++;
        return result;
    }

    auto scoreCandidate = [&](const Candidate& c) -> qreal {
        int softCrossings = 0;
        const QLineF leader(c.leaderStart, request.anchorPos);
        // Only soft obstacles whose cells overlap the leader's bounds can cross
        // it. query() dedups, so each obstacle is counted at most once — the
        // count matches the old full scan exactly (a genuine crossing point
        // lies on the leader, so its cell is always in range).
        const QRectF leaderBounds = QRectF(leader.p1(), leader.p2()).normalized();
        m_softObstacleGrid->query(leaderBounds, [&](int softId) {
            m_stats.softObstacleChecks++;
            if(cwCaptureLabelPlacer::segmentsCross(leader, m_softLineObstacles.at(softId).segment)) {
                softCrossings++;
            }
        });

        const QPointF delta = c.labelRect.center() - request.anchorPos;
        const qreal   mag   = std::hypot(delta.x(), delta.y());
        qreal directionDeviation = 0.0;
        if(mag > 1.0e-6) {
            const qreal dot = (delta.x() * preferredDir.x()
                             + delta.y() * preferredDir.y()) / mag;
            directionDeviation = std::acos(qBound(qreal(-1.0), dot, qreal(1.0)));
        }

        return mag
             + kSoftCrossPenalty * qreal(softCrossings)
             + kDirectionPenalty * directionDeviation;
    };

    int     bestIndex = 0;
    qreal   bestScore = scoreCandidate(candidates[0]);
    for(int i = 1; i < candidates.size(); i++) {
        const qreal s = scoreCandidate(candidates[i]);
        if(s < bestScore) {
            bestScore = s;
            bestIndex = i;
        }
    }

    const Candidate& winner = candidates[bestIndex];
    const QRectF committed = winner.labelRect.adjusted(-collisionPad, -collisionPad,
                                                         collisionPad,  collisionPad);
    m_placedLabelGrid->insert(m_placedLabels.size(), committed);
    m_placedLabels.append(committed);

    m_stats.placed++;
    result.placed      = true;
    result.labelRect   = winner.labelRect;
    result.leaderEnd   = request.anchorPos;
    result.leaderStart = winner.leaderStart;
    return result;
}

QVector<cwCaptureLabelPlacer::Placement> cwCaptureLabelPlacer::placeAll(
    const QVector<LabelRequest>& requests,
    const cwLabelPlacementControl& control)
{
    QVector<Placement> results(requests.size());
    if(requests.isEmpty() || m_maskW <= 0 || m_maskH <= 0) {
        return results;
    }

    // Window-major processing order (stable, so a single-window page places in
    // exactly the caller's order). Collision state is global, so reordering
    // across windows can shift which of two competing labels wins a spot, but
    // can never let placements overlap.
    const int span = windowCellSpan();
    const int windowCols = qMax(1, (m_maskW + span - 1) / span);
    const int maxCol = qMax(0, (m_maskW - 1) / span);
    const int maxRow = qMax(0, (m_maskH - 1) / span);
    auto windowIndexFor = [&](const QPointF& anchor) {
        const int cellX = qFloor((anchor.x() - m_bounds.left()) / m_cellSize);
        const int cellY = qFloor((anchor.y() - m_bounds.top())  / m_cellSize);
        const int col = qBound(0, cellX / span, maxCol);
        const int row = qBound(0, cellY / span, maxRow);
        return row * windowCols + col;
    };
    QVector<int> windowOfRequest(requests.size());
    for(int i = 0; i < requests.size(); i++) {
        windowOfRequest[i] = windowIndexFor(requests.at(i).anchorPos);
    }
    QVector<int> order(requests.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return windowOfRequest.at(a) < windowOfRequest.at(b);
    });

    // One halo per WINDOW — the largest spiral reach among the requests routed
    // to it — so each window is built once even though label sizes vary. Per
    // window rather than per batch: a single long lead label's reach is many
    // times a station's, and a batch-wide halo made every window on a
    // whole-cave export mostly halo (6x the page area of DT built, measured).
    QHash<int, int> windowHaloCells;
    QVector<qreal> labelExtents;
    labelExtents.reserve(requests.size());
    for(int i = 0; i < requests.size(); i++) {
        const QSizeF size = requests.at(i).size;
        if(size.width() <= 0.0 || size.height() <= 0.0) {
            continue;
        }
        int& halo = windowHaloCells[windowOfRequest.at(i)];
        halo = qMax(halo, spiralReachCells(size));
        labelExtents.append(qMax(size.width(), size.height()));
    }

    // Median label extent sizes the broad-phase grid cells (see
    // ensureGridsBuilt) — representative of the many stations rather than the
    // few oversized leads that happen to place first.
    if(!labelExtents.isEmpty()) {
        const int mid = labelExtents.size() / 2;
        std::nth_element(labelExtents.begin(), labelExtents.begin() + mid,
                         labelExtents.end());
        m_gridCellSizeHint = labelExtents.at(mid);
    }

    for(int requestIndex : std::as_const(order)) {
        // Poll for cancelation and report progress once per label, before its
        // (potentially expensive) placement, so a canceled export bails
        // promptly and the progress count covers skipped labels too.
        if(control.isCanceled && control.isCanceled()) {
            break;
        }
        if(control.labelProcessed) {
            control.labelProcessed();
        }

        const LabelRequest& request = requests.at(requestIndex);
        m_batchHaloCells = windowHaloCells.value(windowOfRequest.at(requestIndex), 0);
        Placement placement = placeLabel(request);
        if(placement.placed && request.leaderObstacleThickness > 0.0) {
            const QLineF leader(placement.leaderStart, placement.leaderEnd);
            if(leader.length() >= request.leaderMinLength) {
                // Register the leader so every later placement — in this
                // window or any other — avoids sitting on the drawn line.
                placement.hasLeader = true;
                addLineObstacle(leader, request.leaderObstacleThickness);
            }
        }
        results[requestIndex] = placement;
    }
    m_batchHaloCells = 0;

    return results;
}

void cwCaptureLabelPlacer::addLineObstacle(const QLineF& segment, qreal thicknessPaperPx)
{
    if(segment.p1() == segment.p2()) {
        return;
    }
    m_maxLineObstacleThickness = qMax(m_maxLineObstacleThickness, thicknessPaperPx);
    const int index = m_lineObstacles.size();
    m_lineObstacles.append({segment, thicknessPaperPx});
    // Leaders are registered between placements (after the grids are built), so
    // index incrementally. If a leader is somehow added before the first
    // placeLabel(), ensureGridsBuilt() indexes the backlog instead.
    if(m_lineObstacleGrid) {
        m_lineObstacleGrid->insert(index, segment);
    }
}

void cwCaptureLabelPlacer::addSoftLineObstacle(const QLineF& segment, qreal thicknessPaperPx)
{
    if(segment.p1() == segment.p2()) {
        return;
    }
    m_softLineObstacles.append({segment, thicknessPaperPx});
}
