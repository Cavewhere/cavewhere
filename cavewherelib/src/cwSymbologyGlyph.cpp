/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSymbologyGlyph.h"

QRectF cwSymbologyGlyph::boundingBox() const
{
    QRectF bounds;
    for (const auto &stroke : strokes) {
        bounds = bounds.united(stroke.boundingBox());
    }
    return bounds;
}
