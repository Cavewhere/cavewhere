/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwUniformSpacingRule.h"
#include "cwStrokePath.h"
#include "cwPlacementRuleParams.h"

//Std includes
#include <cmath>

QString cwUniformSpacingRule::displayName() const
{
    return cwUniformSpacingRuleName();
}

QString cwUniformSpacingRule::description() const
{
    return QStringLiteral("Drops a glyph at a fixed spacing along the line "
                          "(e.g. floor-step ticks every 2 mm).");
}

void cwUniformSpacingRule::apply(QVector<cwStampPosition> &positions,
                                 const cwPlacementContext &context) const
{
    const cwStrokePath &geometry = context.strokePath;
    if (geometry.isEmpty()) {
        return;
    }

    const auto params = context.ruleParameters.value<cwUniformSpacingParams>();
    const double spacingWorld = params.spacingMm * context.worldPerPaperMm;
    // Reject non-positive AND non-finite spacing: spacingMm is file-driven, so a
    // crafted/corrupt palette could carry +Inf (which slips past `<= 0.0` and
    // makes the loop below never terminate) or NaN. Both -> no stamps.
    if (!std::isfinite(spacingWorld) || spacingWorld <= 0.0) {
        return;
    }

    for (double s = 0.0; s <= geometry.totalLength() + spacingWorld * 1e-6; s += spacingWorld) {
        cwStampPosition position;
        position.anchorWorld = geometry.pointAtArcLength(s);
        position.glyphName = context.layer.glyphName;
        positions.append(position);
    }
}
