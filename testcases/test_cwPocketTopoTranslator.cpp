/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCavernRunner.h"
#include "cwPocketTopoFile.h"
#include "cwPocketTopoTranslator.h"
#include "cwSurvex3DFileReader.h"
#include "cwSurvexCS.h"
#include "cwSurvexExporterUtils.h"

// Test helpers
#include "PocketTopoFixtureWriter.h"

// Qt
#include <QDate>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector3D>

using cwPocketTopoFile::Reference;
using cwPocketTopoFile::Shot;
using cwPocketTopoFile::StationId;
using cwPocketTopoFile::Trip;
using PocketTopoFixtureWriter::Fixture;

namespace {

const QString kHeader = QStringLiteral("*units tape metres\n"
                                       "*units compass clino degrees\n"
                                       "*data normal from to tape compass clino\n");
const QString kUtm13N = QStringLiteral("EPSG:32613");
const QString kNad27Utm13N = QStringLiteral("EPSG:26713");
const QString kWgs84LatLong = QStringLiteral("EPSG:4326");
const QString kRootLabel = QStringLiteral("pt");

constexpr int kMillimetersPerMeter = 1000;
constexpr qint16 kNoTrip = Shot::kNoTrip;
constexpr qint16 kOutOfRangeTrip = 5;
// 1234 / 65536 of a circle, 6.78 degrees.
constexpr qint16 kEastDeclinationUnits = 1234;
// -2000 / 65536 of a circle, -10.99 degrees.
constexpr qint16 kWestDeclinationUnits = -2000;
constexpr double kInjectedDeclination = 3.5;
constexpr double kQuarterTurn = 90.0;

// The node's location for `*declination auto`, in WGS84 UTM zone 13N.
constexpr double kBaseEasting = 478000.0;
constexpr double kBaseNorthing = 4430000.0;
constexpr double kBaseElevation = 1655.0;

// The end-to-end fixture's reference on 1.0, in millimeters.
constexpr qint64 kFixEastMm = 1000000;
constexpr qint64 kFixNorthMm = 2000000;
constexpr qint32 kFixAltitudeMm = 300000;
constexpr double kPositionMargin = 0.01;

StationId dotted(quint16 major, quint16 minor)
{
    return StationId::dotted(major, minor);
}

Trip trip(const QDate& date, const QString& comment, qint16 declination = 0)
{
    Trip result;
    result.ticks = PocketTopoFixtureWriter::ticks(date);
    result.comment = comment;
    result.declination = declination;
    return result;
}

Shot shot(StationId from, StationId to, double meters, double azimuth, double clino, qint16 tripIndex)
{
    Shot result;
    result.from = from;
    result.to = to;
    result.distance = qint32(qRound(meters * kMillimetersPerMeter));
    result.azimuth = PocketTopoFixtureWriter::azimuthUnits(azimuth);
    result.inclination = PocketTopoFixtureWriter::angleUnits(clino);
    result.tripIndex = tripIndex;
    return result;
}

Shot splay(StationId from, double meters, double azimuth, double clino, qint16 tripIndex)
{
    return shot(from, StationId(), meters, azimuth, clino, tripIndex);
}

Reference reference(StationId station, qint64 eastMm, qint64 northMm, qint32 altitudeMm)
{
    Reference result;
    result.station = station;
    result.east = eastMm;
    result.north = northMm;
    result.altitude = altitudeMm;
    return result;
}

//! The translation of \a fixture written to a temporary .top, with \a input
//! (its topPath replaced) and \a initialSystem in scope around it. The
//! translation must exist.
QString translatedText(const Fixture& fixture,
                       cwPocketTopoTranslator::Input input = {},
                       const QString& initialSystem = QString())
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = QDir(dir.path()).absoluteFilePath(QStringLiteral("survey.top"));
    REQUIRE(PocketTopoFixtureWriter::writeFile(fixture, path));
    input.topPath = path;

    cwSurvexCS::SidecarWriter sidecars;
    cwSurvexExporterUtils::CsScope scope(sidecars);
    QString preamble;
    QTextStream preambleStream(&preamble);
    scope.ensure(preambleStream, initialSystem);

    const std::optional<QString> text = cwPocketTopoTranslator::translate(input, scope);
    REQUIRE(text.has_value());
    return *text;
}

QString triplet(double easting, double northing, double elevation)
{
    QString text;
    QTextStream stream(&text);
    cwSurvexExporterUtils::writeCoordTriplet(stream, easting, northing, elevation);
    stream.flush();
    return text;
}

Fixture oneTripFixture(const QList<Shot>& shots, const QString& comment = QStringLiteral("Robin survey"))
{
    Fixture fixture;
    fixture.document.trips = {trip(QDate(2026, 2, 5), comment)};
    fixture.document.shots = shots;
    return fixture;
}

//! The text up to and including trip #1's `*declination` line for a manual
//! zero declination, which the shot-layout tests share.
QString tripOnePrelude(const QString& name = QStringLiteral("2026-02-05 Robin survey"))
{
    return kHeader
           + QStringLiteral("\n; trip #1 : ") + name + QStringLiteral("\n"
                            "*date 2026.02.05\n"
                            "*declination 0.00 degrees\n");
}

//! The number \a pattern captures in cavern's log, or -1 when it matches nothing.
double logNumber(const QString& logText, const QString& pattern)
{
    const QRegularExpression expression(pattern);
    const QRegularExpressionMatch match = expression.match(logText);
    if (!match.hasMatch()) {
        return -1.0;
    }
    return match.captured(1).toDouble();
}

//! Runs cavern on \a translation wrapped in the driver's `*begin pt` block.
cwCavernRunner::Result solve(const QTemporaryDir& dir, const QString& translation)
{
    const QString driverPath = QDir(dir.path()).absoluteFilePath(QStringLiteral("driver.svx"));
    QFile driver(driverPath);
    REQUIRE(driver.open(QFile::WriteOnly | QFile::Text));
    driver.write((QStringLiteral("*begin ") + kRootLabel + QStringLiteral("\n") + translation
                  + QStringLiteral("*end ") + kRootLabel + QStringLiteral("\n")).toUtf8());
    driver.close();

    const auto ran = cwCavernRunner::run(driverPath, driverPath + QStringLiteral(".3d"));
    INFO("cavern: " << ran.errorMessage().toStdString());
    REQUIRE_FALSE(ran.hasError());
    return ran.value();
}

} // namespace

TEST_CASE("cwPocketTopoTranslator writes repeated readings together, in one direction, with splays after",
          "[PocketTopo][Translator]")
{
    const Fixture fixture = oneTripFixture({
        splay(dotted(1, 0), 2.0, 10.0, 0.0, 0),
        shot(dotted(1, 0), dotted(1, 1), 3.38, 271.35, -1.51, 0),
        splay(dotted(1, 0), 1.54, 333.43, 5.91, 0),
        shot(dotted(1, 0), dotted(1, 1), 3.38, 271.35, -1.69, 0),
        // The same leg read from 1.1 back to 1.0.
        shot(dotted(1, 1), dotted(1, 0), 3.38, 91.36, 1.77, 0),
        splay(dotted(1, 1), 3.93, 223.94, 26.42, 0),
        shot(dotted(1, 1), dotted(1, 2), 3.06, 301.39, -4.19, 0),
    });

    const QString expected = tripOnePrelude()
                             + QStringLiteral("*flags splay\n"
                                              "1.0 .. 2.000 10.00 0.00\n"
                                              "*flags not splay\n"
                                              "1.0 1.1 3.380 271.35 -1.51\n"
                                              "1.0 1.1 3.380 271.35 -1.69\n"
                                              "1.0 1.1 3.380 271.36 -1.77\n"
                                              "*flags splay\n"
                                              "1.0 .. 1.540 333.43 5.91\n"
                                              "1.1 .. 3.930 223.94 26.42\n"
                                              "*flags not splay\n"
                                              "1.1 1.2 3.060 301.39 -4.19\n");
    const QString text = translatedText(fixture);
    INFO("translation:\n" << text.toStdString());
    CHECK(text == expected);
}

TEST_CASE("cwPocketTopoTranslator keeps a repeat that another leg separates as its own leg",
          "[PocketTopo][Translator]")
{
    const Fixture fixture = oneTripFixture({
        shot(dotted(1, 0), dotted(1, 1), 5.0, 0.0, 0.0, 0),
        shot(dotted(1, 1), dotted(1, 2), 5.0, 90.0, 0.0, 0),
        // Not part of the first group, so written the way it was read.
        shot(dotted(1, 1), dotted(1, 0), 4.99, 180.0, 0.0, 0),
    });

    const QString expected = tripOnePrelude()
                             + QStringLiteral("1.0 1.1 5.000 0.00 0.00\n"
                                              "1.1 1.2 5.000 90.00 0.00\n"
                                              "1.1 1.0 4.990 180.00 0.00\n");
    const QString text = translatedText(fixture);
    INFO("translation:\n" << text.toStdString());
    CHECK(text == expected);
}

TEST_CASE("cwPocketTopoTranslator writes a zero-length named leg as *equate",
          "[PocketTopo][Translator]")
{
    const Fixture fixture = oneTripFixture({
        shot(dotted(1, 1), dotted(1, 2), 3.06, 301.39, -4.19, 0),
        shot(dotted(1, 2), dotted(2, 0), 0.0, 0.0, 0.0, 0),
        shot(dotted(2, 0), dotted(2, 1), 2.5, 45.0, 2.5, 0),
    });

    const QString expected = tripOnePrelude()
                             + QStringLiteral("1.1 1.2 3.060 301.39 -4.19\n"
                                              "*equate 1.2 2.0\n"
                                              "2.0 2.1 2.500 45.00 2.50\n");
    const QString text = translatedText(fixture);
    INFO("translation:\n" << text.toStdString());
    CHECK(text == expected);
}

TEST_CASE("cwPocketTopoTranslator leaves out shots that belong to no trip",
          "[PocketTopo][Translator]")
{
    const Fixture fixture = oneTripFixture({
        // PocketTopo's leading zero-length row, which marks the start station.
        splay(dotted(1, 0), 0.0, 0.0, 0.0, kNoTrip),
        shot(dotted(1, 0), dotted(1, 1), 5.0, 0.0, 0.0, 0),
        shot(dotted(1, 1), dotted(1, 5), 4.0, 10.0, 0.0, kNoTrip),
        shot(dotted(1, 1), dotted(1, 6), 4.0, 20.0, 0.0, kOutOfRangeTrip),
        shot(dotted(1, 1), dotted(1, 2), 5.0, 90.0, 0.0, 0),
    });

    const QString expected = tripOnePrelude()
                             + QStringLiteral("1.0 1.1 5.000 0.00 0.00\n"
                                              "1.1 1.2 5.000 90.00 0.00\n");
    const QString text = translatedText(fixture);
    INFO("translation:\n" << text.toStdString());
    CHECK(text == expected);
}

TEST_CASE("cwPocketTopoTranslator heads each trip with its number, date, name and team",
          "[PocketTopo][Translator]")
{
    Fixture fixture;
    fixture.document.trips = {
        trip(QDate(2026, 2, 5), QStringLiteral("Robin survey\rBecka\r\rSam Smith")),
        trip(QDate(2026, 3, 14), QString()),
    };
    fixture.document.shots = {
        shot(dotted(1, 0), dotted(1, 1), 5.0, 0.0, 0.0, 0),
        shot(dotted(1, 1), dotted(1, 2), 5.0, 90.0, 0.0, 1),
        shot(dotted(1, 1), dotted(1, 3), 2.0, 180.0, 0.0, 0),
    };

    const QString expected = kHeader
                             + QStringLiteral("\n; trip #1 : 2026-02-05 Robin survey\n"
                                              "*date 2026.02.05\n"
                                              "*declination 0.00 degrees\n"
                                              "*team \"Becka\"\n"
                                              "*team \"Sam Smith\"\n"
                                              "1.0 1.1 5.000 0.00 0.00\n"
                                              "1.1 1.3 2.000 180.00 0.00\n"
                                              "\n; trip #2 : 2026-03-14\n"
                                              "*date 2026.03.14\n"
                                              "*declination 0.00 degrees\n"
                                              "1.1 1.2 5.000 90.00 0.00\n");
    const QString text = translatedText(fixture);
    INFO("translation:\n" << text.toStdString());
    CHECK(text == expected);
}

TEST_CASE("cwPocketTopoTranslator writes a manual declination as the file's own value",
          "[PocketTopo][Translator]")
{
    Fixture fixture;
    fixture.document.trips = {
        trip(QDate(2026, 2, 5), QString(), kEastDeclinationUnits),
        trip(QDate(2026, 2, 6), QString(), kWestDeclinationUnits),
    };
    fixture.document.shots = {
        shot(dotted(1, 0), dotted(1, 1), 5.0, 0.0, 0.0, 0),
        shot(dotted(1, 1), dotted(1, 2), 5.0, 90.0, 0.0, 1),
    };

    // A location and injected values are for Auto trips only.
    cwPocketTopoTranslator::Input input;
    input.autoDeclinationLocation =
        cwSurvexExporterUtils::DeclinationContext{kUtm13N, kBaseEasting, kBaseNorthing, kBaseElevation};
    input.injectedDeclinations = {{0, kInjectedDeclination}, {1, kInjectedDeclination}};

    const QString expected = kHeader
                             + QStringLiteral("\n; trip #1 : 2026-02-05\n"
                                              "*date 2026.02.05\n"
                                              "*declination 6.78 degrees\n"
                                              "1.0 1.1 5.000 0.00 0.00\n"
                                              "\n; trip #2 : 2026-02-06\n"
                                              "*date 2026.02.06\n"
                                              "*declination -10.99 degrees\n"
                                              "1.1 1.2 5.000 90.00 0.00\n");
    const QString text = translatedText(fixture, input, kUtm13N);
    INFO("translation:\n" << text.toStdString());
    CHECK(text == expected);
}

TEST_CASE("cwPocketTopoTranslator takes an Auto trip's declination at the node's location",
          "[PocketTopo][Translator]")
{
    Fixture fixture;
    fixture.document.trips = {
        trip(QDate(2026, 2, 5), QString(), Trip::kAutoDeclination),
        // A manual trip between two Auto ones: each trip states its own.
        trip(QDate(2026, 2, 6), QString(), kEastDeclinationUnits),
        trip(QDate(2026, 2, 7), QString(), Trip::kAutoDeclination),
    };
    fixture.document.shots = {
        shot(dotted(1, 0), dotted(1, 1), 5.0, 0.0, 0.0, 0),
        shot(dotted(1, 1), dotted(1, 2), 5.0, 90.0, 0.0, 1),
        shot(dotted(1, 2), dotted(1, 3), 5.0, 180.0, 0.0, 2),
    };

    cwPocketTopoTranslator::Input input;
    input.autoDeclinationLocation =
        cwSurvexExporterUtils::DeclinationContext{kUtm13N, kBaseEasting, kBaseNorthing, kBaseElevation};
    input.injectedDeclinations = {{0, kInjectedDeclination}};

    const QString autoLine = QStringLiteral("*declination auto ")
                             + triplet(kBaseEasting, kBaseNorthing, kBaseElevation)
                             + QStringLiteral("\n");

    SECTION("The location's system is already in scope")
    {
        const QString expected = kHeader
                                 + QStringLiteral("\n; trip #1 : 2026-02-05\n"
                                                  "*date 2026.02.05\n")
                                 + autoLine
                                 + QStringLiteral("1.0 1.1 5.000 0.00 0.00\n"
                                                  "\n; trip #2 : 2026-02-06\n"
                                                  "*date 2026.02.06\n"
                                                  "*declination 6.78 degrees\n"
                                                  "1.1 1.2 5.000 90.00 0.00\n"
                                                  "\n; trip #3 : 2026-02-07\n"
                                                  "*date 2026.02.07\n")
                                 + autoLine
                                 + QStringLiteral("1.2 1.3 5.000 180.00 0.00\n");
        const QString text = translatedText(fixture, input, kUtm13N);
        INFO("translation:\n" << text.toStdString());
        CHECK(text == expected);
    }

    SECTION("Another system is in scope")
    {
        const QString expected = kHeader
                                 + QStringLiteral("\n; trip #1 : 2026-02-05\n"
                                                  "*date 2026.02.05\n"
                                                  "*cs EPSG:32613\n")
                                 + autoLine
                                 + QStringLiteral("1.0 1.1 5.000 0.00 0.00\n"
                                                  "\n; trip #2 : 2026-02-06\n"
                                                  "*date 2026.02.06\n"
                                                  "*declination 6.78 degrees\n"
                                                  "1.1 1.2 5.000 90.00 0.00\n"
                                                  "\n; trip #3 : 2026-02-07\n"
                                                  "*date 2026.02.07\n")
                                 + autoLine
                                 + QStringLiteral("1.2 1.3 5.000 180.00 0.00\n");
        const QString text = translatedText(fixture, input, kWgs84LatLong);
        INFO("translation:\n" << text.toStdString());
        CHECK(text == expected);
    }
}

TEST_CASE("cwPocketTopoTranslator writes the injected value for an Auto trip with no location",
          "[PocketTopo][Translator]")
{
    Fixture fixture;
    fixture.document.trips = {
        trip(QDate(2026, 2, 5), QString(), Trip::kAutoDeclination),
        trip(QDate(2026, 2, 6), QString(), Trip::kAutoDeclination),
    };
    fixture.document.shots = {
        shot(dotted(1, 0), dotted(1, 1), 5.0, 0.0, 0.0, 0),
        shot(dotted(1, 1), dotted(1, 2), 5.0, 90.0, 0.0, 1),
    };

    // Trip #2 has no entry, so it gets zero.
    cwPocketTopoTranslator::Input input;
    input.injectedDeclinations = {{0, kInjectedDeclination}};

    const QString expected = kHeader
                             + QStringLiteral("\n; trip #1 : 2026-02-05\n"
                                              "*date 2026.02.05\n"
                                              "*declination 3.50 degrees\n"
                                              "1.0 1.1 5.000 0.00 0.00\n"
                                              "\n; trip #2 : 2026-02-06\n"
                                              "*date 2026.02.06\n"
                                              "*declination 0.00 degrees\n"
                                              "1.1 1.2 5.000 90.00 0.00\n");
    const QString text = translatedText(fixture, input);
    INFO("translation:\n" << text.toStdString());
    CHECK(text == expected);
}

TEST_CASE("cwPocketTopoTranslator writes references as *fix in meters under the fix system",
          "[PocketTopo][Translator]")
{
    Fixture fixture = oneTripFixture({shot(dotted(1, 0), dotted(1, 1), 5.0, 0.0, 0.0, 0)}, QString());
    fixture.document.references = {
        reference(dotted(1, 0), 500123456, 4500321000LL, 812345),
        reference(dotted(1, 1), 500123456, 4500326000LL, 812345),
    };
    const QString fixes = QStringLiteral("*fix 1.0 ") + triplet(500123.456, 4500321.0, 812.345)
                          + QStringLiteral("\n*fix 1.1 ") + triplet(500123.456, 4500326.0, 812.345)
                          + QStringLiteral("\n");
    const QString trips = tripOnePrelude(QStringLiteral("2026-02-05"))
                          + QStringLiteral("1.0 1.1 5.000 0.00 0.00\n");

    SECTION("No fix system is chosen")
    {
        const QString text = translatedText(fixture);
        INFO("translation:\n" << text.toStdString());
        CHECK(text == trips + QStringLiteral("\n") + fixes);
    }

    SECTION("The fix system is already in scope")
    {
        cwPocketTopoTranslator::Input input;
        input.fixCS = kUtm13N;
        const QString text = translatedText(fixture, input, kUtm13N);
        INFO("translation:\n" << text.toStdString());
        CHECK(text == trips + QStringLiteral("\n") + fixes);
    }

    SECTION("An Auto trip's location left another system in scope")
    {
        fixture.document.trips.first().declination = Trip::kAutoDeclination;
        cwPocketTopoTranslator::Input input;
        input.fixCS = kNad27Utm13N;
        input.autoDeclinationLocation =
            cwSurvexExporterUtils::DeclinationContext{kUtm13N, kBaseEasting, kBaseNorthing, kBaseElevation};
        const QString expected = kHeader
                                 + QStringLiteral("\n; trip #1 : 2026-02-05\n"
                                                  "*date 2026.02.05\n"
                                                  "*cs EPSG:32613\n"
                                                  "*declination auto ")
                                 + triplet(kBaseEasting, kBaseNorthing, kBaseElevation)
                                 + QStringLiteral("\n1.0 1.1 5.000 0.00 0.00\n"
                                                  "\n*cs EPSG:26713\n")
                                 + fixes;
        const QString text = translatedText(fixture, input, kNad27Utm13N);
        INFO("translation:\n" << text.toStdString());
        CHECK(text == expected);
    }
}

TEST_CASE("cwPocketTopoTranslator renames a plain station that shares a number with a series",
          "[PocketTopo][Translator]")
{
    Fixture fixture = oneTripFixture({
        shot(StationId::plain(0), dotted(0, 0), 2.0, 90.0, 0.0, 0),
        shot(dotted(0, 0), StationId::plain(7), 2.0, 90.0, 0.0, 0),
        shot(StationId::plain(7), dotted(1, 5), 2.0, 90.0, 0.0, 0),
    }, QString());
    fixture.document.references = {reference(StationId::plain(0), 0, 0, 0)};

    const QHash<quint32, QString> names = cwPocketTopoTranslator::stationNames(fixture.document);
    CHECK(names.size() == 4);
    CHECK(names.value(StationId::plain(0).raw()) == QStringLiteral("0_"));
    CHECK(names.value(dotted(0, 0).raw()) == QStringLiteral("0.0"));
    CHECK(names.value(StationId::plain(7).raw()) == QStringLiteral("7"));
    CHECK(names.value(dotted(1, 5).raw()) == QStringLiteral("1.5"));
    CHECK_FALSE(names.contains(StationId().raw()));

    const QString expected = tripOnePrelude(QStringLiteral("2026-02-05"))
                             + QStringLiteral("0_ 0.0 2.000 90.00 0.00\n"
                                              "0.0 7 2.000 90.00 0.00\n"
                                              "7 1.5 2.000 90.00 0.00\n"
                                              "\n*fix 0_ ")
                             + triplet(0.0, 0.0, 0.0) + QStringLiteral("\n");
    const QString text = translatedText(fixture);
    INFO("translation:\n" << text.toStdString());
    CHECK(text == expected);
}

TEST_CASE("cwPocketTopoTranslator returns nothing for a file it cannot read",
          "[PocketTopo][Translator]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString missing = QDir(dir.path()).absoluteFilePath(QStringLiteral("missing.top"));
    const QString notTop = QDir(dir.path()).absoluteFilePath(QStringLiteral("not.top"));
    QFile file(notTop);
    REQUIRE(file.open(QFile::WriteOnly));
    file.write(QByteArrayLiteral("*begin not a top file\n"));
    file.close();

    cwSurvexCS::SidecarWriter sidecars;
    cwSurvexExporterUtils::CsScope scope(sidecars);
    cwPocketTopoTranslator::Input input;
    input.topPath = missing;
    CHECK_FALSE(cwPocketTopoTranslator::translate(input, scope).has_value());
    input.topPath = notTop;
    CHECK_FALSE(cwPocketTopoTranslator::translate(input, scope).has_value());
}

TEST_CASE("Cavern solves a PocketTopo translation with repeated readings merged",
          "[PocketTopo][Translator][Cavern]")
{
    Fixture fixture;
    fixture.document.trips = {
        trip(QDate(2026, 2, 5), QStringLiteral("Robin survey")),
        trip(QDate(2026, 2, 6), QString()),
    };
    fixture.document.shots = {
        splay(dotted(1, 0), 0.0, 0.0, 0.0, kNoTrip),
        // A triple with a splay inside it and one reading taken backwards.
        shot(dotted(1, 0), dotted(1, 1), 5.0, 0.0, 0.0, 0),
        splay(dotted(1, 0), 1.5, 90.0, 0.0, 0),
        shot(dotted(1, 0), dotted(1, 1), 5.01, 0.25, 0.0, 0),
        shot(dotted(1, 1), dotted(1, 0), 4.99, 180.0, 0.0, 0),
        shot(dotted(1, 1), dotted(1, 2), 5.0, 90.0, 0.0, 0),
        // Closes the one loop.
        shot(dotted(1, 2), dotted(1, 0), 7.07, 225.0, 0.0, 0),
        // Series 2 joins series 1, then runs to a plain 0 and on to 0.0.
        shot(dotted(1, 2), dotted(2, 0), 0.0, 0.0, 0.0, 1),
        shot(dotted(2, 0), dotted(2, 1), 3.0, 90.0, 0.0, 1),
        shot(dotted(2, 0), dotted(2, 1), 3.0, 90.0, 0.0, 1),
        shot(dotted(2, 0), dotted(2, 1), 3.0, 90.0, 0.0, 1),
        shot(dotted(2, 1), StationId::plain(0), 2.0, 90.0, 0.0, 1),
        shot(StationId::plain(0), dotted(0, 0), 2.0, 90.0, 0.0, 1),
    };
    fixture.document.references = {reference(dotted(1, 0), kFixEastMm, kFixNorthMm, kFixAltitudeMm)};

    const QString text = translatedText(fixture);
    INFO("translation:\n" << text.toStdString());
    REQUIRE(text.contains(QStringLiteral("*data normal from to tape compass clino")));

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const cwCavernRunner::Result result = solve(dir, text);
    INFO("cavern log:\n" << result.logText.toStdString());

    // Each triple counts as one leg and closes no loop of its own; the splay
    // and the equate add no length.
    CHECK(logNumber(result.logText, QStringLiteral("joined by (\\d+) (?:legs|shots)")) == 8);
    CHECK(result.logText.contains(QStringLiteral("There is 1 loop.")));
    CHECK(logNumber(result.logText, QStringLiteral("Total length of survey (?:legs|shots) =\\s*([\\d.]+)m"))
          == Catch::Approx(24.07));

    cwSurvex3DFileReader reader;
    const cwStationPositionLookup lookup = reader.readNetworkAndLookup(result.output3dPath).lookup;
    for (const QString& station : {QStringLiteral("1.0"), QStringLiteral("1.1"), QStringLiteral("1.2"),
                                   QStringLiteral("2.0"), QStringLiteral("2.1"), QStringLiteral("0_"),
                                   QStringLiteral("0.0")}) {
        INFO("station " << station.toStdString());
        CHECK(lookup.hasPosition(kRootLabel + QStringLiteral(".") + station));
    }
    CHECK_FALSE(lookup.hasPosition(kRootLabel + QStringLiteral(".0")));

    REQUIRE(lookup.hasPosition(QStringLiteral("pt.1.0")));
    const QVector3D fixed = lookup.position(QStringLiteral("pt.1.0"));
    CHECK(fixed.x() == Catch::Approx(1000.0).margin(kPositionMargin));
    CHECK(fixed.y() == Catch::Approx(2000.0).margin(kPositionMargin));
    CHECK(fixed.z() == Catch::Approx(300.0).margin(kPositionMargin));
    REQUIRE(lookup.hasPosition(QStringLiteral("pt.0.0")));
    CHECK(lookup.position(QStringLiteral("pt.0.0")).x() == Catch::Approx(1012.0).margin(kPositionMargin));
}

TEST_CASE("Cavern turns a PocketTopo trip's legs by its manual declination",
          "[PocketTopo][Translator][Cavern]")
{
    Fixture fixture;
    fixture.document.trips = {
        trip(QDate(2026, 2, 5), QString(), PocketTopoFixtureWriter::angleUnits(kQuarterTurn)),
    };
    fixture.document.shots = {shot(dotted(1, 0), dotted(1, 1), 10.0, 0.0, 0.0, 0)};
    fixture.document.references = {reference(dotted(1, 0), 0, 0, 0)};

    const QString text = translatedText(fixture);
    INFO("translation:\n" << text.toStdString());
    REQUIRE(text.contains(QStringLiteral("*declination 90.00 degrees")));

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const cwCavernRunner::Result result = solve(dir, text);
    INFO("cavern log:\n" << result.logText.toStdString());

    cwSurvex3DFileReader reader;
    const cwStationPositionLookup lookup = reader.readNetworkAndLookup(result.output3dPath).lookup;
    REQUIRE(lookup.hasPosition(QStringLiteral("pt.1.1")));
    const QVector3D end = lookup.position(QStringLiteral("pt.1.1"));
    CHECK(end.x() == Catch::Approx(10.0).margin(kPositionMargin));
    CHECK(end.y() == Catch::Approx(0.0).margin(kPositionMargin));
}
