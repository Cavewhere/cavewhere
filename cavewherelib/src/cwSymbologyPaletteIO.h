/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSYMBOLOGYPALETTEIO_H
#define CWSYMBOLOGYPALETTEIO_H

//Qt includes
#include <QByteArray>
#include <QString>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwSymbologyGlyph.h"
#include "cwSymbologyPaletteData.h"
#include "Monad/Result.h"

// JSON serialization for a palette directory. The directory holds palette.json,
// a human-readable JSON projection of the proto schema (source of truth, clean
// git diffs). Glyph files land in a later phase.
//
// Loads return Monad::Result<cwSymbologyPaletteData> by value (no output
// pointers), so a palette can be parsed off the UI thread and moved back for
// async loading. Loading validates the uniqueness invariant — duplicate brush
// names are ambiguous because names are the substitution key — and fails with a
// clear message naming the duplicate.
namespace cwSymbologyPaletteIO {

CAVEWHERE_LIB_EXPORT extern const QString kPaletteJsonFileName; // "palette.json"
CAVEWHERE_LIB_EXPORT extern const QString kGlyphsSubdirName;    // "glyphs"
CAVEWHERE_LIB_EXPORT extern const QString kGlyphFileSuffix;     // ".cwglyph"

// File I/O against a palette directory. save/load round-trip the palette file
// plus one glyphs/<name>.cwglyph file per glyph; load assembles full glyphs
// (metadata from the palette file, strokes from the per-glyph files).
CAVEWHERE_LIB_EXPORT Monad::ResultBase save(const cwSymbologyPaletteData &palette, const QString &directory);
CAVEWHERE_LIB_EXPORT Monad::Result<cwSymbologyPaletteData> load(const QString &directory);

// Single-glyph file I/O against the palette directory's glyphs/ subdir. The
// glyph editor and palette save() use saveGlyph; rewriting one glyph leaves the
// other glyphs' files untouched.
CAVEWHERE_LIB_EXPORT Monad::ResultBase saveGlyph(const cwSymbologyGlyph &glyph, const QString &directory);
CAVEWHERE_LIB_EXPORT Monad::Result<cwSymbologyGlyph> loadGlyph(const QString &directory, const QString &glyphName);

// In-memory (de)serialization, used by the file layer above and directly by
// tests. Proto types stay out of this header so consumers don't pull in the
// generated schema. The palette form carries glyph *metadata* only (no strokes);
// the glyph form is the self-contained per-glyph payload.
CAVEWHERE_LIB_EXPORT QByteArray toJson(const cwSymbologyPaletteData &palette);
CAVEWHERE_LIB_EXPORT Monad::Result<cwSymbologyPaletteData> fromJson(const QByteArray &json);
CAVEWHERE_LIB_EXPORT QByteArray glyphToJson(const cwSymbologyGlyph &glyph);
CAVEWHERE_LIB_EXPORT Monad::Result<cwSymbologyGlyph> glyphFromJson(const QByteArray &json);

}

#endif // CWSYMBOLOGYPALETTEIO_H
