/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAPTURELABELPLACER_H
#define CWCAPTURELABELPLACER_H

// Qt includes
#include <QFont>
#include <QImage>
#include <QLineF>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

// Std includes
#include <memory>
#include <optional>

// Our includes
#include "cwGlobals.h"
#include "cwCompressedImage.h"
#include "cwLabelPlacementControl.h"
#include "cwSpatialHashGrid2D.h"

class CAVEWHERE_LIB_EXPORT cwCaptureLabelPlacer
{
public:
    // Source-of-truth alpha threshold for "passage ink". Anything strictly
    // above this counts as obstacle. Shared with the SVG overlap test analyzer
    // so both sides agree on what overlap means.
    static constexpr int DefaultAlphaThreshold = 16;

    // Conversion factor for `pointSize * dpi * PointsToPixelsAt72Dpi` to get
    // pixel size, used wherever placement-time fonts are constructed.
    static constexpr qreal PointsToPixelsAt72Dpi = 1.0 / 72.0;

    struct LabelRequest {
        QString text;
        QPointF anchorPos;
        QSizeF  size;
        // placeAll() only: when > 0, a successful placement's leader line is
        // registered as a hard line obstacle of this thickness (so later
        // labels avoid it), provided the leader is at least leaderMinLength
        // long. placeLabel() ignores both fields — its callers register
        // leaders themselves.
        qreal   leaderObstacleThickness = 0.0;
        qreal   leaderMinLength = 0.0;
    };

    struct Placement {
        bool    placed = false;
        QRectF  labelRect;
        QPointF leaderStart;
        QPointF leaderEnd;
        // placeAll() only: true when the leader met the request's
        // leaderMinLength and was registered as a line obstacle.
        bool    hasLeader = false;
    };

    // Lightweight aggregate counters for profiling the placement hot path,
    // read via stats() (opt-in: cwCaptureViewport logs them when the
    // CW_PROFILE_CAPTURE env var is set). Every field is a plain accumulator
    // incremented during placeLabel(), window builds, and tile-mask builds,
    // so they cost nothing beyond an integer add (or one QElapsedTimer read
    // per window build) and reading them is free.
    struct Stats {
        int    placeCalls         = 0; // placeLabel() invocations
        int    placed             = 0; // placements that succeeded
        int    culledByViewport   = 0; // dropped by the viewport cull, pre-spiral
        int    noCandidate        = 0; // spiral finished with no viable cell
        qint64 gridCells          = 0; // logical page size in DT cells (never allocated whole)
        qint64 cellsTried         = 0; // in-bounds spiral cells visited, all calls
        qint64 dtClearedCells     = 0; // cells passing the DT clearance (reach grid queries)
        qint64 placedLabelChecks  = 0; // m_placedLabels comparisons (the O(n^2) scan)
        qint64 softObstacleChecks = 0; // soft-obstacle scoring comparisons
        qint64 lineObstacleChecks = 0; // hard leader-obstacle comparisons
        int    windowsBuilt       = 0; // DT window (re)builds — thrash detector
        qint64 windowCellsBuilt   = 0; // total cells rasterized + transformed
        qint64 maxWindowCells     = 0; // largest single window DT allocation
        int    tileMasksBuilt     = 0; // tiles whose inked-cell mask was materialized
        qint64 tileMaskBytes      = 0; // total compressed mask bytes retained
        qint64 windowRasterMs     = 0; // buildWindow: fill + mask/rect rasterization
        qint64 windowDtMs         = 0; // buildWindow: the distance transform itself
    };

    // Helper: when the placer returns a rect tightly sized to glyph ink and
    // the painter has rendered the same text into a path at origin (with
    // `tightBoundingRect` returning a rect rooted at the baseline), this maps
    // the placer's rect back to the baseline-left point the painter needs.
    static QPointF baselineForGlyphInkRect(const QRectF& inkRect,
                                           const QRectF& tightAtOrigin)
    {
        return QPointF(inkRect.left() - tightAtOrigin.left(),
                       inkRect.top()  - tightAtOrigin.top());
    }

    // Shared between the placer's line-obstacle check and the SVG test
    // analyzer so both apply identical geometry — the test would otherwise
    // silently drift from the placer's semantics.
    static bool segmentIntersectsRect(const QLineF& seg, const QRectF& rect);

    // True when two segments cross at a point interior to both — shared
    // endpoints (T-touch) do not count as a cross. Used by the placer to
    // reject candidate leaders that would cross a previously placed leader,
    // and by the SVG analyzer to flag the same condition post-export. When
    // `outIntersection` is non-null, the intersection point is written
    // through it on a true result, sparing callers a second QLineF::intersects.
    static bool segmentsCross(const QLineF& a, const QLineF& b,
                              QPointF* outIntersection = nullptr);

    // Returns `base` with setPixelSize derived from its pointSizeF and the
    // given DPI. Use to size fonts so QFontMetricsF / QPainterPath::addText
    // return paper-pixel-aligned bounds at the export resolution.
    static QFont scaledFont(const QFont& base, int dpi);

    cwCaptureLabelPlacer();

    void setObstacleBounds(const QRectF& parentBounds, qreal cellSizePaperPx);
    void setViewportBounds(const QRectF& viewportBounds);
    void setLabelMarginPaperPx(qreal margin);
    void setAlphaThreshold(int threshold);

    // Side length of the square DT placement windows, in the same units as
    // the obstacle bounds. The distance transform is only ever built for one
    // window (plus a spiral-reach halo) at a time, bounding placement memory
    // to O(window) instead of O(page) — the whole-cave export otherwise
    // allocates a page-area float grid (hundreds of GB). Values <= 0 or >= the
    // page collapse to a single whole-page window, which reproduces the
    // unwindowed placement exactly. When never called, the window defaults to
    // a fixed span in DT *cells* — cells are anchored to paper px, so the
    // default is scale-independent and correct for preview coordinate spaces
    // too (where one local unit is many paper px).
    void setPlacementWindowSize(qreal windowBoundsUnits);

    void addTileAlpha(const QImage& tileImage,
                      const QPointF& tilePosParent,
                      qreal tileScale);
    // Same obstacle source as addTileAlpha, but fed a zlib-compressed image
    // (see cwCompressedImageItem) so export tiles never have to exist raw on
    // this side. The pixels are decompressed exactly once — the first time a
    // DT window overlapping the tile is built — to derive the tile's
    // inked-cell mask, then dropped.
    void addCompressedTileAlpha(const cwCompressedImage& tileImage,
                                const QPointF& tilePosParent,
                                qreal tileScale);
    void addObstacleRect(const QRectF& rectParent);

    void finalize();

    void clearPlacements();
    Placement placeLabel(const LabelRequest& request);

    // Places every request, batched by DT window so each window's distance
    // transform is built once (placeLabel() alone rebuilds whenever
    // consecutive anchors hop windows). Requests are processed window-major
    // (row-major over the window grid) but keep their relative order within a
    // window, so a single-window page places in exactly the given order.
    // Collision state (placed labels, leaders, soft obstacles) is global
    // across windows, so labels can never overlap across a window seam.
    // Results are returned in request order. The optional control is polled
    // once per request: isCanceled() aborts leaving the remainder unplaced,
    // labelProcessed() ticks progress.
    QVector<Placement> placeAll(const QVector<LabelRequest>& requests,
                                const cwLabelPlacementControl& control = {});

    // Profiling counters accumulated across placeLabel() calls (see Stats).
    const Stats& stats() const { return m_stats; }
    void resetStats() { m_stats = Stats(); }

    // Profiling: cells walked by each broad-phase grid's queries (hash-probe /
    // iteration cost, separate from the exact-test counts in Stats). Zero until
    // the grids are built (first placeLabel).
    qint64 placedGridCellsVisited() const {
        return m_placedLabelGrid ? m_placedLabelGrid->cellsVisited() : 0;
    }
    qint64 lineGridCellsVisited() const {
        return m_lineObstacleGrid ? m_lineObstacleGrid->cellsVisited() : 0;
    }
    qint64 softGridCellsVisited() const {
        return m_softObstacleGrid ? m_softObstacleGrid->cellsVisited() : 0;
    }

    // Registers a line segment as a post-finalize obstacle. Subsequent
    // placeLabel() calls reject candidates whose rect intersects the
    // segment expanded by half the thickness plus the standard label
    // margin. Use for already-drawn leader lines so later labels don't sit
    // on top of them.
    void addLineObstacle(const QLineF& segment, qreal thicknessPaperPx);

    // Registers a line segment as a *soft* obstacle: candidate leaders that
    // cross it incur a scoring penalty during placeLabel(), but are not
    // hard-rejected. Use for centerline legs so leaders prefer routes that
    // don't visually cut across cave passages they could go around.
    void addSoftLineObstacle(const QLineF& segment, qreal thicknessPaperPx);

private:
    struct LineObstacle {
        QLineF segment;
        qreal  thickness;
    };

    // A rendered tile's alpha, recorded by addTileAlpha (raw QImage handle,
    // implicitly shared) or addCompressedTileAlpha (cwCompressedImage). The pixels are
    // read once, lazily: the first DT window build that overlaps the tile's
    // cell bbox scans them into `maskBits` — a qCompress'd bitmap of the
    // tile's inked cells, row-major over its clamped cell bbox — and releases both pixel
    // sources. Later window builds OR the mask straight into the DT, so no
    // tile's pixels are ever decompressed or scanned twice, and tiles no
    // window touches (fully culled regions) are never materialized at all.
    struct TileAlphaSource {
        QImage            image;      // raw source (empty on the compressed path)
        cwCompressedImage compressed; // compressed source (null on the raw path)
        QSize   sizePixels;
        QPointF pos;
        qreal   scale = 1.0;
        // Cell-space bbox of the tile, clamped to the page (computed on add).
        int cellX0 = 0;
        int cellY0 = 0;
        int cellX1 = 0;
        int cellY1 = 0;
        // Lazily built inked-cell bitmap over the bbox (row-major, bit set =
        // cell contains alpha above the threshold), qCompress'd.
        bool       maskBuilt = false;
        QByteArray maskBits;
    };

    QRectF              m_bounds;
    QRectF              m_viewportBounds;
    qreal               m_cellSize    = 2.0;
    qreal               m_labelMargin = 3.0;
    int                 m_alphaThreshold = 16;
    int                 m_maskW = 0;
    int                 m_maskH = 0;
    bool                m_finalized = false;
    QVector<QRectF>       m_placedLabels;
    QVector<LineObstacle> m_lineObstacles;
    QVector<LineObstacle> m_softLineObstacles;
    Stats                 m_stats;

    // Obstacle sources, recorded until a DT window needs them. Only the
    // window's cells are ever rasterized, so memory stays O(sources + window)
    // instead of O(page area).
    QVector<TileAlphaSource> m_tileSources;
    QVector<QRectF>          m_obstacleRects;

    // The active DT window: distance-transform values for the global cell
    // range [m_dtX0, m_dtX1) x [m_dtY0, m_dtY1), stored row-major relative to
    // (m_dtX0, m_dtY0). Covers one placement window plus a halo wide enough
    // that every spiral candidate's clearance test reads an exact value.
    // (Raw distances saturate at the halo edge: an anchor whose nearest ink
    // lies beyond the halo reads the fill plateau, so the gradient bias falls
    // back to the default direction — the clearance pass/fail is never
    // affected, because passing values are by construction within the halo.)
    // Empty = default window span in DT cells (scale-independent); explicit
    // values are in bounds units, <= 0 meaning one whole-page window.
    std::optional<qreal> m_windowSize = std::nullopt;
    QVector<float> m_dt;
    bool           m_windowLoaded = false;
    int            m_dtX0 = 0;
    int            m_dtY0 = 0;
    int            m_dtX1 = 0;
    int            m_dtY1 = 0;
    // Extra halo (in cells) applied to window builds during placeAll: the
    // largest spiral reach among the requests routed to the CURRENT window,
    // so one build serves every label in it — without one oversized label on
    // the far side of the page inflating every other window's halo.
    int            m_batchHaloCells = 0;
    // placeAll's hint for sizing the broad-phase grid cells: the batch's
    // median label extent. Without it the grids are sized from the first
    // placed request — and leads (the largest labels) place first, which on a
    // whole-cave export made every cell hold dozens of station labels.
    qreal          m_gridCellSizeHint = 0.0;

    // Broad-phase accelerators built lazily on the first placeLabel() (once a
    // representative label size is known for cell sizing). m_placedLabelGrid and
    // m_lineObstacleGrid grow as placements commit and leaders are registered;
    // m_softObstacleGrid is static after the build because all soft obstacles
    // are registered before placement begins.
    std::unique_ptr<cwSpatialHashGrid2D> m_placedLabelGrid;
    std::unique_ptr<cwSpatialHashGrid2D> m_softObstacleGrid;
    std::unique_ptr<cwSpatialHashGrid2D> m_lineObstacleGrid;
    bool                                 m_gridsBuilt = false;
    // Largest hard-obstacle thickness seen so far; the line-obstacle query pads
    // the candidate by half of this (plus the label margin) so the broad-phase
    // region conservatively covers every per-line expanded test.
    qreal                                m_maxLineObstacleThickness = 0.0;

    void ensureGridsBuilt(const QSizeF& labelSize);

    int windowCellSpan() const;
    // Spiral reach of a request in cells: the capped search radius plus the
    // clearance the DT must be exact for at any reached cell. Windows are
    // haloed by this so windowed DT reads equal the whole-page values.
    int spiralReachCells(const QSizeF& labelSize) const;
    void ensureWindowFor(int anchorCellX, int anchorCellY, int reachCells);
    void buildWindow(int x0, int y0, int x1, int y1);
    void appendTileSource(TileAlphaSource&& tile);
    void ensureTileMask(TileAlphaSource& tile);
    void rasterizeTileMask(const TileAlphaSource& tile);
    void rasterizeObstacleRect(const QRectF& rectParent);
};

#endif // CWCAPTURELABELPLACER_H
