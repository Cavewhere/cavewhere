/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYPALETTESEED_H
#define CWSYMBOLOGYPALETTESEED_H

//Qt includes
#include <QString>
#include <QUuid>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSymbologyPaletteData.h"

// The built-in seed palette: shipped, read-only, and minimal. It mirrors the
// prototype's symbols (wall / scrap-outline / feature) expressed declaratively
// so nothing regresses, each with a stable machine name that is the
// cross-palette substitution key. The floor-step demo brush + glyph land in a
// later phase alongside cwSymbologyGlyph.
namespace cwSymbologyPaletteSeed {

// Stable identity, hard-coded so duplicate-id resolution is deterministic.
CAVEWHERE_LIB_EXPORT QUuid id();
CAVEWHERE_LIB_EXPORT QString name();

// Brush machine names (kebab-case, immutable).
CAVEWHERE_LIB_EXPORT QString wallBrushName();         // "wall"
CAVEWHERE_LIB_EXPORT QString scrapOutlineBrushName(); // "scrap-outline"
CAVEWHERE_LIB_EXPORT QString featureBrushName();      // "feature"

// Builds the seed palette payload. The manager wraps it in a read-only
// cwSymbologyPalette.
CAVEWHERE_LIB_EXPORT cwSymbologyPaletteData create();

}

#endif // CWSYMBOLOGYPALETTESEED_H
