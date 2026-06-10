/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXPLICITPOINTRULE_H
#define CWEXPLICITPOINTRULE_H

//Our includes
#include "cwPlacementRule.h"

// Generate stage. Seeds a single stamp at one anchor — the single-glyph
// counterpart to Uniform spacing. Until rule params carry an explicit world
// anchor (the params/editor commit), it uses the stroke's first point — the
// cartographer-placed location of a point symbol.
class cwExplicitPointRule : public cwPlacementRule {
public:
    QString displayName() const override;
    Stage stage() const override { return Generate; }
    void apply(QVector<cwStampPosition> &positions,
               const cwPlacementContext &context) const override;
};

#endif // CWEXPLICITPOINTRULE_H
