/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWRIGIDSTAMPRULE_H
#define CWRIGIDSTAMPRULE_H

//Our includes
#include "cwStampRuleBase.h"

// Terminal rule: each visible position becomes a glyph drawn straight at its
// anchor + rotation (the shared cwStampRuleBase rigid materialisation). Used by
// both repeated stacks (Uniform spacing -> Align to tangent -> Rigid stamp) and
// single-glyph stacks (Explicit point -> Rigid stamp).
class cwRigidStampRule : public cwStampRuleBase {
public:
    QString displayName() const override;
};

#endif // CWRIGIDSTAMPRULE_H
