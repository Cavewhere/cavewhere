/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwStampRuleBase.h"
#include "cwStrokePath.h"

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
                                        const QPainterPath &glyphPath,
                                        const cwPlacementContext &context) const
{
    Q_UNUSED(context) // rigid placement ignores the stroke path
    // Scale, then rotate (CCW, world +Y up), then translate to the anchor —
    // built explicitly to avoid QTransform's screen-space (y-down) convention.
    const double s = position.scale;
    const double c = std::cos(position.rotationRad);
    const double sn = std::sin(position.rotationRad);
    const QTransform transform(s * c, s * sn, -s * sn, s * c,
                               position.anchorWorld.x(), position.anchorWorld.y());
    return transform.map(glyphPath);
}

QPointF cwStampRuleBase::bendWarp(const cwStrokePath &strokePath,
                                  double startArcLength,
                                  double scale,
                                  const QPointF &glyphPoint)
{
    const double s = startArcLength + scale * glyphPoint.x();
    const double lateral = scale * glyphPoint.y();
    QPointF point;
    QPointF normal;
    strokePath.pointAndNormalAtArcLength(s, point, normal);
    return point + lateral * normal;
}
