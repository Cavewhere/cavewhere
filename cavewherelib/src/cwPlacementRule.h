/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWPLACEMENTRULE_H
#define CWPLACEMENTRULE_H

//Qt includes
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <QVariant>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwDecorationLayer.h"
#include "cwStampPosition.h"

// Holds the unified stamp set + keep-clear bands for the iter-2 MutateScene
// stage. Forward-declared only: iter 1 ships no scene rules and always passes
// nullptr, so the apply() signature is stable when iter 2 lands it.
class cwSceneContext;

// Arclength sampler + nearest-point projector over the stroke's world-metre path.
class cwStrokePath;

// Read-only inputs a per-layer placement rule runs against, bundled so new
// inputs can be added later without changing every rule's apply() signature. The
// mutable stamp vector stays an explicit apply() parameter, keeping the in/out
// split visible at the call site. All geometry is in world metres.
struct cwPlacementContext {
    const cwStrokePath &strokePath;  // built once per stroke; rules share it
    const cwDecorationLayer &layer;
    // paper-mm -> world-m at the active map scale. The layout fills this from
    // cwGlyphTessellationCache::paperMmToWorldM, which clamps the scale and so is
    // always > 0; the 0.0 here is a degenerate default for directly-built contexts,
    // never a live value.
    double worldPerPaperMm = 0.0;
    // This rule invocation's decoded params (proto-free; see
    // cwPlacementRuleParams.h). Null/wrong-typed -> the rule's value<T>()
    // default. The layout rebuilds the context per rule so each gets its own.
    QVariant ruleParameters;
    cwSceneContext *scene = nullptr; // iter-2 MutateScene state; nullptr in iter 1
};

// One executable placement rule. Decoration layers store rules as data
// (cwPlacementRuleData: name + params); cwPlacementRuleRegistry maps each name
// to one of these singletons, which cwSketchDecorationLayout runs in stage order.
// Rules are stateless and const — the registry owns exactly one instance of each.
class CAVEWHERE_LIB_EXPORT cwPlacementRule {
public:
    enum Stage {
        TransformStroke = 0, // rebuild the stroke the rest of the stack runs against (e.g. offset)
        Generate = 1,        // seed candidate positions from the stroke path
        MutatePerLayer = 2,  // adjust rotation / scale / visibility per layer
        Terminal = 3,        // produce the layer's geometry; ends the per-layer pipeline
        MutateScene = 4,     // global collision arbitration (iter 2)
    };

    // What a Terminal rule produces. A layer's output kind (stamps vs a traced
    // polyline) is decided by which Terminal rule its stack ends with — there is
    // no layer "mode". None is the default for non-terminal stages.
    enum class OutputKind { None, Stamps, Polylines };

    virtual ~cwPlacementRule() = default;

    // Also the registry key and the stable palette-schema surface — renaming a
    // registered rule breaks every palette file that references it.
    virtual QString displayName() const = 0;
    virtual Stage stage() const = 0;

    // Per-layer rules read/write `positions` (the in/out stamp vector) using the
    // read-only `context`. Scene-stage rules read only context.scene (nullptr in
    // iter 1).
    virtual void apply(QVector<cwStampPosition> &positions,
                       const cwPlacementContext &context) const = 0;

    // What this rule's terminal materialisation yields. cwSketchDecorationLayout
    // reads it off the stack's Terminal rule to decide how to materialise the
    // layer; non-terminal rules leave it None.
    virtual OutputKind outputKind() const { return OutputKind::None; }

    // Materialise one placed position into world-metre glyph ink (the rule owns
    // its shape — Decision 11a). Overridden by Stamps-kind terminals; the default
    // "no ink" suits every other rule. `glyphPath` is the unplaced world-metre
    // glyph, tessellated once by cwGlyphTessellationCache and passed in so the rule
    // never touches the cache; `context` supplies the stroke path and scale shared
    // by the whole layer.
    virtual QPainterPath stampPath(const cwStampPosition &position,
                                   const QPainterPath &glyphPath,
                                   const cwPlacementContext &context) const
    {
        Q_UNUSED(position)
        Q_UNUSED(glyphPath)
        Q_UNUSED(context)
        return QPainterPath();
    }

    // Produce the layer's traced polylines in world metres. Overridden by
    // Polylines-kind terminals (the offset-curve trace); the default empty result
    // suits every other rule.
    virtual QVector<QPolygonF> tracePolylines(const QVector<QPointF> &strokeWorld,
                                              const cwPlacementContext &context) const
    {
        Q_UNUSED(strokeWorld)
        Q_UNUSED(context)
        return {};
    }

    // Rebuild the world-metre stroke the rest of the layer's stack runs against.
    // Overridden by TransformStroke-stage rules (the lateral offset); the default
    // returns the stroke unchanged. cwSketchDecorationLayout pulls the stack's
    // TransformStroke rule out and applies it first — before Generate — so every
    // downstream rule (stamps and the polyline trace) follows the transformed
    // stroke for free by sampling it.
    virtual QVector<QPointF> transformStroke(const QVector<QPointF> &strokeWorld,
                                             const cwPlacementContext &context) const
    {
        Q_UNUSED(context)
        return strokeWorld;
    }
};

#endif // CWPLACEMENTRULE_H
