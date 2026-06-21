/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWBENDINGSTAMPRULE_H
#define CWBENDINGSTAMPRULE_H

//Our includes
#include "cwStampRuleBase.h"

// Terminal rule: subdivides each glyph edge along the stroke-path arclength so
// the whole glyph curves to follow the stroke. Right for long features (a bar, a
// long dash, a wavy decoration) that should read as part of the bending line.
// For short glyphs whose straight arms should stay crisp, use cwJointedStampRule
// instead — it warps only the vertices and is cheaper. Shares cwStampRuleBase's
// position finalisation and Stamps output kind.
class cwBendingStampRule : public cwStampRuleBase {
public:
    QString displayName() const override;
    QString description() const override;

    // Warp the glyph onto the stroke arclength, subdividing each edge by its
    // along-arclength span so it follows the stroke's curvature rather than
    // cutting across as a chord.
    QPainterPath stampPath(const cwStampPosition &position,
                           const QPainterPath &glyphPath,
                           const cwPlacementContext &context) const override;
};

#endif // CWBENDINGSTAMPRULE_H
