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
// for Bending, the warp.
class cwStampRuleBase : public cwPlacementRule {
public:
    Stage stage() const override { return Terminal; }
    OutputKind outputKind() const override { return OutputKind::Stamps; }
    void apply(QVector<cwStampPosition> &positions,
               const cwPlacementContext &context) const override;

    // Rigid placement: scale, rotate, translate the glyph at the anchor. Rigid
    // stamps use it as-is; Bending overrides with an arclength warp.
    QPainterPath stampPath(const cwStampPosition &position,
                           const cwStampGeometry &geometry) const override;
};

#endif // CWSTAMPRULEBASE_H
