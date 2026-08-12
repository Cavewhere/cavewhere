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

namespace {

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

} // namespace

bool writeBlockDeclinationAuto(QTextStream& stream,
                               const QList<cwFixStation>& fixes,
                               bool anyTripUsesAuto,
                               CsScope& scope)
{
    if (!anyTripUsesAuto) {
        return false;
    }

    const auto ctx = makeDeclinationContext(fixes);
    if (!ctx) {
        return false;
    }

    writeDeclinationAuto(stream, *ctx, scope);
    return true;
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
                      const QString& globalCS,
                      CsScope& scope)
{
    if (fixes.isEmpty()) {
        if (!fallbackFirstStation.isEmpty()) {
            scope.ensure(stream, globalCS);
            stream << "*fix " << fallbackFirstStation << " 0 0 0" << Qt::endl;
        }
        return;
    }

    for (const cwFixStation& fix : fixes) {
        scope.ensure(stream, fix.inputCS());
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
