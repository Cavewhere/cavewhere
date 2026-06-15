/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWDECORATIONLAYERVALIDATOR_H
#define CWDECORATIONLAYERVALIDATOR_H

//Qt includes
#include <QList>
#include <QSet>
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwError.h"

struct cwDecorationLayer;
class cwPlacementRuleRegistry;

// Load- and edit-time well-formedness check for a decoration layer's rule stack.
// Pure, synchronous, thread-safe (no QObject state): returns the layer's
// problems as typed cwErrors, and an empty list for a well-formed (or empty)
// stack.
//
// Severity (cwError::ErrorType):
//  - Fatal   — the engine would resolve an ambiguity arbitrarily (two rules
//              competing for a singleton slot), so the palette is refused at load.
//  - Warning — the layer renders unambiguously, just less than intended (a no-op
//              layer or a dead rule), so the palette loads with the warning
//              surfaced.
// Each error carries a SymbologyErrorCode in cwError::errorTypeId.
//
// NEVER called from the layout/render path: cwSketchDecorationLayout degrades
// silently at runtime (drops unknowns, first-terminal-wins) and must keep doing
// so. This is a load-time guard, reused by the brush editor (commit 9) for live
// per-layer feedback. Messages are location-free ("has...", "stamps...") so a
// caller can prefix the brush/layer; the editor groups by layer and uses them as-is.
namespace cwDecorationLayerValidator {

CAVEWHERE_LIB_EXPORT QList<cwError> validate(const cwDecorationLayer &layer,
                                            const cwPlacementRuleRegistry &registry,
                                            const QSet<QString> &availableGlyphNames);

}

#endif // CWDECORATIONLAYERVALIDATOR_H
