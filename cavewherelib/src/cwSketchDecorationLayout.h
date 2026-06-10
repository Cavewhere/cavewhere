/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHDECORATIONLAYOUT_H
#define CWSKETCHDECORATIONLAYOUT_H

//Qt includes
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwDecorationLayer.h"
#include "cwStampPosition.h"

class cwGlyphTessellationCache;
class cwLineBrush;

// A finalised glyph stamp plus its materialised world-metre ink. `path` is the
// cached glyph path the layer's terminal rule placed — rigidly transformed
// (Rigid stamp) or arclength-warped (Bending stamp); `position` carries the
// anchor/rotation/priority the pipeline produced (kept for iter-2 scene rules
// and tests).
struct cwResolvedStamp {
    cwStampPosition position;
    QPainterPath    path;   // world metres
};

// One decoration layer's laid-out geometry, in the brush's declared layer order
// (== paint order, Decision 12). `kind` is set from the layer's terminal rule —
// stamps or a traced polyline, never both.
struct cwBrushDecorationGeometry {
    struct Layer {
        enum Kind { Stamps, Polylines };
        Kind                     kind = Stamps;
        QVector<QPolygonF>       offsetPolylines; // kind == Polylines (iter 1: offset 0 -> stroke path)
        QVector<cwResolvedStamp> stamps;          // kind == Stamps; visible stamps only
    };
    QVector<Layer> layers;
};

// Runs each decoration layer's placement-rule pipeline (Decision 11) against a
// stroke's path and materialises the result into world-metre draw
// geometry. Stateless except for the injected glyph tessellation cache (which
// owns the active cwPaletteSnapshot); commit 6 shares one cache across the
// painter and this layout. Commit 5 consumes the output.
//
// Iter 1 runs the three per-layer stages (Generate -> MutatePerLayer -> Stamp);
// the global MutateScene stage is iter 2.
class CAVEWHERE_LIB_EXPORT cwSketchDecorationLayout {
public:
    explicit cwSketchDecorationLayout(cwGlyphTessellationCache *tessellationCache);

    cwBrushDecorationGeometry layout(const cwLineBrush &brush,
                                     const QVector<QPointF> &strokeWorld,
                                     double mapScale) const;

private:
    cwGlyphTessellationCache *m_tessellationCache;
};

#endif // CWSKETCHDECORATIONLAYOUT_H
