/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwBendingStampRule.h"
#include "cwStrokePath.h"

QString cwBendingStampRule::displayName() const
{
    return QStringLiteral("Bending stamp");
}

QPainterPath cwBendingStampRule::stampPath(const cwStampPosition &position,
                                           const cwStampGeometry &geometry) const
{
    const cwStrokePath &strokePath = geometry.strokePath;
    const double startArcLength = strokePath.arcLengthNearPoint(position.anchorWorld);

    const auto warp = [&](const QPointF &glyphPoint) {
        const double s = startArcLength + position.scale * glyphPoint.x();
        const double lateral = position.scale * glyphPoint.y();
        return strokePath.pointAtArcLength(s) + lateral * strokePath.normalAtArcLength(s);
    };

    const QPainterPath &glyphPath = geometry.glyphPath;
    QPainterPath warped;
    for (int i = 0; i < glyphPath.elementCount(); ++i) {
        const QPainterPath::Element element = glyphPath.elementAt(i);
        const QPointF mapped = warp(QPointF(element.x, element.y));
        if (element.isMoveTo()) {
            warped.moveTo(mapped);
        } else {
            warped.lineTo(mapped);
        }
    }
    return warped;
}
