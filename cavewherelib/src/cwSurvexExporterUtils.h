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

//! Emit `*cs <inputCS>` + `*declination auto X Y Z` inside the current trip
//! block. Survex re-evaluates IGRF for the trip's `*date`, so the literal
//! `*calibrate DECLINATION` line is suppressed by the caller.
void writeDeclinationAuto(QTextStream& stream, const DeclinationContext& ctx);

//! True for survex's own keyword coordinate systems that cavern refuses as an
//! output system — see the definition for which, and why the test can't be
//! "PROJ doesn't understand it".
bool isUnusableAsSurvexOutputCS(const QString& cs);

//! \a cs rendered as survex *cs syntax. Survex names a system by keyword or
//! authority code; anything else — a raw PROJ string, which the project's own
//! local projection is — has to be quoted behind the CUSTOM keyword, or cavern
//! refuses it with "Unknown coordinate system" (survex/src/commands.c, the
//! CS_CUSTOM branch). The test is what a bare survex name may contain rather
//! than what a PROJ string looks like, so WKT is quoted too.
QString toSurvexCS(const QString& cs);

//! Emit a `*cs` (or `*cs out`) line for \a cs, quoted as survex needs. Every
//! emitter goes through here so a new one can't drop the CUSTOM quoting and
//! fail the solve.
void writeCsLine(QTextStream& stream, const QString& cs, bool isOutput = false);

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
    const QString derived = cwCoordinateTransform::deriveProjectedOutputCS(
        fix.inputCS(), cwGeoPoint(fix.easting(), fix.northing(), fix.elevation()));
    if (!derived.isEmpty()) {
        return derived;
    }
    // A system PROJ doesn't know at all is a survex spelling of its own (UTM16N,
    // S-MERC, OSGB:XX, EUR79Z30) — cavern accepts those for output, and an svx
    // import stores *cs verbatim — so offer the string as it stands;
    // isUnusableAsSurvexOutputCS screens the ones cavern refuses. A system PROJ
    // *does* know yielded nothing for a different reason: the point wouldn't
    // transform. Falling back there would name a geographic CS as *cs out and
    // fail the solve, so leave it to the next fix.
    if (cwCoordinateTransform::isValidCS(fix.inputCS())) {
        return QString();
    }
    return fix.inputCS().trimmed();
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
        for (const cwFixStation& fix : cave.fixStations) {
            const QString cs = shareableCSForFix(fix);
            if (!cs.isEmpty() && !isUnusableAsSurvexOutputCS(cs)) {
                return cs;
            }
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
 */
void writeFixStations(QTextStream& stream,
                      const QList<cwFixStation>& fixes,
                      const QString& fallbackFirstStation,
                      const QString& globalCS = QString());

//! True when a shot only carries passage dimensions: a valid zero-length
//! distance with no compass or clino (front or back). Compass records LRUD on
//! dead-end stations this way, e.g. "ALT13LRUD ALT13 0 - - - -". Survex rejects
//! such a leg (omitted compass on a non-plumbed shot), so it's exported as an
//! *equate of the two stations instead.
bool isLrudOnlyShot(const cwShot& shot);

bool stationHasLrudData(const cwStation& station);

} // namespace cwSurvexExporterUtils

#endif // CWSURVEXEXPORTERUTILS_H
