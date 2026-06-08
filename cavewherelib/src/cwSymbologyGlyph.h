/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYGLYPH_H
#define CWSYMBOLOGYGLYPH_H

//Qt includes
#include <QMetaType>
#include <QRectF>
#include <QString>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPenStroke.h"

// A glyph is a small vector drawing authored in the glyph editor and stored in
// the palette — the only decoration primitive (there is no built-in
// tick/dot/T-bar taxonomy; the cartographer draws them). Implicitly-shared
// value type, same as cwLineBrush, so copying it is a handful of refcount bumps.
//
// Strokes are authored in glyph-local paper-millimetres: origin (0,0) is the
// anchor on the centerline at stamp time and +X is the local tangent. Each
// stroke is a regular cwPenStroke and resolves its ink through the same brush
// name lookup as any sketch stroke — which is what enables recursive glyph
// composition (a stroke's brush can carry decoration layers that stamp other
// glyphs).
struct CAVEWHERE_LIB_EXPORT cwSymbologyGlyph {
    cwSymbologyGlyph() = default;

    QString name;                   // machine id, kebab-case, unique within palette
    QString displayName;            // human-readable label (stored verbatim, not translated)
    QVector<cwPenStroke> strokes;   // glyph-local paper-mm

    // Paper-mm extent, unioned from the strokes' bounding boxes on demand
    // (cheap; mirrors cwPenStroke::boundingBox()). Deliberately not stored or
    // serialized, so there is no derived cache to keep in sync with the strokes.
    QRectF boundingBox() const;

    bool operator==(const cwSymbologyGlyph &o) const = default;
};

Q_DECLARE_METATYPE(cwSymbologyGlyph)

#endif // CWSYMBOLOGYGLYPH_H
