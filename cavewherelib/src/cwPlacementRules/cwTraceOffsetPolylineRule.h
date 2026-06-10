/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRACEOFFSETPOLYLINERULE_H
#define CWTRACEOFFSETPOLYLINERULE_H

//Our includes
#include "cwPlacementRule.h"

// Terminal rule whose output is one continuous traced polyline, not stamps —
// the visible line of a wall/feature, and the edge of a floor-step. It ignores
// the per-stamp position vector; cwSketchDecorationLayout asks it for the
// polylines (iter 1: offset 0, the polyline equals the stroke). A layer ending
// with this rule is what used to be "OffsetCurve mode".
class cwTraceOffsetPolylineRule : public cwPlacementRule {
public:
    QString displayName() const override;
    Stage stage() const override { return Terminal; }
    OutputKind outputKind() const override { return OutputKind::Polylines; }
    void apply(QVector<cwStampPosition> &positions,
               const cwPlacementContext &context) const override;
    QVector<QPolygonF> tracePolylines(const QVector<QPointF> &strokeWorld,
                                      const cwPlacementContext &context) const override;
};

#endif // CWTRACEOFFSETPOLYLINERULE_H
