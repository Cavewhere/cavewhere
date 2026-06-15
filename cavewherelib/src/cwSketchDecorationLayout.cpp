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

// A resolved rule paired with its decoded parameters, kept together so each
// rule's apply() sees its own params (the layout builds a per-rule context).
struct ResolvedRule {
    const cwPlacementRule *rule = nullptr;
    QVariant parameters;
};

// The layer's resolved rule stack in stage order. The stack is the layer's own
// rules (unknown names dropped with a one-shot warning); a layer with no rules
// resolves to nothing and draws nothing — there is no mode-derived default.
QVector<ResolvedRule> resolveRuleStack(const cwDecorationLayer &layer)
{
    const cwPlacementRuleRegistry &registry = cwPlacementRuleRegistry::instance();

    QVector<ResolvedRule> stack;
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
        stack.append(ResolvedRule{rule, ruleData.parameters});
    }

    std::stable_sort(stack.begin(), stack.end(),
                     [](const ResolvedRule &a, const ResolvedRule &b) {
                         return a.rule->stage() < b.rule->stage();
                     });
    return stack;
}

// The Terminal-stage rule in a resolved stack — the rule that produces the
// layer's geometry and decides its output kind (Decision 11a). One per stack.
// Returns nullptr in `out.rule` when the stack has no terminal.
ResolvedRule terminalOf(const QVector<ResolvedRule> &stack)
{
    for (const ResolvedRule &step : stack) {
        if (step.rule->stage() == cwPlacementRule::Terminal) {
            return step;
        }
    }
    return ResolvedRule{};
}

// The TransformStroke-stage rule in a resolved stack — the rule that rebuilds the
// stroke the rest of the stack runs against (the lateral offset). Pulled out and
// applied first, the mirror of pulling the terminal out and applying it last.
// Returns nullptr in `out.rule` when the stack applies no stroke transform.
ResolvedRule transformOf(const QVector<ResolvedRule> &stack)
{
    for (const ResolvedRule &step : stack) {
        if (step.rule->stage() == cwPlacementRule::TransformStroke) {
            return step;
        }
    }
    return ResolvedRule{};
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
    const cwStrokePath baseStrokePath(strokeWorld);

    geometry.layers.reserve(brush.decorations.size());
    for (const cwDecorationLayer &layer : brush.decorations) {
        cwBrushDecorationGeometry::Layer out;

        const QVector<ResolvedRule> stack = resolveRuleStack(layer);

        // TransformStroke stage: rebuild the stroke the rest of the stack runs
        // against (a lateral offset). Applied first, before any position is
        // seeded, so stamps and the polyline trace both follow the offset for
        // free by sampling the transformed stroke. Held in a local the per-layer
        // cwStrokePath borrows for the whole layer.
        const ResolvedRule transform = transformOf(stack);
        QVector<QPointF> layerStroke = strokeWorld;
        if (transform.rule != nullptr) {
            const cwPlacementContext transformContext{baseStrokePath, layer, worldPerPaperMm,
                                                      transform.parameters, nullptr};
            layerStroke = transform.rule->transformStroke(strokeWorld, transformContext);
        }
        if (layerStroke.size() < 2) {
            geometry.layers.append(out);   // the transform collapsed the stroke
            continue;
        }
        const cwStrokePath strokePath(layerStroke);

        QVector<cwStampPosition> positions;
        for (const ResolvedRule &step : stack) {
            const cwPlacementContext context{strokePath, layer, worldPerPaperMm,
                                             step.parameters, nullptr};
            step.rule->apply(positions, context);
        }

        // The terminal rule produces the layer's geometry and names its kind
        // (Decision 11a): a traced polyline, or glyph stamps it materialises
        // from the positions the pipeline placed.
        const ResolvedRule terminal = terminalOf(stack);
        if (terminal.rule == nullptr) {
            geometry.layers.append(out);
            continue;
        }

        const cwPlacementContext terminalCtx{strokePath, layer, worldPerPaperMm,
                                             terminal.parameters, nullptr};

        if (terminal.rule->outputKind() == cwPlacementRule::OutputKind::Polylines) {
            out.kind = cwBrushDecorationGeometry::Layer::Polylines;
            out.offsetPolylines = terminal.rule->tracePolylines(layerStroke, terminalCtx);
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

            const QPainterPath placed = terminal.rule->stampPath(position, glyphPath, terminalCtx);
            position.bufferedBboxWorld =
                placed.boundingRect().adjusted(-bufferWorld, -bufferWorld, bufferWorld, bufferWorld);
            out.stamps.append(cwResolvedStamp{position, placed});
        }

        geometry.layers.append(out);
    }

    return geometry;
}
