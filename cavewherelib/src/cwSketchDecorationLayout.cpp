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
#include <QPainterPath>
#include <QRectF>

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
// rules; unknown names are dropped silently — the validator
// (cwDecorationLayerValidator, run at load/edit) already reports UnknownRule into
// the error model, so the render path trusts a pre-validated palette and does not
// re-report per frame. A layer with no rules resolves to nothing and draws nothing
// — there is no mode-derived default.
QVector<ResolvedRule> resolveRuleStack(const cwDecorationLayer &layer)
{
    const cwPlacementRuleRegistry &registry = cwPlacementRuleRegistry::instance();

    QVector<ResolvedRule> stack;
    stack.reserve(layer.rules.size());
    for (const cwPlacementRuleData &ruleData : layer.rules) {
        const cwPlacementRule *rule = registry.rule(ruleData.name);
        if (rule == nullptr) {
            continue;   // unknown rule already reported by the validator at load/edit
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

        // Trace: one traced path per region. Each carries the layer's pen; if the
        // layer carries a fill colour the region is a Polygon (the fill closes an
        // open path's implicit edge, which the pen leaves undrawn), otherwise a
        // bare Stroke. addPolygon leaves the path open either way.
        if (terminal.rule->outputKind() == cwPlacementRule::OutputKind::Trace) {
            out.kind = cwBrushDecorationGeometry::Layer::Trace;
            const QVector<QPolygonF> regions = terminal.rule->tracePolylines(layerStroke, terminalCtx);
            const bool filled = layer.fillColorLight.isValid();
            out.traced.reserve(regions.size());
            for (const QPolygonF &region : regions) {
                cwGlyphSubPath traced;
                traced.kind = filled ? cwGlyphSubPath::Polygon : cwGlyphSubPath::Stroke;
                traced.path.addPolygon(region);
                traced.penColorLight = layer.lineColorLight;
                traced.penColorDark = layer.lineColorDark;
                traced.penWidthMm = layer.lineWidthMm;
                if (filled) {
                    traced.fillColorLight = layer.fillColorLight;
                    traced.fillColorDark = layer.fillColorDark;
                }
                out.traced.append(traced);
            }
            geometry.layers.append(out);
            continue;
        }

        // Stamps: the layout owns the tessellation cache + bbox derivation; the
        // terminal owns the per-glyph transform (stampPath), applied to each of
        // the glyph's typed sub-paths so per-sub-path pen/fill rides through.
        out.kind = cwBrushDecorationGeometry::Layer::Stamps;
        const double bufferWorld = layer.bufferMm * worldPerPaperMm;
        for (cwStampPosition &position : positions) {
            if (!position.visible) {
                continue;
            }
            const QVector<cwGlyphSubPath> glyphSubPaths =
                m_tessellationCache->tessellate(position.glyphName, mapScale);
            if (glyphSubPaths.isEmpty()) {
                continue;
            }

            cwResolvedStamp resolved;
            resolved.subPaths.reserve(glyphSubPaths.size());
            QRectF bbox;
            for (const cwGlyphSubPath &glyphSub : glyphSubPaths) {
                cwGlyphSubPath placed = glyphSub;
                placed.path = terminal.rule->stampPath(position, glyphSub.path, terminalCtx);
                bbox = bbox.united(placed.path.boundingRect());
                resolved.subPaths.append(placed);
            }
            position.bufferedBboxWorld =
                bbox.adjusted(-bufferWorld, -bufferWorld, bufferWorld, bufferWorld);
            resolved.position = position;
            out.stamps.append(resolved);
        }

        geometry.layers.append(out);
    }

    return geometry;
}
