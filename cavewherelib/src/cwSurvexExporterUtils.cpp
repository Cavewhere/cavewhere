/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurvexExporterUtils.h"
#include "cwClinoReading.h"
#include "cwCompassReading.h"
#include "cwDistanceReading.h"
#include "cwShot.h"
#include "cwStation.h"

//Qt includes
#include <QTextStream>
#include <QtGlobal>

namespace {

//Survex's parser rejects scientific notation, and two decimals matches the
//source granularity of the values that carry one (Compass stores hundredths).
constexpr int kSurvexDecimalPlaces = 2;

//A clino this far from ±90 isn't a plumb.
constexpr double kPlumbToleranceDegrees = 0.001;

} // namespace

namespace cwSurvexExporterUtils {

void writeCoordTriplet(QTextStream& stream, double e, double n, double z)
{
    stream << QString::number(e, 'f', 6) << ' '
           << QString::number(n, 'f', 6) << ' '
           << QString::number(z, 'f', 6);
}

std::optional<DeclinationContext> makeDeclinationContext(const QList<cwFixStation>& fixes)
{
    // "Mark Station as Fixed" creates a row before the user types anything, and
    // that row carries a coordinate system from the moment it exists. Reading
    // its components anyway would emit `*declination auto 0 0 0` — the Gulf of
    // Guinea, not this cave — and, worse, a `*cs` that lands after the fallback
    // `*fix`, which cavern rejects outright ("fixed before CS command first
    // used"). So skip past anything that isn't a coordinate.
    //
    // Valid is the whole test: a fix with no CS to read its text under is
    // NoSystem, never Valid, so it can't reach the return below.
    for (const cwFixStation& fix : fixes) {
        if (fix.state() != cwFixStation::Valid) {
            continue;
        }
        return DeclinationContext{ fix.inputCS().trimmed(), fix.easting(),
                                   fix.northing(), fix.elevation() };
    }
    return std::nullopt;
}

void writeDeclinationAuto(QTextStream& stream, const DeclinationContext& ctx)
{
    stream << "*cs " << ctx.inputCS << Qt::endl;
    stream << "*declination auto ";
    writeCoordTriplet(stream, ctx.easting, ctx.northing, ctx.elevation);
    stream << Qt::endl;
}

// The list is survex's own (`ok_for_output == NO`, survex/src/commands.c:2521-2540;
// LOCAL and LAT-LONG produce no PROJ string at all and are rejected outright).
//
// PROJ can't create any of them, and cwCoordinateTransform::isGeographic()
// answers false for anything it fails to create — "not geographic" there means
// "couldn't tell", not "projected". So without this list a LONG-LAT fix reads as
// projected and is picked for *cs out, which fails the whole solve even when a
// perfectly good projected fix sits later in the same cave. The other survex
// keywords PROJ can't parse — UTM<zone>N, S-MERC, OSGB:XX, EUR79Z30, IJTSK —
// are all fine for output, which is why this is a list of what to skip rather
// than a blanket "PROJ must understand it" gate: such a gate would drop those
// too and leave the export with no *cs out at all.
bool isUnusableAsSurvexOutputCS(const QString& cs)
{
    static const QSet<QString> unusable = {
        QStringLiteral("long-lat"), QStringLiteral("lat-long"),
        QStringLiteral("jtsk"), QStringLiteral("jtsk03"),
        QStringLiteral("local")
    };
    return unusable.contains(cs.trimmed().toLower());
}

bool isValidSurvexRole(const QString& role)
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

// A reading's stored string can be in exponent form (older imports formatted
// LRUD with QString::number(value, 'g', 2)), so rewrite just those; any string
// without an exponent is returned untouched.
QString toSurvexNumber(const QString& value)
{
    if(!value.contains(QLatin1Char('e'), Qt::CaseInsensitive)) {
        return value;
    }

    bool ok = false;
    const double number = value.toDouble(&ok);
    if(!ok) {
        return value;
    }

    return QString::number(number, 'f', kSurvexDecimalPlaces);
}

QString verticalClinoText(const cwClinoReading& reading)
{
    if(reading.state() != cwClinoReading::State::Valid) {
        return QString();
    }

    bool ok = false;
    const double value = reading.toDouble(&ok);
    if(!ok) {
        return QString();
    }

    if(qAbs(qAbs(value) - 90.0) >= kPlumbToleranceDegrees) {
        return QString();
    }

    return value >= 0.0 ? QStringLiteral("UP") : QStringLiteral("DOWN");
}

// Only a Valid fix has components: every other state reads 0
// (cwFixStation::refresh() zeroes them before it branches), so exporting one
// would anchor the station at the origin and move the whole cave there.
// Dropping it with a message is the only honest option — a coordinate we can't
// read is not a coordinate at 0, 0, 0.
QString fixUnusableReason(const cwFixStation& fix)
{
    switch (fix.state()) {
    case cwFixStation::Valid:
        return QString();
    case cwFixStation::Empty:
        return QStringLiteral("Fix on station \"%1\" has no coordinate")
            .arg(fix.stationName());
    case cwFixStation::NoSystem:
        return QStringLiteral("Fix on station \"%1\" declares no coordinate system, "
                              "so its coordinate can't be read")
            .arg(fix.stationName());
    case cwFixStation::Unreadable:
        return QStringLiteral("Fix on station \"%1\" has a coordinate that can't be read: \"%2\"")
            .arg(fix.stationName(), fix.coordinate());
    }
    return QString();
}

QList<cwFixStation> validateFixStations(const QList<cwFixStation>& fixes,
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
        const QString unusable = fixUnusableReason(fix);
        if (!unusable.isEmpty()) {
            errors.append(unusable);
            continue;
        }
        seenLower.insert(lower);
        kept.append(fix);
    }
    return kept;
}

void writeFixStations(QTextStream& stream,
                      const QList<cwFixStation>& fixes,
                      const QString& fallbackFirstStation,
                      const QString& globalCS)
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
        const QString cs = fix.inputCS().trimmed();
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

bool isLrudOnlyShot(const cwShot& shot)
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

bool stationHasLrudData(const cwStation& station)
{
    return station.left().state()  == cwDistanceReading::State::Valid
        || station.right().state() == cwDistanceReading::State::Valid
        || station.up().state()    == cwDistanceReading::State::Valid
        || station.down().state()  == cwDistanceReading::State::Valid;
}

} // namespace cwSurvexExporterUtils
