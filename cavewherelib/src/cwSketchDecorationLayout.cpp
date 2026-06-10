/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSketchDecorationLayout.h"
#include "cwStrokePath.h"
#include "cwGlyphTessellationCache.h"
#include "cwLineBrush.h"
#include "cwPlacementRule.h"
#include "cwPlacementRuleRegistry.h"

//Qt includes
#include <QSet>

//Std includes
#include <algorithm>

namespace {

// The layer's resolved rule stack in stage order. The stack is the layer's own
// rules (unknown names dropped with a one-shot warning); a layer with no rules
// resolves to nothing and draws nothing — there is no mode-derived default.
QVector<const cwPlacementRule *> resolveRuleStack(const cwDecorationLayer &layer)
{
    const cwPlacementRuleRegistry &registry = cwPlacementRuleRegistry::instance();

    QVector<const cwPlacementRule *> stack;
    stack.reserve(layer.rules.size());
    for (const cwPlacementRuleData &ruleData : layer.rules) {
        const cwPlacementRule *rule = registry.rule(ruleData.name);
        if (rule == nullptr) {
            // thread_local: layout runs concurrently across strokes/layers, so a
            // shared warned-set would race. One warning per thread is fine.
            thread_local QSet<QString> warned;
            if (!warned.contains(ruleData.name)) {
                warned.insert(ruleData.name);
                qWarning("cwSketchDecorationLayout: unknown placement rule \"%s\" — skipping",
                         qPrintable(ruleData.name));
            }
            continue;
        }
        stack.append(rule);
    }

    std::stable_sort(stack.begin(), stack.end(),
                     [](const cwPlacementRule *a, const cwPlacementRule *b) {
                         return a->stage() < b->stage();
                     });
    return stack;
}

// The Terminal-stage rule in a resolved stack — the rule that produces the
// layer's geometry and decides its output kind (Decision 11a). One per stack.
const cwPlacementRule *terminalOf(const QVector<const cwPlacementRule *> &stack)
{
    for (const cwPlacementRule *rule : stack) {
        if (rule->stage() == cwPlacementRule::Terminal) {
            return rule;
        }
    }
    return nullptr;
}

} // namespace

cwSketchDecorationLayout::cwSketchDecorationLayout(cwGlyphTessellationCache *tessellationCache) :
    m_tessellationCache(tessellationCache)
{
}

cwBrushDecorationGeometry cwSketchDecorationLayout::layout(const cwLineBrush &brush,
                                                          const QVector<QPointF> &strokeWorld,
                                                          double mapScale) const
{
    cwBrushDecorationGeometry geometry;
    if (strokeWorld.size() < 2) {
        return geometry;
    }

    const double worldPerPaperMm = cwGlyphTessellationCache::paperMmToWorldM(mapScale);
    const cwStrokePath strokePath(strokeWorld);

    geometry.layers.reserve(brush.decorations.size());
    for (const cwDecorationLayer &layer : brush.decorations) {
        cwBrushDecorationGeometry::Layer out;

        QVector<cwStampPosition> positions;
        const cwPlacementContext context{strokePath, layer, worldPerPaperMm, nullptr};
        const QVector<const cwPlacementRule *> stack = resolveRuleStack(layer);
        for (const cwPlacementRule *rule : stack) {
            rule->apply(positions, context);
        }

        // The terminal rule produces the layer's geometry and names its kind
        // (Decision 11a): a traced polyline, or glyph stamps it materialises
        // from the positions the pipeline placed.
        const cwPlacementRule *terminal = terminalOf(stack);
        if (terminal == nullptr) {
            geometry.layers.append(out);
            continue;
        }

        if (terminal->outputKind() == cwPlacementRule::OutputKind::Polylines) {
            out.kind = cwBrushDecorationGeometry::Layer::Polylines;
            out.offsetPolylines = terminal->tracePolylines(strokeWorld, context);
            geometry.layers.append(out);
            continue;
        }

        // Stamps: the layout owns the tessellation cache + bbox derivation; the
        // terminal owns the per-glyph transform (stampPath).
        out.kind = cwBrushDecorationGeometry::Layer::Stamps;
        const double bufferWorld = layer.bufferMm * worldPerPaperMm;
        for (cwStampPosition &position : positions) {
            if (!position.visible) {
                continue;
            }
            const QPainterPath glyphPath = m_tessellationCache->tessellate(position.glyphName, mapScale);
            if (glyphPath.isEmpty()) {
                continue;
            }

            const QPainterPath placed = terminal->stampPath(position, cwStampGeometry{glyphPath, strokePath});
            position.bufferedBboxWorld =
                placed.boundingRect().adjusted(-bufferWorld, -bufferWorld, bufferWorld, bufferWorld);
            out.stamps.append(cwResolvedStamp{position, placed});
        }

        geometry.layers.append(out);
    }

    return geometry;
}
