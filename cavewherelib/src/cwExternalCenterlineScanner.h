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
#include <QDate>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

//Std includes
#include <algorithm>
#include <optional>

/**
 * One survey block found while scanning an entry file's dependency
 * closure: a named region of the survey whose stations share a
 * prefix. Phase 3 turns each block into a Scope trip.
 *
 * path is the dotted name relative to the file root, spelled exactly
 * as the file writes it - "big-passage.east" for an *end-nested
 * Survex block. stationCount counts the distinct station names
 * appearing in the block's OWN shot lines; stations belonging to a
 * child block are counted against that child, so the counts of a
 * whole tree sum to the file's stations. depth is the number of
 * ancestors (0 for a top-level block).
 *
 * A block stands for a naming level cavern actually builds, so a
 * file structure cavern flattens makes no block at all - its
 * stations are reported as ScanResult::rootStationCount instead.
 *
 * Per-format mapping:
 *   Survex  - the *begin / *end tree. An anonymous *begin folds
 *             into its parent: it makes no block and its shots
 *             count toward the enclosing named block.
 *   Compass - none. cavern discards a .dat's "SURVEY NAME:" value
 *             and reads each .mak '#' DAT into a survey named
 *             after its file (survex datain.c mak_dat_survey), so a
 *             .mak fix is recorded under that survey
 *             (compassDatSurveyName). Station counting has no such
 *             level yet: every Compass station still lands in
 *             rootStationCount, a known gap.
 *   Walls   - one block per distinct prefix path, merged across
 *             the project's files. path is the prefix levels
 *             #PREFIX3, #PREFIX2, #PREFIX1 joined by '.'
 *             (outermost first; #PREFIX / #P is #PREFIX1, the
 *             innermost), and depth is the number of non-empty
 *             prefix slots minus 1. .BOOK / .SURVEY titles and
 *             .NAMEs make no block: cavern treats them as
 *             metadata. A station token with its own ':'
 *             qualifiers replaces that many innermost prefix
 *             levels and keeps the outer levels in force.
 *             Stations written while no prefix is in force are
 *             root stations.
 *
 * date is the block's own survey date - Survex *date (a range takes
 * its start), Compass "SURVEY DATE:", Walls #DATE - and stays
 * invalid when the block writes none. Survex dates scope downward,
 * so a dateless nested block inherits its nearest dated ancestor's
 * date at the point a Scope trip is seeded, not here.
 */
class CAVEWHERE_LIB_EXPORT cwScanBlock
{
    Q_GADGET
    QML_VALUE_TYPE(cwScanBlock)

    Q_PROPERTY(QString path MEMBER path FINAL)
    Q_PROPERTY(QString name READ name FINAL)
    Q_PROPERTY(int stationCount MEMBER stationCount FINAL)
    Q_PROPERTY(int depth MEMBER depth FINAL)
    Q_PROPERTY(QDate date MEMBER date FINAL)

public:
    QString path;
    int stationCount = 0;
    int depth = 0;
    QDate date;

    /**
     * The block's own segment - the last dotted segment of path.
     * Approximation for Compass and Walls, whose survey names may
     * themselves contain a dot: such a name splits at its last dot.
     */
    QString name() const { return path.section(QLatin1Char('.'), -1); }

    bool operator==(const cwScanBlock& other) const
    {
        return path == other.path
            && stationCount == other.stationCount
            && depth == other.depth
            && date == other.date;
    }
    bool operator!=(const cwScanBlock& other) const { return !(*this == other); }
};

//The dialog's tree preview walks the whole list in qml, so the list
//itself has to be a qml sequence. See
//cwExternalSourceAttentionRowListRegistration for the same pattern.
class cwScanBlockListRegistration
{
    Q_GADGET
    QML_FOREIGN(QList<cwScanBlock>)
    QML_ANONYMOUS
    QML_SEQUENTIAL_CONTAINER(cwScanBlock)
};

Q_DECLARE_METATYPE(cwScanBlock)

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

/**
 * Best-effort trip metadata pulled from the entry file only -
 * nested included files are not consulted because their directives
 * would scope to a child block, not the trip being attached. The
 * one exception: a Compass .mak entry carries no header itself, so
 * the first referenced .dat's header is consulted instead.
 *
 * fileOwnsDeclination() is the file-owns-declination signal: cavern
 * applies the file's own directive over anything the driver emits,
 * so the driver must not inject a declination for that trip and the
 * UI shows the value read-only. It is true for a numeric directive
 * (declination set) and for Survex "*declination auto ..."
 * (declinationIsAuto - cavern computes the value itself via IGRF).
 *
 * Sources per format:
 *   date        - Survex *date (yyyy.mm.dd / yyyy-mm-dd),
 *                 Compass "SURVEY DATE: M D YYYY",
 *                 Walls #DATE YYYY-MM-DD
 *   declination - Survex *calibrate declination <deg> or
 *                 *declination <deg> <units> / *declination auto,
 *                 Compass "DECLINATION: <deg>" header field,
 *                 Walls #UNITS DECL=<deg>
 *   team        - one entry per person: Survex *team lines,
 *                 Compass "SURVEY TEAM:" names split like
 *                 cwCompassImporter (';' if present, else commas /
 *                 runs of spaces), Walls #NOTE lines mentioning
 *                 "team" split on commas (best-effort)
 *
 * Missing values leave the optional empty and the team list empty;
 * a present-but-unparseable date or declination additionally
 * appends a warning to ScanResult.warnings in every format.
 */
struct SeededTripMetadata {
    std::optional<QDate> date = std::nullopt;
    std::optional<double> declination = std::nullopt;  // degrees, positive east
    bool declinationIsAuto = false;
    QStringList team;

    bool fileOwnsDeclination() const
    {
        return declination.has_value() || declinationIsAuto;
    }

    bool operator==(const SeededTripMetadata& other) const
    {
        return date == other.date
            && declination == other.declination
            && declinationIsAuto == other.declinationIsAuto
            && team == other.team;
    }
    bool operator!=(const SeededTripMetadata& other) const { return !(*this == other); }
};

/**
 * One station the closure fixes: its name as cavern qualifies it within the
 * entry file (the enclosing *begin blocks, a Compass DAT's survey, a Walls
 * prefix path, then the station - "doghill.d1"), the same spelling
 * cwExternalStationHarvest's fixedNames use; its coordinate as the file
 * writes it; and the input coordinate system in force for that coordinate. A fix
 * with one reads its coordinate in that system; one without reads it in
 * whatever system the driver puts in scope around the file's *include.
 *
 * Per format:
 *   Survex  - a *fix, with the *cs (not *cs out) in force in its block, named
 *             as the file writes it ("EPSG:32616").
 *   Compass - a station fixed on a .mak '#' line ("name[m,x,y,z]", whose
 *             bracket is the coordinate), with a datum ('&') and a UTM zone
 *             ('$', or a base location's '@') in force, or a *cs around the
 *             .mak's *include. A .dat fixes nothing.
 *   Walls   - a #FIX in a .srv, with a .REF georeference in force on its .wpj
 *             entry. A .srv attached on its own has none.
 */
struct ScannedFix {
    QString station;
    QString coordinate;
    //! Empty when no input coordinate system is in force.
    QString coordinateSystem;

    bool hasCoordinateSystem() const { return !coordinateSystem.isEmpty(); }

    bool operator==(const ScannedFix& other) const = default;
};

/**
 * A Compass .mak whose project copy reads its UTM zone in Compass's default
 * datum, North American 1927, because the source names a zone and no datum
 * (see compassMakWithDefaultDatum): the .mak's file name and the zone as a
 * system name spells it ("13N").
 */
struct DefaultedDatum {
    QString file;
    QString zone;

    bool operator==(const DefaultedDatum& other) const = default;
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
     * directory, an include written as an absolute path) returns
     * Monad::Result with an error.
     */
    QStringList warnings;

    /**
     * The files the entry file references directly - first level
     * only, so a file pulled in by an included file stays out.
     * Stored as canonical absolute paths, de-duplicated, in walk
     * order. Feeds the multi-cave heuristic that offers alternate
     * attach targets when the entry looks like a project driver.
     */
    QStringList entryDirectIncludes;

    /**
     * Every named survey block in the closure, in document order -
     * a parent always precedes its children. Feeds Scope-trip
     * creation and the attach dialog's structure preview. See
     * cwScanBlock for the per-format mapping.
     */
    QList<cwScanBlock> blocks;

    /**
     * How many distinct station names the closure spells outside
     * every block - a Compass station, a Walls station with no
     * prefix in force, a Survex station outside every named *begin.
     * cavern gives these no naming level of their own, so the
     * cave's own namespace holds them and one whole-cave window
     * owns them (P3.16).
     */
    int rootStationCount = 0;

    /**
     * The first date directive the closure writes while no block is
     * open: a Survex *date at file root, a Compass "SURVEY DATE:"
     * (the first survey's), a Walls #DATE with no prefix in force.
     * Invalid when the closure writes none.
     */
    QDate rootDate;

    /**
     * True when the entry file itself carries shot data outside any
     * included file. False for a pure driver - a Survex master that
     * only writes *include lines, a Compass .mak, a Walls .wpj -
     * which is half of the multi-cave heuristic's trigger.
     */
    bool entryHasOwnShots = false;

    /**
     * Trip metadata seeded from the entry file. Participates in
     * equality so a metadata-only edit (e.g. adding *calibrate
     * declination) reads as a changed scan.
     */
    SeededTripMetadata seededMetadata;

    /**
     * Every station the closure fixes, in walk order. See ScannedFix.
     */
    QList<ScannedFix> fixes;

    /**
     * Every .mak in the closure that CaveWhere's copy gave Compass's default
     * datum, in walk order. Empty when the scan reads a source rather than
     * the project's copy.
     */
    QList<DefaultedDatum> defaultedDatums;

    bool hasFixWithoutCoordinateSystem() const
    {
        return std::any_of(fixes.cbegin(), fixes.cend(), [](const ScannedFix& fix) {
            return !fix.hasCoordinateSystem();
        });
    }

    bool operator==(const ScanResult& other) const
    {
        return dependencies == other.dependencies
            && warnings == other.warnings
            && entryDirectIncludes == other.entryDirectIncludes
            && blocks == other.blocks
            && rootStationCount == other.rootStationCount
            && rootDate == other.rootDate
            && entryHasOwnShots == other.entryHasOwnShots
            && seededMetadata == other.seededMetadata
            && fixes == other.fixes
            && defaultedDatums == other.defaultedDatums;
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
 * User-facing name for a format: "Survex", "Compass", or "Walls";
 * empty for Format::Unknown. The single source for the format label
 * shown by cwExternalCenterline::format() (attached owners) and the
 * attach dialog's "auto-detected" line (via
 * cwExternalCenterlineScanPreview). Proper nouns, so untranslated.
 */
CAVEWHERE_LIB_EXPORT QString formatName(Format format);

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
 * Returns Monad::Result error when the entry file itself cannot be
 * opened or canonicalised, and when any *include names its target
 * with an absolute path - a project written that way only solves on
 * the machine that authored it, so the attached copy would be
 * broken everywhere else. Missing or unreadable *included files
 * emit warnings; the rest of the closure continues to resolve.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<ScanResult> scanSurvex(const QString& entryFile);

/**
 * Walks a Compass entry file's dependency closure.
 *  - .dat -> just the file itself (data files don't include others)
 *  - .mak -> the .mak plus every '#'-line .dat referenced in it
 * Missing referenced .dat files emit warnings; an unreadable entry
 * file, or a '#' reference written as an absolute path, returns
 * Monad::Result error.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<ScanResult> scanCompass(const QString& entryFile);

/**
 * Walks a Walls entry file's dependency closure.
 *  - .srv -> just the file itself
 *  - .wpj -> the .wpj plus every leaf .srv reachable through the
 *           WpjBook tree (dewalls' WallsProjectParser is the
 *           authoritative parser here)
 * Missing referenced .srv files emit warnings; an unreadable entry
 * file, a parser failure, or a .PATH / .NAME written as an absolute
 * path returns Monad::Result error.
 */
CAVEWHERE_LIB_EXPORT Monad::Result<ScanResult> scanWalls(const QString& entryFile);

/**
 * True when \a path names a Compass project file (.mak, any case).
 */
CAVEWHERE_LIB_EXPORT bool isCompassMak(const QString& path);

/**
 * The bytes of CaveWhere's project copy of a Compass .mak. A .mak that names
 * a UTM zone ('$', or a base location's '@') and no datum ('&') anywhere gets
 * a line setting Compass's own default datum, North American 1927, inserted
 * before its first '$' or '@' line, so cavern and the scan read the zone in
 * the datum Compass shows the project in. Every other .mak comes back
 * byte-identical.
 */
CAVEWHERE_LIB_EXPORT QByteArray compassMakWithDefaultDatum(const QByteArray& makBytes);

} // namespace cwExternalCenterlineScanner

#endif // CWEXTERNALCENTERLINESCANNER_H
