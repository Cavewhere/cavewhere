/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRACEPOLYLINERULE_H
#define CWTRACEPOLYLINERULE_H

//Our includes
#include "cwPlacementRule.h"

// Terminal rule whose output is one continuous traced polyline, not stamps —
// the visible line of a wall/feature, and the edge of a floor-step. It ignores
// the per-stamp position vector; cwSketchDecorationLayout asks it for the
// polyline, which is the stroke it is handed. A lateral offset (parallel rails,
// ceiling channel) is not this rule's job: an "Offset stroke" TransformStroke
// rule earlier in the stack rebuilds the stroke first, so this just traces it.
// This is where line-specific styling (dash, width, taper) will live once the
// layer's line* fields migrate into rule params (commit 9).
class cwTracePolylineRule : public cwPlacementRule {
public:
    QString displayName() const override;
    Stage stage() const override { return Terminal; }
    OutputKind outputKind() const override { return OutputKind::Polylines; }
    void apply(QVector<cwStampPosition> &positions,
               const cwPlacementContext &context) const override;
    QVector<QPolygonF> tracePolylines(const QVector<QPointF> &strokeWorld,
                                      const cwPlacementContext &context) const override;
};

#endif // CWTRACEPOLYLINERULE_H
