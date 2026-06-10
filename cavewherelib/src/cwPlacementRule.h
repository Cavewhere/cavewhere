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
    double worldPerPaperMm = 0.0;    // paper-mm -> world-m at the active map scale
    cwSceneContext *scene = nullptr; // iter-2 MutateScene state; nullptr in iter 1
};

// What a Stamp rule needs to turn one placed position into world-metre ink. The
// glyph is tessellated once by cwGlyphTessellationCache and passed in, so the
// rule never touches the cache and stays cheap to re-invoke.
struct cwStampGeometry {
    const QPainterPath &glyphPath;   // unplaced world-metre glyph ink
    const cwStrokePath &strokePath;  // for arclength-warping stamps
};

// One executable placement rule. Decoration layers store rules as data
// (cwPlacementRuleData: name + params); cwPlacementRuleRegistry maps each name
// to one of these singletons, which cwSketchDecorationLayout runs in stage order.
// Rules are stateless and const — the registry owns exactly one instance of each.
class CAVEWHERE_LIB_EXPORT cwPlacementRule {
public:
    enum Stage {
        Generate = 0,        // seed candidate positions from the stroke path
        MutatePerLayer = 1,  // adjust rotation / scale / visibility per layer
        Terminal = 2,        // produce the layer's geometry; ends the per-layer pipeline
        MutateScene = 3,     // global collision arbitration (iter 2)
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
    // "no ink" suits every other rule.
    virtual QPainterPath stampPath(const cwStampPosition &position,
                                   const cwStampGeometry &geometry) const
    {
        Q_UNUSED(position)
        Q_UNUSED(geometry)
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
};

#endif // CWPLACEMENTRULE_H
