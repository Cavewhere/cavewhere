/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHDECORATIONLAYOUT_H
#define CWSKETCHDECORATIONLAYOUT_H

//Qt includes
#include <QPointF>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwDecorationLayer.h"
#include "cwGlyphSubPath.h"
#include "cwStampPosition.h"

class cwGlyphTessellationCache;
class cwLineBrush;

// A finalised glyph stamp plus its materialised world-metre ink. `subPaths` are
// the cached glyph's typed sub-paths the layer's terminal rule placed — each
// rigidly transformed (Rigid stamp) or arclength-warped (Bending stamp) — so a
// filled glyph stamp keeps its per-sub-path pen/fill. `position` carries the
// anchor/rotation/priority the pipeline produced (kept for iter-2 scene rules
// and tests).
struct cwResolvedStamp {
    cwStampPosition         position;
    QVector<cwGlyphSubPath> subPaths;   // world metres, placed
};

// One decoration layer's laid-out geometry, in the brush's declared layer order
// (== paint order, Decision 12). `kind` is set from the layer's terminal rule —
// stamps or a traced path, never both. A traced region is a Stroke sub-path, or a
// Polygon sub-path when the layer carries a fill (the fill closes it).
struct cwBrushDecorationGeometry {
    struct Layer {
        enum Kind { Stamps, Trace };
        Kind                     kind = Stamps;
        QVector<cwGlyphSubPath>  traced;   // kind == Trace; one per region (Stroke, or Polygon if filled)
        QVector<cwResolvedStamp> stamps;   // kind == Stamps; visible stamps only
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
//
// CONCURRENCY: layout() is a const transform and is designed to run concurrently
// across strokes/layers (the cwConcurrent::run follow-up; iter 1 is synchronous on
// the UI thread). The injected cache is shared, so before that path is wired the
// cache must be made thread-safe — see the thread-safety note on
// cwGlyphTessellationCache. An unknown rule name reaching the pipeline is dropped
// silently here; it is reported once at load/edit by cwDecorationLayerValidator
// into the error model, so the render path never re-reports.
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
