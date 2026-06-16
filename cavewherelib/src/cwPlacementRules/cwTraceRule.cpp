/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwTraceRule.h"

QString cwTraceRule::displayName() const
{
    return QStringLiteral("Trace");
}

void cwTraceRule::apply(QVector<cwStampPosition> &positions,
                        const cwPlacementContext &context) const
{
    // This rule's output is a traced path, not stamps — see tracePolylines().
    // Intentionally a no-op on the per-stamp pipeline.
    Q_UNUSED(positions)
    Q_UNUSED(context)
}

QVector<QPolygonF> cwTraceRule::tracePolylines(const QVector<QPointF> &strokeWorld,
                                               const cwPlacementContext &context) const
{
    // The traced path is the stroke this rule is handed. Lateral offset (parallel
    // rails) is not done here: an "Offset stroke" TransformStroke rule earlier in
    // the stack rebuilds the stroke first, so the layout passes us the already-
    // offset path and we trace it verbatim. The layer decides whether to fill it.
    Q_UNUSED(context)
    return {QPolygonF(strokeWorld)};
}
