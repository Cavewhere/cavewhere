/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWGLYPHTESSELLATIONCACHE_H
#define CWGLYPHTESSELLATIONCACHE_H

//Qt includes
#include <QHash>
#include <QPainterPath>
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPaletteSnapshot.h"

// Materialises a glyph into a single QPainterPath in world metres, cached by
// (glyphName, mapScale) so the glyph→brush→glyph composition runs once per key
// rather than once per stamp (Decision 8). A glyph stamped 200 times along a
// stroke costs 200 draw-time transforms, not 200 tessellations; a per-stamp
// scale (the Taper scale rule) is part of that draw-time transform, so it never
// multiplies the cache.
//
// Iter 1 flattens each glyph stroke whose brush carries an OffsetCurve layer
// (offset 0) into the path, converting glyph-local paper-mm to world metres.
// Stamp-mode recursion (a glyph stroke's brush stamping another glyph) is wired
// once the decoration-layout engine lands; the load-time DAG cycle check
// (cwSymbologyPaletteData::findGlyphDependencyCycle) already guards it.
class CAVEWHERE_LIB_EXPORT cwGlyphTessellationCache {
public:
    cwGlyphTessellationCache() = default;

    // Resolves glyphs and their strokes' brushes. Replacing it clears the cache
    // because a brush edit changes the ink of every glyph that uses it.
    void setSnapshot(const cwPaletteSnapshot &snapshot);
    cwPaletteSnapshot snapshot() const { return m_snapshot; }

    // Glyph as a single world-metre QPainterPath at the given map-scale ratio
    // (e.g. 1/250), anchored at the glyph origin, unrotated, authored size. An
    // unknown glyph returns an empty path and is not cached.
    QPainterPath tessellate(const QString &glyphName, double mapScale);

    // Paper-millimetres to world metres at a given map-scale ratio:
    //   world_m = paper_mm / (1000 * scaleRatio)
    // e.g. at 1:250, 1 mm paper ≈ 0.25 m world. The single source of truth so
    // glyph ink and stamp spacing stay paper-sized across scales (cwSketchPainter::
    // LinePlotReferenceMapScaleRatio is the fallback when mapScale <= 0).
    static double paperMmToWorldM(double mapScale);

    // Invalidation hooks.
    void clear();
    void invalidateScale(double mapScale);       // cwScale::scaleChanged
    void invalidateGlyph(const QString &glyphName);

    int entryCount() const { return m_cache.size(); }

private:
    struct Key {
        QString glyphName;
        double  mapScale = 0.0;
        bool operator==(const Key &o) const = default;
    };
    friend size_t qHash(const Key &key, size_t seed) noexcept
    {
        return qHashMulti(seed, key.glyphName, key.mapScale);
    }

    // Drops every cache entry whose key satisfies `matches`.
    template <typename Predicate>
    void invalidateIf(Predicate matches)
    {
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            if (matches(it.key())) {
                it = m_cache.erase(it);
            } else {
                ++it;
            }
        }
    }

    cwPaletteSnapshot m_snapshot;
    QHash<Key, QPainterPath> m_cache;
};

#endif // CWGLYPHTESSELLATIONCACHE_H
