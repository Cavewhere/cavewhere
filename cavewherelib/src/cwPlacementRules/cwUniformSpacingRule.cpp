/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwUniformSpacingRule.h"
#include "cwStrokePath.h"

namespace {
// Authored 2 mm spacing of the seed floor-step tick. A real per-layer override
// arrives when rule params are interpreted (commit 4.x).
constexpr double kDefaultStampSpacingMm = 2.0;
}

QString cwUniformSpacingRule::displayName() const
{
    return QStringLiteral("Uniform spacing");
}

void cwUniformSpacingRule::apply(QVector<cwStampPosition> &positions,
                                 const cwPlacementContext &context) const
{
    const cwStrokePath &geometry = context.strokePath;
    if (geometry.isEmpty()) {
        return;
    }

    const double spacingWorld = kDefaultStampSpacingMm * context.worldPerPaperMm;
    if (spacingWorld <= 0.0) {
        return;
    }

    for (double s = 0.0; s <= geometry.totalLength() + spacingWorld * 1e-6; s += spacingWorld) {
        cwStampPosition position;
        position.anchorWorld = geometry.pointAtArcLength(s);
        position.glyphName = context.layer.glyphName;
        positions.append(position);
    }
}
