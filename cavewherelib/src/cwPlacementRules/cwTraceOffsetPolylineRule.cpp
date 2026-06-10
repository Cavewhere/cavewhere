/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTraceOffsetPolylineRule.h"

QString cwTraceOffsetPolylineRule::displayName() const
{
    return QStringLiteral("Trace offset polyline");
}

void cwTraceOffsetPolylineRule::apply(QVector<cwStampPosition> &positions,
                                      const cwPlacementContext &context) const
{
    // This rule's output is a continuous polyline, not stamps — see
    // tracePolylines(). Intentionally a no-op on the per-stamp pipeline.
    Q_UNUSED(positions)
    Q_UNUSED(context)
}

QVector<QPolygonF> cwTraceOffsetPolylineRule::tracePolylines(const QVector<QPointF> &strokeWorld,
                                                             const cwPlacementContext &context) const
{
    // Iter 1 supports offset 0 only: the traced polyline is the stroke itself.
    // A non-zero perpendicular offset (context.layer.offsetCurveOffsetMm) is iter 2.
    Q_UNUSED(context)
    return {QPolygonF(strokeWorld)};
}
