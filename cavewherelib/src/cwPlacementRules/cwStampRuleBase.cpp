/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStampRuleBase.h"

//Qt includes
#include <QTransform>

//Std includes
#include <cmath>

void cwStampRuleBase::apply(QVector<cwStampPosition> &positions,
                            const cwPlacementContext &context) const
{
    for (auto &position : positions) {
        position.priority = context.layer.collisionPriority;
    }
}

QPainterPath cwStampRuleBase::stampPath(const cwStampPosition &position,
                                        const cwStampGeometry &geometry) const
{
    // Scale, then rotate (CCW, world +Y up), then translate to the anchor —
    // built explicitly to avoid QTransform's screen-space (y-down) convention.
    const double s = position.scale;
    const double c = std::cos(position.rotationRad);
    const double sn = std::sin(position.rotationRad);
    const QTransform transform(s * c, s * sn, -s * sn, s * c,
                               position.anchorWorld.x(), position.anchorWorld.y());
    return transform.map(geometry.glyphPath);
}
