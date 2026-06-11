/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwJointedStampRule.h"
#include "cwStrokePath.h"

QString cwJointedStampRule::displayName() const
{
    return QStringLiteral("Jointed stamp");
}

QPainterPath cwJointedStampRule::stampPath(const cwStampPosition &position,
                                           const QPainterPath &glyphPath,
                                           const cwPlacementContext &context) const
{
    const cwStrokePath &strokePath = context.strokePath;
    const double startArcLength = strokePath.arcLengthNearPoint(position.anchorWorld);

    QPainterPath warped;
    for (int i = 0; i < glyphPath.elementCount(); ++i) {
        const QPainterPath::Element element = glyphPath.elementAt(i);
        const QPointF mapped =
            bendWarp(strokePath, startArcLength, position.scale, QPointF(element.x, element.y));
        if (element.isMoveTo()) {
            warped.moveTo(mapped);
        } else {
            warped.lineTo(mapped);
        }
    }
    return warped;
}
