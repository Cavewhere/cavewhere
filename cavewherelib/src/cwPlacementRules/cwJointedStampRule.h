/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWJOINTEDSTAMPRULE_H
#define CWJOINTEDSTAMPRULE_H

//Our includes
#include "cwStampRuleBase.h"

// Terminal rule: warps only the glyph's vertices onto the stroke-path arclength
// and connects them with straight chords (hinges at the vertices, straight
// between — hence "jointed"). Cheap and crisp; right for short-segment glyphs
// (ticks, chevrons) whose straight arms should stay straight. For a glyph that
// should curve to follow the stroke, use cwBendingStampRule instead. Shares
// cwStampRuleBase's position finalisation and Stamps output kind.
class cwJointedStampRule : public cwStampRuleBase {
public:
    QString displayName() const override;
    QString description() const override;

    // Warp each glyph vertex: glyph (x, y) maps to path(S + scale*x) +
    // scale*y * normal. Edges between warped vertices stay straight.
    QPainterPath stampPath(const cwStampPosition &position,
                           const QPainterPath &glyphPath,
                           const cwPlacementContext &context) const override;
};

#endif // CWJOINTEDSTAMPRULE_H
