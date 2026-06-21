/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwExplicitPointRule.h"
#include "cwStrokePath.h"

QString cwExplicitPointRule::displayName() const
{
    return QStringLiteral("Explicit point");
}

QString cwExplicitPointRule::description() const
{
    return QStringLiteral("Places a single glyph at one spot — for a standalone "
                          "point symbol.");
}

void cwExplicitPointRule::apply(QVector<cwStampPosition> &positions,
                                const cwPlacementContext &context) const
{
    if (context.strokePath.isEmpty()) {
        return;
    }

    cwStampPosition position;
    position.anchorWorld = context.strokePath.pointAtArcLength(0.0);
    position.glyphName = context.layer.glyphName;
    positions.append(position);
}
