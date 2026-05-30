/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWEXTERNALCENTERLINESCANNER_H
#define CWEXTERNALCENTERLINESCANNER_H

//Our includes
#include "cwGlobals.h"
#include <Monad/Result.h>

//Qt includes
#include <QStringList>

/**
 * Free-function scanner that walks the dependency closure of an
 * external-centerline entry file. Lives in a namespace because the
 * per-format helpers (scanSurvex / scanCompass / scanWalls) are
 * meaningfully cohesive but stateless - the dependency graph is
 * built entirely from the entry file and the filesystem.
 *
 * Phase 1 / commit 4 ships Survex support. Commit 5 adds Compass,
 * Walls, and the cross-format dispatch (scanSurvex following an
 * *include into a .dat or .wpj).
 *
 * See plans/EXTERNAL_FILE_PHASE1.html sections 5-7 for the full
 * edge-case spec.
 */
namespace cwExternalCenterlineScanner {

enum class Format {
    Survex,    // .svx
    Compass,   // .dat, .mak
    Walls,     // .wpj, .srv
    Unknown    // no extension or unrecognised
};

struct ScanResult {
    /**
     * Every file required to feed the entry file into cavern,
     * stored as canonical absolute paths. The entry file itself is
     * always the first element when it resolves. De-duplicated via
     * the same canonical-path visited set that drives cycle
     * detection.
     */
    QStringList dependencies;

    /**
     * Human-readable advisories. Surfaced in the attach dialog as
     * yellow [Attach]-still-enabled messages: missing *include
     * targets, case-fallback matches, encoding fallback, circular
     * include chains. The presence of warnings does NOT fail the
     * scan; only a hard failure (missing entry file, unreadable
     * directory) returns Monad::Result with an error.
     */
    QStringList warnings;

    bool operator==(const ScanResult& other) const
    {
        return dependencies == other.dependencies && warnings == other.warnings;
    }
    bool operator!=(const ScanResult& other) const { return !(*this == other); }
};

/**
 * Maps an entry file to its format using only the extension
 * (case-insensitive). Returns Format::Unknown when the file has no
 * recognised extension; the attach dialog surfaces that as an
 * error and keeps [Attach] disabled.
 */
CAVEWHERE_LIB_EXPORT Format formatFor(const QString& entryFile);

/**
 * Dispatches to the per-format scanner based on formatFor(). A
 * Format::Unknown entry file returns Monad::Result error rather
 * than walking; everything else delegates to the matching scanX
 * function. Cross-format includes from a Survex entry into a
 * Compass or Walls file (Phase 1 / commit 5) are handled inside
 * scanSurvex, not here.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<ScanResult> scan(const QString& entryFile);

/**
 * Walks a Survex (.svx) entry file's *include closure. Cycle
 * detection uses a canonical-path visited set; case-sensitivity
 * fallback retries unmatched paths against a case-insensitive
 * directory listing (Linux only; macOS / Windows are case-
 * preserving so the fallback never triggers there). Encoding
 * fallback decodes as UTF-8 first; on decode failure retries as
 * Latin-1 and emits a warning naming the encoding.
 *
 * Returns Monad::Result error only when the entry file itself
 * cannot be opened or canonicalised. Missing or unreadable
 * *included files emit warnings; the rest of the closure
 * continues to resolve.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<ScanResult> scanSurvex(const QString& entryFile);

/**
 * Walks a Compass entry file's dependency closure.
 *  - .dat -> just the file itself (data files don't include others)
 *  - .mak -> the .mak plus every '#'-line .dat referenced in it
 * Missing referenced .dat files emit warnings; only an unreadable
 * entry file returns Monad::Result error.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<ScanResult> scanCompass(const QString& entryFile);

/**
 * Walks a Walls entry file's dependency closure.
 *  - .srv -> just the file itself
 *  - .wpj -> the .wpj plus every leaf .srv reachable through the
 *           WpjBook tree (dewalls' WallsProjectParser is the
 *           authoritative parser here)
 * Missing referenced .srv files emit warnings; only an unreadable
 * entry file or a parser failure returns Monad::Result error.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<ScanResult> scanWalls(const QString& entryFile);

} // namespace cwExternalCenterlineScanner

#endif // CWEXTERNALCENTERLINESCANNER_H
