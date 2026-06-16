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
#include <QString>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwGlyphSubPath.h"
#include "cwPaletteSnapshot.h"

// Materialises a glyph into a list of typed sub-paths in world metres, cached by
// (glyphName, mapScale) so the glyph→brush→glyph composition runs once per key
// rather than once per stamp (Decision 8). A glyph stamped 200 times along a
// stroke costs 200 draw-time transforms, not 200 tessellations; a per-stamp
// scale (the Taper scale rule) is part of that draw-time transform, so it never
// multiplies the cache.
//
// Each inked glyph stroke becomes one cwGlyphSubPath, tagged Stroke or Polygon
// by its brush's terminal rule and carrying that brush's pen (and fill, when the
// Trace brush carries a fill colour) — so a filled glyph (e.g. a stream-indicator
// triangle) stamps as a fillable sub-path rather than a bare outline. Glyph-local paper-mm
// is converted to world metres. Stamp-mode recursion (a glyph stroke's brush
// stamping another glyph) is still future; the load-time DAG cycle check
// (cwSymbologyPaletteData::findGlyphDependencyCycle) already guards it.
//
// THREAD-SAFETY (deferred contract): this cache is NOT yet thread-safe. tessellate()
// lazily inserts into m_cache and invalidateScale()/invalidateGlyph() erase from it,
// all unsynchronised. Iter-1 layout runs synchronously on the UI thread, so that is
// fine today. The decoration layout is *designed* to run concurrently across
// strokes/layers (see cwSketchDecorationLayout) sharing one cache; when that async
// path is wired (the cwConcurrent::run follow-up), this cache must first be made
// safe — guard the mutating entry points with a QReadWriteLock (it is read-mostly),
// or give each worker its own cache. Returns are already by value, so only the
// internal mutation needs guarding, not the handed-out geometry.
class CAVEWHERE_LIB_EXPORT cwGlyphTessellationCache {
public:
    cwGlyphTessellationCache() = default;

    // Resolves glyphs and their strokes' brushes. Replacing it clears the cache
    // because a brush edit changes the ink of every glyph that uses it.
    void setSnapshot(const cwPaletteSnapshot &snapshot);
    cwPaletteSnapshot snapshot() const { return m_snapshot; }

    // Glyph as world-metre typed sub-paths at the given map-scale ratio (e.g.
    // 1/250), anchored at the glyph origin, unrotated, authored size — one entry
    // per inked glyph stroke, carrying the look its brush imposes. Returned by
    // value: the cache is meant to be shared across concurrent layout, so a
    // reference into the lazily-mutated cache would dangle on another thread's
    // insert. An unknown glyph returns an empty list; a found-but-inkless glyph
    // caches an empty one.
    QVector<cwGlyphSubPath> tessellate(const QString &glyphName, double mapScale);

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
    QHash<Key, QVector<cwGlyphSubPath>> m_cache;
};

#endif // CWGLYPHTESSELLATIONCACHE_H
