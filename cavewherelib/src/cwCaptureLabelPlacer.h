/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWCAPTURELABELPLACER_H
#define CWCAPTURELABELPLACER_H

// Qt includes
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QVector>

// Our includes
#include "cwGlobals.h"
#include "cwCollisionRectKdTree.h"

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

private:
    QRectF              m_bounds;
    QRectF              m_viewportBounds;
    qreal               m_cellSize    = 2.0;
    qreal               m_labelMargin = 3.0;
    int                 m_alphaThreshold = 16;
    int                 m_maskW = 0;
    int                 m_maskH = 0;
    QVector<float>      m_dt;           // squared distance during build, sqrt'd in finalize()
    bool                m_finalized = false;
    cwCollisionRectKdTree m_placedLabels;
};

#endif // CWCAPTURELABELPLACER_H
