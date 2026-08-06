#ifndef CWSURVEXEXPORTERUTILS_H
#define CWSURVEXEXPORTERUTILS_H

//Qt includes
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

//Std includes
#include <optional>

//Our includes
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwGeoPoint.h"

class cwClinoReading;
class cwShot;
class cwStation;
class QTextStream;

namespace cwSurvexExporterUtils {

//! `<e> <n> <z>`, each fixed to six decimals — survex's parser rejects
//! scientific notation.
void writeCoordTriplet(QTextStream& stream, double e, double n, double z);

//! `*calibrate <type> <value>`, negated because survex's sign convention is the
//! opposite of ours. A zero value at unit scale is left implicit.
void writeCalibration(QTextStream& stream, const QString& type, double value, double scale = 1.0);

/**
 * The trip's `*calibrate DECLINATION`.
 *
 * Written even at zero when \a autoDeclinationInScope, which is the one case
 * where a zero has to be spelled out: silence would inherit the enclosing
 * block's `*declination auto` instead of overriding it with the zero that was
 * asked for. Cavern honors the explicit zero — cmd_calibrate stores 0 rather
 * than HUGE_REAL, and get_declination tests that slot before the auto path.
 *
 * \a gridConvergence is subtracted first, with a comment saying so. CaveWhere's
 * declination is a pure magnetic declination (magnetic -> true), and cavern
 * folds convergence (true -> grid) into `*declination auto` only, never into a
 * literal `*calibrate DECLINATION` (survex datain.c get_declination) — so
 * without this the literal path plots to true north while the auto path plots
 * to grid north (issue #628). Zero for a block with no grid, which leaves the
 * value untouched.
 */
void writeDeclinationCalibration(QTextStream& stream,
                                 bool autoDeclination,
                                 double declination,
                                 bool autoDeclinationInScope,
                                 double gridConvergence = 0.0);

/**
 * The *cs in scope in the block being written.
 *
 * Cavern scopes *cs to its *begin block (cmd_begin copies proj_str into the
 * child settings), so every writer inside one block is naming the same system
 * until somebody changes it. Sharing one CsScope across those writers lets each
 * ask for the system it needs and get a *cs line only when that actually
 * changes something.
 */
class CsScope
{
public:
    //! A CS-less request leaves the scope alone, matching cavern: a fix with no
    //! system of its own doesn't un-declare the enclosing one.
    void ensure(QTextStream& stream, const QString& cs);

private:
    QString m_current;
};

//! Representative location for a `*declination auto X Y Z` directive, derived
//! from the first fix station that actually gives one — it needs both a CS to
//! read the coordinate under and a coordinate to read. Survex IGRF only needs
//! one representative point per scope, so later fixes are ignored once one
//! qualifies. A fix with no CS of its own yields nothing rather than borrowing
//! the project's.
struct DeclinationContext {
    QString inputCS;
    double easting = 0.0;
    double northing = 0.0;
    double elevation = 0.0;
};

//! The first fix in `fixes` that can stand for the whole scope, or nothing when
//! none can.
std::optional<DeclinationContext> makeDeclinationContext(const QList<cwFixStation>& fixes);

/**
 * Grid convergence in degrees at \a ctx's location, read in \a outputCS — the
 * system `*cs out` names, which is the only grid the solved stations are ever
 * plotted in. A fix's own inputCS says where the cave is, not which grid the
 * answer is about, so the location is transformed into \a outputCS first.
 *
 * Zero when the block has no representative location, when nothing named an
 * output system (cavern then solves in no projection, so there is no grid to
 * converge to), or when PROJ can't bridge the two systems.
 */
double gridConvergenceForBlock(const std::optional<DeclinationContext>& ctx,
                               const QString& outputCS);

/**
 * Emit the cave block's `*declination auto X Y Z`, preceded by a `*cs` when
 * \a scope doesn't already name the system the coordinate is in. Returns
 * whether one is now in scope for the trips inside the block.
 *
 * Written once per cave, not once per trip. Cavern converts the location to
 * WGS84 at parse time and stores it on the settings stack, so every enclosed
 * *begin block inherits it, and re-evaluates IGRF from that block's own *date.
 * \a anyTripUsesAuto keeps caves that never ask for auto byte-identical to what
 * they exported before.
 */
bool writeBlockDeclinationAuto(QTextStream& stream,
                               const QList<cwFixStation>& fixes,
                               bool anyTripUsesAuto,
                               CsScope& scope);

/**
 * The `*cs out` + `*declination auto` preamble for a trip exported on its own,
 * which is the whole file: nothing above it has named a coordinate frame or a
 * location. Both or neither — `*declination auto` without `*cs out` is a cavern
 * error, since the grid convergence it subtracts is a property of the output
 * system. Returns whether the location was written.
 *
 * No `*fix`: fixes belong to the cave, and a lone trip means "solve this from
 * the origin". \a fixes are read only for the location.
 */
bool writeStandaloneTripHeader(QTextStream& stream,
                               const QList<cwFixStation>& fixes,
                               bool tripUsesAuto);

/**
 * Which coordinate system *cs out should name. There is no default, and the two
 * answers are now genuinely different systems: the working frame is the
 * project's local projection, which is a PROJ string centered on this one cave
 * and no use to anybody else, while a shared file wants a system its reader can
 * paste somewhere.
 */
enum class OutputCSPolicy {
    //! A system the file's reader can use elsewhere. What an exported .svx
    //! wants — it is for somebody else to read.
    Shareable,
    //! The frame the caller intends to read the solved positions back in. What
    //! the line-plot driver wants; it hands cavern's output straight to the
    //! scene, so *cs out has to name the frame the scene is in.
    WorkingFrame
};

//! The CS a fix can contribute to a file that leaves the project: the WGS84 UTM
//! zone containing it, or a projected CS as it stands. A fix that places
//! nothing contributes nothing — cwFixStation::hasPlacedCoordinate.
inline QString shareableCSForFix(const cwFixStation& fix)
{
    if (!fix.hasPlacedCoordinate()) {
        return QString();
    }
    // Empty for a system PROJ can't read, and for one it can read but whose
    // point wouldn't transform. Both leave the choice to the next fix rather
    // than offering the string as it stands: importers normalize their format's
    // spelling to PROJ's, so a system PROJ can't read names nothing cavern
    // could use either — and a geographic CS as *cs out fails the solve
    // outright, whatever it is spelled like.
    return cwCoordinateTransform::deriveProjectedOutputCS(
        fix.inputCS(), cwGeoPoint(fix.easting(), fix.northing(), fix.elevation()));
}

//! The first fix in \a fixes that yields a shareable CS, or empty when none
//! does. What a cave or trip exported on its own names in *cs out.
inline QString shareableCSForFixes(const QList<cwFixStation>& fixes)
{
    for (const cwFixStation& fix : fixes) {
        const QString cs = shareableCSForFix(fix);
        if (!cs.isEmpty()) {
            return cs;
        }
    }
    return QString();
}

/**
 * Survex requires *cs out whenever any *cs appears, so a project whose fixes
 * carry a CS of their own must name one — otherwise cavern errors with "input
 * projection is set but output projection isn't" the moment the per-cave *cs is
 * emitted.
 *
 * \a frameCS is the project's local projection, which is the whole answer under
 * WorkingFrame: the solve reports its stations in whatever *cs out names, and
 * the scene is in the LDP. A fix cannot stand in for it — a fix's own CS is
 * where its coordinate was typed, not where the scene is — so an
 * Ungeoreferenced project yields nothing here, which is right: it has no fixes
 * to place either, and cavern falls back to fixing the first station at the
 * origin.
 *
 * Shareable ignores \a frameCS for the opposite reason: an LDP is a PROJ string
 * built around one cave, and pasting it anywhere else is meaningless. The first
 * fix that yields a shareable system wins — for a geographic fix that is the
 * UTM zone containing it.
 *
 * The Region template parameter is duck-typed: it must expose `.caves`, and
 * each cave must expose `.fixStations`. This works for both
 * cwSurveyDataArtifact::Region (Rule export path) and cwCavingRegionData
 * (line-plot export path), which is why this lives here as a template rather
 * than beside the rest.
 */
template <typename Region>
QString resolveOutputCS(const Region& region, const QString& frameCS, OutputCSPolicy policy)
{
    if (policy == OutputCSPolicy::WorkingFrame) {
        // No screening: the frame is either empty or a local projection PROJ
        // built, never one of survex's output-refused keywords.
        return frameCS;
    }

    for (const auto& cave : region.caves) {
        const QString cs = shareableCSForFixes(cave.fixStations);
        if (!cs.isEmpty()) {
            return cs;
        }
    }
    return QString();
}

//! Valid survex *team role keywords (must match role_tab in
//! survex/src/commands.c)
bool isValidSurvexRole(const QString& role);

//! `value` with any exponent rewritten as a plain decimal, since survex's
//! parser rejects scientific notation.
QString toSurvexNumber(const QString& value);

//! "UP" or "DOWN" for a plumbed clino reading, or an empty string when the
//! reading isn't vertical.
QString verticalClinoText(const cwClinoReading& reading);

//! Why a fix can't be written out, or an empty string when it can.
QString fixUnusableReason(const cwFixStation& fix);

/**
 * Filter fix stations against the cave's actual stations and dedupe by
 * stationName (case-insensitive). Returns the kept fixes; appends one error
 * line per dropped fix to errors.
 *
 * - A fix whose stationName is empty or doesn't match any station name in
 *   stationNamesLower is dropped with "fix references unknown station".
 * - A fix whose (lowercased) stationName collides with an earlier kept fix
 *   is dropped with "duplicate fix on station".
 * - A fix whose coordinate can't be read — no CS to read it under, unreadable
 *   text, or no text at all — is dropped with what's wrong with it. Its
 *   components are 0, so writing it would silently relocate the cave.
 */
QList<cwFixStation> validateFixStations(const QList<cwFixStation>& fixes,
                                        const QSet<QString>& stationNamesLower,
                                        QStringList& errors);

/**
 * Emit the per-cave *cs / *fix block.
 *
 * - When fixes is non-empty: group by inputCS, emitting `*cs <inputCS>`
 *   before each group and `*fix` per fix.
 *
 *   Pass only fixes that validateFixStations kept. This writes
 *   `fix.easting()/northing()/elevation()`, which are 0 for every state but
 *   Valid, so an unreadable or CS-less fix reaching here anchors its station at
 *   the origin and takes the cave with it. validateFixStations drops those with
 *   an error; a caller that skips it gets no such protection.
 * - When fixes is empty: fall back to `*fix <fallbackFirstStation> 0 0 0`.
 *   When globalCS is set, prefix it with `*cs <globalCS>` so the legacy
 *   fallback survives the *cs out scope. With no globalCS, no *cs is
 *   emitted (un-fixed projects keep their pre-CS behavior).
 *
 * \a scope carries which system is in scope out to the caller, so what the
 * block writes after the fixes — `*declination auto` — can skip a *cs that
 * would name the system already in force.
 */
void writeFixStations(QTextStream& stream,
                      const QList<cwFixStation>& fixes,
                      const QString& fallbackFirstStation,
                      const QString& globalCS,
                      CsScope& scope);

//! True when a shot only carries passage dimensions: a valid zero-length
//! distance with no compass or clino (front or back). Compass records LRUD on
//! dead-end stations this way, e.g. "ALT13LRUD ALT13 0 - - - -". Survex rejects
//! such a leg (omitted compass on a non-plumbed shot), so it's exported as an
//! *equate of the two stations instead.
bool isLrudOnlyShot(const cwShot& shot);

bool stationHasLrudData(const cwStation& station);

//! Opens a `*data passage` block. Both survex exporters write it, so they share
//! one spelling of the wire format.
inline QString passageDataHeader() {
    return QStringLiteral("*data passage station left right up down ignoreall");
}

} // namespace cwSurvexExporterUtils

#endif // CWSURVEXEXPORTERUTILS_H
