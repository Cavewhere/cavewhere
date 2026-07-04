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

// Our includes
#include "cwGlobals.h"
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
    };

    struct Placement {
        bool    placed = false;
        QRectF  labelRect;
        QPointF leaderStart;
        QPointF leaderEnd;
    };

    // Lightweight aggregate counters for profiling the placement hot path,
    // read via stats() (opt-in: cwCaptureViewport logs them when the
    // CW_PROFILE_CAPTURE env var is set). Every field is a plain accumulator
    // incremented during placeLabel(), so they cost nothing beyond an integer
    // add in the inner loops and reading them is free.
    struct Stats {
        int    placeCalls         = 0; // placeLabel() invocations
        int    placed             = 0; // placements that succeeded
        int    culledByViewport   = 0; // dropped by the viewport cull, pre-spiral
        int    noCandidate        = 0; // spiral finished with no viable cell
        qint64 gridCells          = 0; // m_maskW * m_maskH (distance-transform size)
        qint64 cellsTried         = 0; // in-bounds spiral cells visited, all calls
        qint64 dtClearedCells     = 0; // cells passing the DT clearance (reach grid queries)
        qint64 placedLabelChecks  = 0; // m_placedLabels comparisons (the O(n^2) scan)
        qint64 softObstacleChecks = 0; // soft-obstacle scoring comparisons
        qint64 lineObstacleChecks = 0; // hard leader-obstacle comparisons
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

    void addTileAlpha(const QImage& tileImage,
                      const QPointF& tilePosParent,
                      qreal tileScale);
    void addObstacleRect(const QRectF& rectParent);

    void finalize();

    void clearPlacements();
    Placement placeLabel(const LabelRequest& request);

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

    QRectF              m_bounds;
    QRectF              m_viewportBounds;
    qreal               m_cellSize    = 2.0;
    qreal               m_labelMargin = 3.0;
    int                 m_alphaThreshold = 16;
    int                 m_maskW = 0;
    int                 m_maskH = 0;
    QVector<float>      m_dt;           // squared distance during build, sqrt'd in finalize()
    bool                m_finalized = false;
    QVector<QRectF>       m_placedLabels;
    QVector<LineObstacle> m_lineObstacles;
    QVector<LineObstacle> m_softLineObstacles;
    Stats                 m_stats;

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
};

#endif // CWCAPTURELABELPLACER_H
