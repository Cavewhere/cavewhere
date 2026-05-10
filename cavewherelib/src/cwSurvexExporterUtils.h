#ifndef CWSURVEXEXPORTERUTILS_H
#define CWSURVEXEXPORTERUTILS_H

#include <QtGlobal>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QList>
#include <QTextStream>

#include "cwClinoReading.h"
#include "cwFixStation.h"
#include "cwStation.h"

namespace cwSurvexExporterUtils {

/**
 * Survex requires *cs out whenever any *cs appears, so when the user hasn't
 * set a globalCS but a fix has its own inputCS, fall back to that fix's CS
 * for *cs out — otherwise cavern errors with "input projection is set but
 * output projection isn't" the moment the per-cave *cs is emitted.
 *
 * The Region template parameter is duck-typed: it must expose `.globalCS`
 * and `.caves`, and each cave must expose `.fixStations`. This works for
 * both cwSurveyDataArtifact::Region (Rule export path) and
 * cwCavingRegionData (line-plot export path).
 */
template <typename Region>
QString resolveOutputCS(const Region& region)
{
    if (!region.globalCS.isEmpty()) {
        return region.globalCS;
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
        QString cs = fix.inputCS().trimmed();
        if (cs.isEmpty()) {
            cs = globalCSTrimmed;
        }
        if (!csEmitted || cs != currentCS) {
            if (!cs.isEmpty()) {
                stream << "*cs " << cs << Qt::endl;
            }
            currentCS = cs;
            csEmitted = true;
        }
        stream << "*fix " << fix.stationName()
               << ' ' << QString::number(fix.easting(),   'f', 6)
               << ' ' << QString::number(fix.northing(),  'f', 6)
               << ' ' << QString::number(fix.elevation(), 'f', 6)
               << Qt::endl;
    }
}

} // namespace cwSurvexExporterUtils

#endif // CWSURVEXEXPORTERUTILS_H
