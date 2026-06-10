/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWUNIFORMSPACINGRULE_H
#define CWUNIFORMSPACINGRULE_H

//Our includes
#include "cwPlacementRule.h"

// Generate stage. Seeds one stamp at every fixed arclength step along the
// stroke (paper-mm spacing baked to world metres). The default step matches the
// seed floor-step's authored 2 mm tick spacing; per-layer params override it
// once param interpretation lands (the params/editor commit).
class cwUniformSpacingRule : public cwPlacementRule {
public:
    QString displayName() const override;
    Stage stage() const override { return Generate; }
    void apply(QVector<cwStampPosition> &positions,
               const cwPlacementContext &context) const override;
};

#endif // CWUNIFORMSPACINGRULE_H
