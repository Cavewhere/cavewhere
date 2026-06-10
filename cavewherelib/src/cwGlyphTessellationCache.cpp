/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwGlyphTessellationCache.h"
#include "cwLineBrush.h"
#include "cwSketchPainter.h"
#include "cwSymbologyGlyph.h"

namespace {

// A stroke contributes ink only if its brush carries at least one decoration
// layer. A missing brush draws the debug fallback line, so it contributes too; a
// brush with no decoration layers (e.g. scrap-outline) draws nothing.
bool strokeHasInk(const cwPaletteSnapshot &snapshot, const cwPenStroke &stroke)
{
    const auto brush = snapshot.findBrush(stroke.brushName);
    if (!brush) {
        return true;
    }
    return !brush->decorations.isEmpty();
}

} // namespace

double cwGlyphTessellationCache::paperMmToWorldM(double mapScale)
{
    constexpr double kMmPerMeter = 1000.0;
    const double ratio =
        mapScale > 0.0 ? mapScale : cwSketchPainter::LinePlotReferenceMapScaleRatio;
    return 1.0 / (kMmPerMeter * ratio);
}

void cwGlyphTessellationCache::setSnapshot(const cwPaletteSnapshot &snapshot)
{
    m_snapshot = snapshot;
    m_cache.clear();
}

QPainterPath cwGlyphTessellationCache::tessellate(const QString &glyphName, double mapScale)
{
    const Key key{glyphName, mapScale};
    const auto cached = m_cache.constFind(key);
    if (cached != m_cache.constEnd()) {
        return *cached;
    }

    const auto glyph = m_snapshot.findGlyph(glyphName);
    if (!glyph) {
        return QPainterPath();
    }

    const double scale = paperMmToWorldM(mapScale);

    QPainterPath path;
    for (const auto &stroke : glyph->strokes) {
        if (stroke.points.isEmpty() || !strokeHasInk(m_snapshot, stroke)) {
            continue;
        }
        const QPointF first = stroke.points.first().position * scale;
        path.moveTo(first);
        for (int i = 1; i < stroke.points.size(); ++i) {
            path.lineTo(stroke.points.at(i).position * scale);
        }
    }

    m_cache.insert(key, path);
    return path;
}

void cwGlyphTessellationCache::clear()
{
    m_cache.clear();
}

void cwGlyphTessellationCache::invalidateScale(double mapScale)
{
    invalidateIf([mapScale](const Key &key) { return key.mapScale == mapScale; });
}

void cwGlyphTessellationCache::invalidateGlyph(const QString &glyphName)
{
    invalidateIf([&glyphName](const Key &key) { return key.glyphName == glyphName; });
}
