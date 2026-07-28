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

/**
 * Survex requires *cs out whenever any *cs appears, so when the user hasn't
 * set a globalCS but a fix has its own inputCS, fall back to that fix's CS
 * for *cs out — otherwise cavern errors with "input projection is set but
 * output projection isn't" the moment the per-cave *cs is emitted.
 *
 * <b>Only a projected fix CS will do.</b> Cavern refuses a geographic output
 * system outright ("Coordinate system unsuitable for output" —
 * survex/src/commands.c:2672, the `ok_for_output == NO` branch), so a
 * geographic fix is no fallback at all; emitting it fails the whole solve.
 * That covers both ways a fix can be geographic: one PROJ recognizes, and one
 * spelled as a survex keyword PROJ can't parse (see isUnusableAsSurvexOutputCS).
 * Fix stations, unlike the project's own CS, may legitimately be geographic and
 * new rows start that way, so this is the ordinary shape of a project whose
 * global CS was never set, not an exotic one. With nothing projected to fall
 * back to we emit no *cs out, and cwFixStationValidator's needsOutputCS prompt
 * is what asks the user to choose one.
 *
 * The Region template parameter is duck-typed: it must expose
 * `.globalCoordinateSystem` and `.caves`, and each cave must expose
 * `.fixStations`. This works for both cwSurveyDataArtifact::Region
 * (Rule export path) and cwCavingRegionData (line-plot export path), which is
 * why this one is a template and lives here rather than beside the rest.
 */
template <typename Region>
QString resolveOutputCS(const Region& region)
{
    if (!region.globalCoordinateSystem.isEmpty()) {
        return region.globalCoordinateSystem;
    }
    for (const auto& cave : region.caves) {
        for (const cwFixStation& fix : cave.fixStations) {
            const QString cs = fix.inputCS().trimmed();
            if (!cs.isEmpty()
                && !cwCoordinateTransform::isGeographic(cs)
                && !isUnusableAsSurvexOutputCS(cs)) {
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
 *   <b>Pass only fixes that validateFixStations kept.</b> This writes
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
