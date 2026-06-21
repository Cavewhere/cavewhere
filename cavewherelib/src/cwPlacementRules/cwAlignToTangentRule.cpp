/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwAlignToTangentRule.h"
#include "cwStrokePath.h"

//Std includes
#include <cmath>

QString cwAlignToTangentRule::displayName() const
{
    return QStringLiteral("Align to tangent");
}

QString cwAlignToTangentRule::description() const
{
    return QStringLiteral("Turns each glyph to follow the line's direction so it "
                          "rides the curve.");
}

void cwAlignToTangentRule::apply(QVector<cwStampPosition> &positions,
                                 const cwPlacementContext &context) const
{
    const cwStrokePath &geometry = context.strokePath;
    if (geometry.isEmpty()) {
        return;
    }

    for (auto &position : positions) {
        const QPointF tangent = geometry.tangentNearPoint(position.anchorWorld);
        position.rotationRad = std::atan2(tangent.y(), tangent.x());
    }
}
