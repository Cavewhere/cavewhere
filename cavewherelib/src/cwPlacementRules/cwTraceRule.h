/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWTRACERULE_H
#define CWTRACERULE_H

//Our includes
#include "cwPlacementRule.h"

// Terminal rule whose output is one traced path, not stamps — the visible line of
// a wall/feature, the edge of a floor-step, and (when the layer carries a fill) a
// filled region such as a stream-indicator triangle. It ignores the per-stamp
// position vector; cwSketchDecorationLayout asks it for the traced geometry, which
// is the stroke it is handed. Whether that geometry is stroked-only or also filled
// is the layer's concern, not the rule's: the layout reads the layer's fill*
// fields and, if a fill is set, closes and fills the path (an open path's implicit
// closing edge is left undrawn by the pen but closed by the fill). A lateral
// offset (parallel rails, ceiling channel) is not this rule's job: an "Offset
// stroke" TransformStroke rule earlier in the stack rebuilds the stroke first, so
// this just traces it. Line/fill styling migrates into this rule's params once the
// layer's line*/fill* fields move (commit 9).
class cwTraceRule : public cwPlacementRule {
public:
    QString displayName() const override;
    Stage stage() const override { return Terminal; }
    OutputKind outputKind() const override { return OutputKind::Trace; }
    void apply(QVector<cwStampPosition> &positions,
               const cwPlacementContext &context) const override;
    QVector<QPolygonF> tracePolylines(const QVector<QPointF> &strokeWorld,
                                      const cwPlacementContext &context) const override;
};

#endif // CWTRACERULE_H
