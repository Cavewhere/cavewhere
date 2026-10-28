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
#include "cwCoordinateTransform.h"
#include "cwDistanceReading.h"
#include "cwFixStationDiagnostics.h"
#include "cwGridConvergence.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurvexCS.h"

//Qt includes
#include <QTextStream>
#include <QtGlobal>

namespace {

//Survex's parser rejects scientific notation, and two decimals matches the
//source granularity of the values that carry one (Compass stores hundredths).
constexpr int kSurvexDecimalPlaces = 2;

//A coordinate under a geographic *cs is degrees, where the six decimals that
//are micrometers in a projected system are ~11 cm of latitude — enough to pull
//a cave visibly off its LiDAR scan. Nine covers both regimes by overshooting
//rather than by branching on cwCoordinateTransform::isGeographic(), which
//answers only for geographic 2D/3D CRS: a compound or engineering system would
//take the coarse branch and land back on this same bug.
constexpr int kCoordinateDecimalPlaces = 9;

//A clino this far from ±90 isn't a plumb.
constexpr double kPlumbToleranceDegrees = 0.001;

void writeCalibrationLine(QTextStream& stream, const QString& type, double value, double scale,
                          bool skipZero)
{
    if(skipZero && value == 0.0 && scale == 1.0) { return; }
    value = -value; //Flip the value be survex is counter intuitive

    QString calibrationString = QStringLiteral("*calibrate %1 %2")
                                    .arg(type)
                                    .arg(value, 0, 'f', kSurvexDecimalPlaces);

    if(scale != 1.0) {
        calibrationString += QStringLiteral(" %3").arg(scale, 0, 'f', kSurvexDecimalPlaces);
    }

    stream << calibrationString << Qt::endl;
}

} // namespace

namespace cwSurvexExporterUtils {

void writeCoordTriplet(QTextStream& stream, double e, double n, double z)
{
    stream << QString::number(e, 'f', kCoordinateDecimalPlaces) << ' '
           << QString::number(n, 'f', kCoordinateDecimalPlaces) << ' '
           << QString::number(z, 'f', kCoordinateDecimalPlaces);
}

void CsScope::ensure(QTextStream& stream, const QString& cs)
{
    const QString trimmed = cs.trimmed();
    if (trimmed.isEmpty() || trimmed == m_current) {
        return;
    }
    cwSurvexCS::writeCsLine(stream, m_sidecars, trimmed);
    m_current = trimmed;
}

void CsScope::writeFileOutputSystem(QTextStream& stream, const QString& system)
{
    cwSurvexExporterUtils::writeFileOutputSystem(stream, system);
    m_current = system.trimmed();
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

void writeDeclinationAuto(QTextStream& stream, const DeclinationContext& ctx, CsScope& scope)
{
    // The fixes above may have left a different system in scope: this context
    // comes from the first Valid fix, while the *cs standing after them is the
    // last one's. When they agree — the usual case — this writes nothing.
    scope.ensure(stream, ctx.inputCS);
    stream << "*declination auto ";
    writeCoordTriplet(stream, ctx.easting, ctx.northing, ctx.elevation);
    stream << Qt::endl;
}

bool writeStandaloneTripHeader(QTextStream& stream,
                               cwSurvexCS::SidecarWriter& sidecars,
                               const QList<cwFixStation>& fixes,
                               bool tripUsesAuto)
{
    if (!tripUsesAuto) {
        return false;
    }

    const auto ctx = makeDeclinationContext(fixes);
    if (!ctx) {
        return false;
    }

    const QString outputCS = shareableCSForFixes(fixes);
    if (outputCS.isEmpty()) {
        return false;
    }

    CsScope scope(sidecars);
    cwSurvexCS::writeCsLine(stream, sidecars, outputCS, true);
    writeDeclinationAuto(stream, *ctx, scope);
    stream << Qt::endl;
    return true;
}

void writeCalibration(QTextStream& stream, const QString& type, double value, double scale)
{
    writeCalibrationLine(stream, type, value, scale, /*skipZero*/ true);
}

void writeDeclinationCalibration(QTextStream& stream,
                                 bool autoDeclination,
                                 double declination,
                                 bool autoDeclinationInScope,
                                 double gridConvergence)
{
    if (autoDeclination && autoDeclinationInScope) {
        return; //inherits the cave block's *declination auto
    }

    if (gridConvergence != 0.0) {
        //Say so in the file, or a reader comparing it against the declination
        //they typed concludes CaveWhere got their declination wrong. The wording
        //deliberately spells no survex directive: a reader grepping the export
        //for one should find only the lines that are actually directives.
        stream << QStringLiteral("; Declination %1 is magnetic; %2 of grid convergence "
                                 "subtracted so the survey plots to grid north, as the "
                                 "auto-declination path would have done.")
                      .arg(declination, 0, 'f', kSurvexDecimalPlaces)
                      .arg(gridConvergence, 0, 'f', kSurvexDecimalPlaces)
               << Qt::endl;
    }

    writeCalibrationLine(stream, QStringLiteral("DECLINATION"), declination - gridConvergence, 1.0,
                         /*skipZero*/ !autoDeclinationInScope);
}

void writeDeclinationReset(QTextStream& stream)
{
    writeCalibrationLine(stream, QStringLiteral("DECLINATION"), 0.0, 1.0, /*skipZero*/ false);
}

void writeFileOutputSystem(QTextStream& stream, const QString& system)
{
    stream << "*cs out " << system << Qt::endl;
    stream << "*cs " << system << Qt::endl;
}

double gridConvergenceForBlock(const std::optional<DeclinationContext>& ctx,
                               const QString& outputCS)
{
    if (!ctx || outputCS.trimmed().isEmpty()) {
        return 0.0;
    }

    const auto inOutput = cwCoordinateTransform::transformPoint(
        ctx->inputCS, outputCS, cwGeoPoint(ctx->easting, ctx->northing, ctx->elevation));
    if (!inOutput.has_value()) {
        return 0.0;
    }

    const auto convergence = cwGridConvergence::computeAt(*inOutput, outputCS);
    return convergence.hasError() ? 0.0 : convergence.value();
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
// read is not a coordinate at 0, 0, 0. A Valid fix outside its own system's
// domain is dropped too: cwLocalProjectionManager leaves it out of the frame, so
// writing it would put a *cs in a solve whose *cs out the frame never named.
// So is one whose system PROJ reads as an operation such as a pipeline: it has
// no domain to check against, and cavern can't take it as a *cs.
QString fixUnusableReason(const cwFixStation& fix)
{
    switch (fix.state()) {
    case cwFixStation::Valid:
        if (!cwCoordinateTransform::isValidCS(fix.inputCS())) {
            return QStringLiteral("Fix on station \"%1\" uses \"%2\", which is not a "
                                  "coordinate system")
                .arg(fix.stationName(), fix.inputCS());
        }
        if (cwFixStationDiagnostics::isDomainValid(fix)) {
            return QString();
        }
        return QStringLiteral("Fix on station \"%1\" has a coordinate outside the valid range "
                              "for its coordinate system")
            .arg(fix.stationName());
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
                                        const QSet<QString>& stationKeys,
                                        QStringList& errors)
{
    QList<cwFixStation> kept;
    QSet<QString> seenKeys;
    kept.reserve(fixes.size());
    for (const cwFixStation& fix : fixes) {
        const QString key = cwStation::keyIn(fix.stationName().trimmed(), stationKeys);
        if (key.isEmpty() || !stationKeys.contains(key)) {
            errors.append(QStringLiteral("Fix references unknown station: \"%1\"")
                              .arg(fix.stationName()));
            continue;
        }
        if (seenKeys.contains(key)) {
            errors.append(QStringLiteral("Duplicate fix on station: \"%1\"")
                              .arg(fix.stationName()));
            continue;
        }
        const QString unusable = fixUnusableReason(fix);
        if (!unusable.isEmpty()) {
            errors.append(unusable);
            continue;
        }
        seenKeys.insert(key);
        kept.append(fix);
    }
    return kept;
}

bool keepsCase(const QString& name)
{
    return name != cwStation::canonicalKey(name);
}

QHash<QString, QString> stationKeysBySpelling(const QString& scope, const QStringList& spellings)
{
    QHash<QString, int> spellingsPerKey;
    spellingsPerKey.reserve(spellings.size());
    for (const QString& spelling : spellings) {
        ++spellingsPerKey[cwStation::canonicalKey(spelling)];
    }

    const QString scopeKey = cwStation::canonicalKey(scope);
    QHash<QString, QString> keys;
    keys.reserve(spellings.size());
    for (const QString& spelling : spellings) {
        const QString canonical = cwStation::canonicalKey(spelling);
        const bool isCasePair = spellingsPerKey.value(canonical) > 1;
        keys.insert(spelling, scopeKey + (isCasePair ? spelling : canonical));
    }
    return keys;
}

QSet<QString> scopedStationKeys(const QString& scope, const QStringList& stations)
{
    const QHash<QString, QString> keys = stationKeysBySpelling(scope, stations);
    return QSet<QString>(keys.cbegin(), keys.cend());
}

namespace {

//! Writes \a line inside its own *case preserve block when \a name has a
//! capital, so cavern reads the station as written.
void writeStationLine(QTextStream& stream, const QString& name, const QString& line)
{
    if (keepsCase(name)) {
        stream << "*begin" << Qt::endl
               << "*case preserve" << Qt::endl
               << line << Qt::endl
               << "*end" << Qt::endl;
    } else {
        stream << line << Qt::endl;
    }
}

} // namespace

QString StationKeyUnion::root(const QString& key) const
{
    QString current = key;
    QString parent = m_parents.value(current, current);
    while (parent != current) {
        current = parent;
        parent = m_parents.value(current, current);
    }
    return current;
}

void StationKeyUnion::join(const QString& first, const QString& second)
{
    const QString firstRoot = root(first);
    m_parents.insert(root(second), firstRoot);
    // Point both keys straight at the root so later walks stay short.
    m_parents.insert(first, firstRoot);
    m_parents.insert(second, firstRoot);
}

void writeFixStations(QTextStream& stream,
                      const QList<cwFixStation>& fixes,
                      const QString& fallbackFirstStation,
                      const QString& globalCS,
                      CsScope& scope)
{
    if (fixes.isEmpty()) {
        if (!fallbackFirstStation.isEmpty()) {
            scope.ensure(stream, globalCS);
            writeStationLine(stream, fallbackFirstStation,
                             QStringLiteral("*fix %1 0 0 0").arg(fallbackFirstStation));
        }
        return;
    }

    for (const cwFixStation& fix : fixes) {
        // Before the block: cavern drops a *cs at the block's *end.
        scope.ensure(stream, fix.inputCS());
        QString line;
        QTextStream lineStream(&line);
        lineStream << "*fix " << fix.stationName() << ' ';
        writeCoordTriplet(lineStream, fix.easting(), fix.northing(), fix.elevation());
        lineStream.flush();
        writeStationLine(stream, fix.stationName(), line);
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

bool isQuotableIncludePath(const QString& path)
{
    return !path.contains(QLatin1Char('"'))
        && !path.contains(QLatin1Char('\n'))
        && !path.contains(QLatin1Char('\r'))
        && !path.contains(QLatin1Char('\032'));
}

} // namespace cwSurvexExporterUtils
