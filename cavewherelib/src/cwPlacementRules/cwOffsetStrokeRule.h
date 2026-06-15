/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWOFFSETSTROKERULE_H
#define CWOFFSETSTROKERULE_H

//Our includes
#include "cwPlacementRule.h"

// TransformStroke stage. Rebuilds the stroke the rest of the layer's rule stack
// runs against, pushed laterally by cwOffsetStrokeParams::offsetMm along the
// stroke's left normal (signed: + = left side, 0 = on the stroke). The layout
// pulls this rule out of the stack and applies it before Generate, so every
// downstream rule — glyph stamps and the polyline trace alike — follows the
// offset for free by sampling the transformed stroke. This is the shared
// primitive behind parallel rails (commit 4.5) and the ceiling channel (4.6).
//
// Known limit: a parametric-parallel offset self-intersects on the inner side of
// a tight concave bend (offset distance ~ local radius). Cave passages have the
// offset far smaller than the curve radius, so it never shows; robust
// (Clipper-style) offsetting is deferred until a symbol actually needs it.
class cwOffsetStrokeRule : public cwPlacementRule {
public:
    QString displayName() const override;
    Stage stage() const override { return TransformStroke; }
    void apply(QVector<cwStampPosition> &positions,
               const cwPlacementContext &context) const override;
    QVector<QPointF> transformStroke(const QVector<QPointF> &strokeWorld,
                                     const cwPlacementContext &context) const override;
};

#endif // CWOFFSETSTROKERULE_H
