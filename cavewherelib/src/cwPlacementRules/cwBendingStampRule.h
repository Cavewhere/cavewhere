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

// Terminal rule: each visible position becomes a glyph re-parameterised along
// the stroke-path arclength (a "bent" stamp). Shares cwStampRuleBase's position
// finalisation and Stamps output kind; overrides only the per-position warp.
class cwBendingStampRule : public cwStampRuleBase {
public:
    QString displayName() const override;

    // Re-parameterise the glyph along the stroke-path arclength: glyph (x, y)
    // maps to path(S + scale*x) + scale*y * normal.
    QPainterPath stampPath(const cwStampPosition &position,
                           const cwStampGeometry &geometry) const override;
};

#endif // CWBENDINGSTAMPRULE_H
