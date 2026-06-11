/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSTAMPRULEBASE_H
#define CWSTAMPRULEBASE_H

//Our includes
#include "cwPlacementRule.h"

// Shared base for the glyph-stamping Terminal rules (Rigid / Bending). Both
// finalise positions identically — copy the layer's collision priority onto each
// generated position — and both yield Stamps; they differ only in how a single
// position is materialised (stampPath). Subclasses supply the registry name and,
// for Bending, the warp. Glyph paths are pre-flattened moveTo/lineTo polylines
// (cwGlyphTessellationCache), so the stamp rules walk elements and treat every
// non-moveTo as a line — there are no CurveTo elements to subdivide.
class cwStampRuleBase : public cwPlacementRule {
public:
    Stage stage() const override { return Terminal; }
    OutputKind outputKind() const override { return OutputKind::Stamps; }
    void apply(QVector<cwStampPosition> &positions,
               const cwPlacementContext &context) const override;

    // Rigid placement: scale, rotate, translate the glyph at the anchor. Rigid
    // stamps use it as-is; the bending/jointed rules override with an arclength warp.
    QPainterPath stampPath(const cwStampPosition &position,
                           const QPainterPath &glyphPath,
                           const cwPlacementContext &context) const override;

protected:
    // The arclength warp shared by the jointed and bending terminals: a glyph
    // point (x, y) maps to path(startArcLength + scale*x) + scale*y * normal at
    // that arclength. Jointed applies it per vertex; Bending per sub-sample.
    static QPointF bendWarp(const cwStrokePath &strokePath,
                            double startArcLength,
                            double scale,
                            const QPointF &glyphPoint);
};

#endif // CWSTAMPRULEBASE_H
