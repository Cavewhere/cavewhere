/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWALIGNTOTANGENTRULE_H
#define CWALIGNTOTANGENTRULE_H

//Our includes
#include "cwPlacementRule.h"

// MutatePerLayer stage. Sets each stamp's orientation to the local stroke
// tangent so a glyph authored along +X (e.g. the floor-step tick) rides the
// line, its +Y landing on the +normal side. Absolute alignment, not a relative
// rotation.
class cwAlignToTangentRule : public cwPlacementRule {
public:
    QString displayName() const override;
    QString description() const override;
    Stage stage() const override { return MutatePerLayer; }
    void apply(QVector<cwStampPosition> &positions,
               const cwPlacementContext &context) const override;
};

#endif // CWALIGNTOTANGENTRULE_H
