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
#include "cwLineBrush.h"
#include "cwSymbologyGlyph.h"
#include "cwSymbologyPaletteData.h"
#include "Monad/Result.h"

// JSON serialization for a palette directory. The directory is the source of
// truth: palette.json holds palette-level identity only, while membership is the
// set of brushes/<name>.cwbrush and glyphs/<name>.cwglyph files present — one
// human-readable JSON file per editable thing, so independent adds merge with
// zero git conflict. See plans/SYMBOLOGY_PALETTE_DIRECTORY_TRUTH_PLAN.md.
//
// Loads return Monad::Result<cwSymbologyPaletteData> by value (no output
// pointers), so a palette can be parsed off the UI thread and moved back for
// async loading. Each per-file name is validated as kebab-case (the path-safety
// guard) and checked against its filename stem.
namespace cwSymbologyPaletteIO {

// Result of a directory load(): the palette plus the highest save/load format
// version (CavewhereCommonProto.FileVersion.version) seen across palette.json and
// every brush/glyph file. maxFileVersion > cwRegionIOTask::protoVersion() means a
// newer build wrote part of this palette — the caller marks it read-only and
// warns rather than re-saving and dropping the newer data. maxFileVersion == 0
// means nothing carried a stamp (a pre-versioning palette), which is in range.
struct cwSymbologyPaletteLoadResult {
    cwSymbologyPaletteData palette;
    int maxFileVersion = 0;
};

CAVEWHERE_LIB_EXPORT extern const QString kPaletteJsonFileName; // "palette.json"
CAVEWHERE_LIB_EXPORT extern const QString kBrushesSubdirName;   // "brushes"
CAVEWHERE_LIB_EXPORT extern const QString kBrushFileSuffix;     // ".cwbrush"
CAVEWHERE_LIB_EXPORT extern const QString kGlyphsSubdirName;    // "glyphs"
CAVEWHERE_LIB_EXPORT extern const QString kGlyphFileSuffix;     // ".cwglyph"

// File I/O against a palette directory. save writes palette.json plus one file
// per brush and glyph, then reconciles (deletes brush/glyph files no longer in
// the palette); load reads palette.json then scans the brushes/ and glyphs/
// subdirs, assembling the palette from the files present.
CAVEWHERE_LIB_EXPORT Monad::ResultBase save(const cwSymbologyPaletteData &palette, const QString &directory);
CAVEWHERE_LIB_EXPORT Monad::Result<cwSymbologyPaletteLoadResult> load(const QString &directory);

// Single-brush file I/O against the palette directory's brushes/ subdir. The
// brush editor and palette save() use saveBrush; rewriting one brush leaves the
// other brushes' files untouched.
CAVEWHERE_LIB_EXPORT Monad::ResultBase saveBrush(const cwLineBrush &brush, const QString &directory);
CAVEWHERE_LIB_EXPORT Monad::Result<cwLineBrush> loadBrush(const QString &directory, const QString &brushName);

// Single-glyph file I/O against the palette directory's glyphs/ subdir. The
// glyph editor and palette save() use saveGlyph; rewriting one glyph leaves the
// other glyphs' files untouched.
CAVEWHERE_LIB_EXPORT Monad::ResultBase saveGlyph(const cwSymbologyGlyph &glyph, const QString &directory);
CAVEWHERE_LIB_EXPORT Monad::Result<cwSymbologyGlyph> loadGlyph(const QString &directory, const QString &glyphName);

// In-memory (de)serialization, used by the file layer above and directly by
// tests. Proto types stay out of this header so consumers don't pull in the
// generated schema. The palette form carries palette-level identity only (no
// brushes or glyphs); the brush and glyph forms are the self-contained per-file
// payloads.
CAVEWHERE_LIB_EXPORT QByteArray toJson(const cwSymbologyPaletteData &palette);
CAVEWHERE_LIB_EXPORT Monad::Result<cwSymbologyPaletteData> fromJson(const QByteArray &json);
CAVEWHERE_LIB_EXPORT QByteArray brushToJson(const cwLineBrush &brush);
CAVEWHERE_LIB_EXPORT Monad::Result<cwLineBrush> brushFromJson(const QByteArray &json);
CAVEWHERE_LIB_EXPORT QByteArray glyphToJson(const cwSymbologyGlyph &glyph);
CAVEWHERE_LIB_EXPORT Monad::Result<cwSymbologyGlyph> glyphFromJson(const QByteArray &json);

}

#endif // CWSYMBOLOGYPALETTEIO_H
