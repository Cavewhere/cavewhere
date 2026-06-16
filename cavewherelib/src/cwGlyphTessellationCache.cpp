/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwGlyphTessellationCache.h"
#include "cwLineBrush.h"
#include "cwPlacementRule.h"
#include "cwPlacementRuleRegistry.h"
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

// The terminal rule of a decoration layer's stack, or nullptr if it has none.
const cwPlacementRule *terminalOfLayer(const cwPlacementRuleRegistry &registry,
                                       const cwDecorationLayer &layer)
{
    for (const cwPlacementRuleData &ruleData : layer.rules) {
        const cwPlacementRule *rule = registry.rule(ruleData.name);
        if (rule != nullptr && rule->stage() == cwPlacementRule::Terminal) {
            return rule;
        }
    }
    return nullptr;
}

// Resolve the look a glyph stroke's brush imposes onto `sub`: the kind (Stroke vs
// fillable Polygon) and the colours, read from the brush's first decoration layer
// that has a terminal rule. A missing brush, or one with no terminal layer, keeps
// the default pen-only Stroke (the debug-fallback / outline case).
void resolveStrokeLook(const cwPaletteSnapshot &snapshot,
                       const cwPlacementRuleRegistry &registry,
                       const cwPenStroke &stroke,
                       cwGlyphSubPath &sub)
{
    const auto brush = snapshot.findBrush(stroke.brushName);
    if (!brush) {
        return;
    }
    for (const cwDecorationLayer &layer : brush->decorations) {
        const cwPlacementRule *terminal = terminalOfLayer(registry, layer);
        if (terminal == nullptr) {
            continue;
        }
        sub.penColorLight = layer.lineColorLight;
        sub.penColorDark = layer.lineColorDark;
        sub.penWidthMm = layer.lineWidthMm;
        // A traced layer that carries a fill makes the stroke a fillable Polygon;
        // otherwise it stays a bare Stroke. (Fill is meaningful only on a Trace
        // terminal, so a stamp terminal never fills.)
        if (terminal->outputKind() == cwPlacementRule::OutputKind::Trace
                && layer.fillColorLight.isValid()) {
            sub.kind = cwGlyphSubPath::Polygon;
            sub.fillColorLight = layer.fillColorLight;
            sub.fillColorDark = layer.fillColorDark;
        }
        return;
    }
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

QVector<cwGlyphSubPath> cwGlyphTessellationCache::tessellate(const QString &glyphName, double mapScale)
{
    const Key key{glyphName, mapScale};
    const auto cached = m_cache.constFind(key);
    if (cached != m_cache.constEnd()) {
        return *cached;
    }

    const auto glyph = m_snapshot.findGlyph(glyphName);
    if (!glyph) {
        return {};
    }

    const double scale = paperMmToWorldM(mapScale);
    const cwPlacementRuleRegistry &registry = cwPlacementRuleRegistry::instance();

    QVector<cwGlyphSubPath> subPaths;
    for (const auto &stroke : glyph->strokes) {
        if (stroke.points.isEmpty() || !strokeHasInk(m_snapshot, stroke)) {
            continue;
        }
        QPainterPath path;
        path.moveTo(stroke.points.first().position * scale);
        for (int i = 1; i < stroke.points.size(); ++i) {
            path.lineTo(stroke.points.at(i).position * scale);
        }

        cwGlyphSubPath sub;
        sub.path = path;
        resolveStrokeLook(m_snapshot, registry, stroke, sub);
        subPaths.append(sub);
    }

    // A found glyph is cached even when it produced no ink, so repeat lookups stay
    // O(1); only an unknown glyph (handled above) is left uncached. Returned by
    // value: the cache is intended to be shared across concurrent layout (see the
    // thread-safety note on this class), so handing out a reference into the
    // lazily-mutated QHash would dangle when another thread inserts.
    m_cache.insert(key, subPaths);
    return subPaths;
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
