/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTAMPPOSITION_H
#define CWSTAMPPOSITION_H

//Qt includes
#include <QMetaType>
#include <QPointF>
#include <QRectF>
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"

// One candidate glyph placement flowing through a decoration layer's per-layer
// placement-rule pipeline (Generate -> MutatePerLayer -> Terminal). Generate
// rules seed positions along the stroke; MutatePerLayer rules set rotation /
// scale / visibility; a Stamps-kind Terminal rule finalises them and
// cwSketchDecorationLayout materialises each visible position into world-metre
// ink. All coordinates are world metres. A polyline-tracing layer doesn't use
// this — its terminal produces a polyline, not stamps.
// Members are ordered widest-first (8-byte-aligned types, then the ints/bool) so
// the layout packs without internal padding holes — not by logical grouping.
struct CAVEWHERE_LIB_EXPORT cwStampPosition {
    QRectF  bufferedBboxWorld;      // ink bbox inflated by layer.bufferMm; populated at materialise time
    QPointF anchorWorld;            // where glyph (0,0) sits
    QString glyphName;              // usually layer.glyphName; rules may swap it
    double  rotationRad = 0.0;      // 0 = glyph +X along world +X
    double  scale = 1.0;            // 1.0 = glyph's authored size
    int     side = 1;               // +1 / -1, for mirror-aware rules
    int     priority = 0;           // copied from layer.collisionPriority by the Terminal rule
    bool    visible = true;

    bool operator==(const cwStampPosition &o) const = default;
};

Q_DECLARE_METATYPE(cwStampPosition)

#endif // CWSTAMPPOSITION_H
