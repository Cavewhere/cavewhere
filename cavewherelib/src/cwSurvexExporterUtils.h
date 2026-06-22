#ifndef CWSURVEXEXPORTERUTILS_H
#define CWSURVEXEXPORTERUTILS_H

#include <QtGlobal>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QList>
#include <QTextStream>
#include <optional>

#include "cwClinoReading.h"
#include "cwCompassReading.h"
#include "cwDistanceReading.h"
#include "cwFixStation.h"
#include "cwStation.h"
#include "cwShot.h"

namespace cwSurvexExporterUtils {

// Resolve a fix's input CS, falling back to the region's globalCS when the
// fix doesn't override it. Returns trimmed string (empty when neither
// provides a CS).
inline QString resolveFixCS(const cwFixStation& fix, const QString& globalCS)
{
    QString cs = fix.inputCS().trimmed();
    if (cs.isEmpty()) {
        cs = globalCS.trimmed();
    }
    return cs;
}

inline void writeCoordTriplet(QTextStream& stream, double e, double n, double z)
{
    stream << QString::number(e, 'f', 6) << ' '
           << QString::number(n, 'f', 6) << ' '
           << QString::number(z, 'f', 6);
}

// Representative location for a `*declination auto X Y Z` directive, derived
// from a cave's first fix station. Survex IGRF only needs one representative
// point per scope, so additional fixes are intentionally ignored.
struct DeclinationContext {
    QString inputCS;
    double easting = 0.0;
    double northing = 0.0;
    double elevation = 0.0;
};

inline std::optional<DeclinationContext> makeDeclinationContext(
    const QList<cwFixStation>& fixes, const QString& globalCS)
{
    if (fixes.isEmpty()) {
        return std::nullopt;
    }
    const cwFixStation& fix = fixes.first();
    const QString cs = resolveFixCS(fix, globalCS);
    if (cs.isEmpty()) {
        return std::nullopt;
    }
    return DeclinationContext{ cs, fix.easting(), fix.northing(), fix.elevation() };
}

// Emit `*cs <inputCS>` + `*declination auto X Y Z` inside the current trip
// block. Survex re-evaluates IGRF for the trip's `*date`, so the literal
// `*calibrate DECLINATION` line is suppressed by the caller.
inline void writeDeclinationAuto(QTextStream& stream, const DeclinationContext& ctx)
{
    stream << "*cs " << ctx.inputCS << Qt::endl;
    stream << "*declination auto ";
    writeCoordTriplet(stream, ctx.easting, ctx.northing, ctx.elevation);
    stream << Qt::endl;
}

/**
 * Survex requires *cs out whenever any *cs appears, so when the user hasn't
 * set a globalCS but a fix has its own inputCS, fall back to that fix's CS
 * for *cs out — otherwise cavern errors with "input projection is set but
 * output projection isn't" the moment the per-cave *cs is emitted.
 *
 * The Region template parameter is duck-typed: it must expose
 * `.globalCoordinateSystem` and `.caves`, and each cave must expose
 * `.fixStations`. This works for both cwSurveyDataArtifact::Region
 * (Rule export path) and cwCavingRegionData (line-plot export path).
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
            if (!cs.isEmpty()) {
                return cs;
            }
        }
    }
    return QString();
}

// Valid survex *team role keywords (must match role_tab in survex/src/commands.c)
inline bool isValidSurvexRole(const QString& role)
{
    static const QSet<QString> validRoles = {
        "instruments", "insts", "tape", "length", "compass", "bearing",
        "clino", "gradient", "backtape", "backlength", "backcompass",
        "backbearing", "backclino", "backgradient", "station", "position",
        "notes", "notebook", "pictures", "pics", "assistant", "dog",
        "explorer", "altitude", "dz", "dimensions", "left", "right",
        "up", "ceiling", "down", "floor", "count", "counter", "depth"
    };
    return validRoles.contains(role.toLower());
}

//Survex's parser rejects scientific notation (e.g. "1.1e+02"). A reading's
//stored string can be in that form (older imports formatted LRUD with
//QString::number(value, 'g', 2)), so rewrite just those as a plain decimal.
//Two decimals matches the source granularity of such values (Compass stores
//hundredths); any string without an exponent is returned untouched.
inline QString toSurvexNumber(const QString& value)
{
    constexpr int SurvexDecimalPlaces = 2;

    if(!value.contains(QLatin1Char('e'), Qt::CaseInsensitive)) {
        return value;
    }

    bool ok = false;
    const double number = value.toDouble(&ok);
    if(!ok) {
        return value;
    }

    return QString::number(number, 'f', SurvexDecimalPlaces);
}

inline QString verticalClinoText(const cwClinoReading& reading)
{
    if(reading.state() != cwClinoReading::State::Valid) {
        return QString();
    }

    bool ok = false;
    const double value = reading.toDouble(&ok);
    if(!ok) {
        return QString();
    }

    if(qAbs(qAbs(value) - 90.0) >= 0.001) {
        return QString();
    }

    return value >= 0.0 ? QStringLiteral("UP") : QStringLiteral("DOWN");
}

/**
 * Filter fix stations against the cave's actual stations and dedupe by
 * stationName (case-insensitive). Returns the kept fixes; appends one error
 * line per dropped fix to errors.
 *
 * - A fix whose stationName is empty or doesn't match any station name in
 *   stationNamesLower is dropped with "fix references unknown station".
 * - A fix whose (lowercased) stationName collides with an earlier kept fix
 *   is dropped with "duplicate fix on station".
 */
inline QList<cwFixStation> validateFixStations(const QList<cwFixStation>& fixes,
                                               const QSet<QString>& stationNamesLower,
                                               QStringList& errors)
{
    QList<cwFixStation> kept;
    QSet<QString> seenLower;
    kept.reserve(fixes.size());
    for (const cwFixStation& fix : fixes) {
        const QString lower = cwStation::canonicalKey(fix.stationName().trimmed());
        if (lower.isEmpty() || !stationNamesLower.contains(lower)) {
            errors.append(QStringLiteral("Fix references unknown station: \"%1\"")
                              .arg(fix.stationName()));
            continue;
        }
        if (seenLower.contains(lower)) {
            errors.append(QStringLiteral("Duplicate fix on station: \"%1\"")
                              .arg(fix.stationName()));
            continue;
        }
        seenLower.insert(lower);
        kept.append(fix);
    }
    return kept;
}

/**
 * Emit the per-cave *cs / *fix block.
 *
 * - When fixes is non-empty: group by inputCS, emitting `*cs <inputCS>`
 *   before each group and `*fix` per fix. If a fix has empty inputCS but
 *   globalCS is set, globalCS is used as the fallback so survex (which
 *   requires *cs to precede *fix once *cs out is in scope) accepts the
 *   block.
 * - When fixes is empty: fall back to `*fix <fallbackFirstStation> 0 0 0`.
 *   When globalCS is set, prefix it with `*cs <globalCS>` so the legacy
 *   fallback survives the *cs out scope. With no globalCS, no *cs is
 *   emitted (un-fixed projects keep their pre-CS behavior).
 */
inline void writeFixStations(QTextStream& stream,
                             const QList<cwFixStation>& fixes,
                             const QString& fallbackFirstStation,
                             const QString& globalCS = QString())
{
    const QString globalCSTrimmed = globalCS.trimmed();

    if (fixes.isEmpty()) {
        if (!fallbackFirstStation.isEmpty()) {
            if (!globalCSTrimmed.isEmpty()) {
                stream << "*cs " << globalCSTrimmed << Qt::endl;
            }
            stream << "*fix " << fallbackFirstStation << " 0 0 0" << Qt::endl;
        }
        return;
    }

    QString currentCS;
    bool csEmitted = false;
    for (const cwFixStation& fix : fixes) {
        const QString cs = resolveFixCS(fix, globalCSTrimmed);
        if (!csEmitted || cs != currentCS) {
            if (!cs.isEmpty()) {
                stream << "*cs " << cs << Qt::endl;
            }
            currentCS = cs;
            csEmitted = true;
        }
        stream << "*fix " << fix.stationName() << ' ';
        writeCoordTriplet(stream, fix.easting(), fix.northing(), fix.elevation());
        stream << Qt::endl;
    }
}

//True when a shot only carries passage dimensions: a valid zero-length
//distance with no compass or clino (front or back). Compass records LRUD on
//dead-end stations this way, e.g. "ALT13LRUD ALT13 0 - - - -". Survex rejects
//such a leg (omitted compass on a non-plumbed shot), so it's exported as an
//*equate of the two stations instead.
inline bool isLrudOnlyShot(const cwShot& shot)
{
    const cwDistanceReading distance = shot.distance();
    const bool distanceIsZero = distance.state() == cwDistanceReading::State::Valid
                                && qFuzzyIsNull(distance.toDouble());
    return distanceIsZero
        && shot.compass().state()     == cwCompassReading::State::Empty
        && shot.backCompass().state() == cwCompassReading::State::Empty
        && shot.clino().state()       == cwClinoReading::State::Empty
        && shot.backClino().state()   == cwClinoReading::State::Empty;
}

inline bool stationHasLrudData(const cwStation& station)
{
    return station.left().state()  == cwDistanceReading::State::Valid
        || station.right().state() == cwDistanceReading::State::Valid
        || station.up().state()    == cwDistanceReading::State::Valid
        || station.down().state()  == cwDistanceReading::State::Valid;
}
} // namespace cwSurvexExporterUtils

#endif // CWSURVEXEXPORTERUTILS_H
