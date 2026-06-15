/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTracePolylineRule.h"

QString cwTracePolylineRule::displayName() const
{
    return QStringLiteral("Trace polyline");
}

void cwTracePolylineRule::apply(QVector<cwStampPosition> &positions,
                                const cwPlacementContext &context) const
{
    // This rule's output is a continuous polyline, not stamps — see
    // tracePolylines(). Intentionally a no-op on the per-stamp pipeline.
    Q_UNUSED(positions)
    Q_UNUSED(context)
}

QVector<QPolygonF> cwTracePolylineRule::tracePolylines(const QVector<QPointF> &strokeWorld,
                                                       const cwPlacementContext &context) const
{
    // The traced polyline is the stroke this rule is handed. Lateral offset
    // (parallel rails) is not done here: an "Offset stroke" TransformStroke rule
    // earlier in the stack rebuilds the stroke first, so the layout passes us the
    // already-offset polyline and we trace it verbatim.
    Q_UNUSED(context)
    return {QPolygonF(strokeWorld)};
}
