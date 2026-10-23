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
#include "cwExternalCenterlineScanner.h"
#include "cwPocketTopoFile.h"
#include "cwSurveyTreeModel.h"

// Test helpers
#include "PocketTopoFixtureWriter.h"

// Qt
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaEnum>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

using cwExternalCenterlineScanner::Format;
using cwExternalCenterlineScanner::ScanResult;
using cwExternalCenterlineScanner::ScannedFix;
using cwPocketTopoFile::Reference;
using cwPocketTopoFile::Shot;
using cwPocketTopoFile::StationId;
using cwPocketTopoFile::Trip;
using cwSurveyNodeKind::Kind;
using PocketTopoFixtureWriter::Fixture;

namespace {

constexpr int kMillimetersPerMeter = 1000;
constexpr double kFullCircleDegrees = 360.0;
constexpr double kAngleUnitsPerCircle = 65536.0;
constexpr qint16 kNoTrip = Shot::kNoTrip;
constexpr qint16 kOutOfRangeTrip = 5;
// 1234 / 65536 of a circle, 6.78 degrees.
constexpr qint16 kEastDeclinationUnits = 1234;
constexpr double kEastDeclinationDegrees = kEastDeclinationUnits * kFullCircleDegrees / kAngleUnitsPerCircle;

// A reference on 1.0, in millimeters.
constexpr qint64 kFixEastMm = 500123456;
constexpr qint64 kFixNorthMm = 4500321000;
constexpr qint32 kFixAltitudeMm = 812345;

const QString kFileName = QStringLiteral("cave.top");

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

//! The leading row PocketTopo writes before any trip: the start station, a
//! zero-length splay, no trip.
Shot leadingStartRow()
{
    return splay(dotted(1, 0), 0.0, 0.0, 0.0, kNoTrip);
}

/**
 * Two trips. The first, manual declination, holds a triple on 1.0-1.1 (one
 * reading taken backwards), a splay from 1.0, and 1.1-1.2. The second, Auto
 * with no comment, joins 1.2 to series 2 with a zero-length leg and runs
 * 2.0-2.1 with a splay from 2.1.
 */
Fixture twoTripFixture()
{
    Fixture fixture;
    fixture.document.trips = {
        trip(QDate(2026, 2, 5), QStringLiteral("Robin survey\rBecka\r\rSam Smith"), kEastDeclinationUnits),
        trip(QDate(2026, 3, 14), QString(), Trip::kAutoDeclination),
    };
    fixture.document.shots = {
        leadingStartRow(),
        shot(dotted(1, 0), dotted(1, 1), 3.38, 271.35, -1.51, 0),
        shot(dotted(1, 0), dotted(1, 1), 3.38, 271.35, -1.69, 0),
        shot(dotted(1, 1), dotted(1, 0), 3.38, 91.36, 1.77, 0),
        splay(dotted(1, 0), 1.54, 333.43, 5.91, 0),
        shot(dotted(1, 1), dotted(1, 2), 3.06, 301.39, -4.19, 0),
        shot(dotted(1, 2), dotted(2, 0), 0.0, 0.0, 0.0, 1),
        shot(dotted(2, 0), dotted(2, 1), 4.0, 10.0, 0.0, 1),
        splay(dotted(2, 1), 1.0, 90.0, 0.0, 1),
    };
    return fixture;
}

QString writeFixture(const QTemporaryDir& dir, const Fixture& fixture, const QString& fileName = kFileName)
{
    const QString path = QDir(dir.path()).absoluteFilePath(fileName);
    REQUIRE(PocketTopoFixtureWriter::writeFile(fixture, path));
    return path;
}

ScanResult scanned(const QString& path)
{
    auto result = cwExternalCenterlineScanner::scan(path);
    INFO("scan: " << result.errorMessage().toStdString());
    REQUIRE_FALSE(result.hasError());
    return result.value();
}

ScanResult scannedFixture(const QTemporaryDir& dir, const Fixture& fixture)
{
    return scanned(writeFixture(dir, fixture));
}

//! One scan row in a form a list comparison prints readably: depth, kind,
//! sourcePath, display name, cavernLabel, role, subtree station count, and the
//! gray flag.
QString describe(const cwScanRow& row)
{
    const QString kindName = QString::fromLatin1(
        QMetaEnum::fromType<Kind>().valueToKey(static_cast<int>(row.kind)));
    QStringList roles;
    if (row.isNode) {
        roles.append(QStringLiteral("node"));
    }
    if (row.isUnit) {
        roles.append(QStringLiteral("unit"));
    }
    QString text = QStringLiteral("%1 %2 '%3' '%4' '%5' %6 %7")
                       .arg(row.depth)
                       .arg(kindName, row.sourcePath, row.displayName, row.cavernLabel,
                            roles.join(QLatin1Char('+')))
                       .arg(row.stationCount);
    if (!row.hasSurveyContent) {
        text += QStringLiteral(" gray");
    }
    if (row.missing) {
        text += QStringLiteral(" missing");
    }
    return text;
}

QStringList describe(const QList<cwScanRow>& rows)
{
    QStringList descriptions;
    for (const cwScanRow& row : rows) {
        descriptions.append(describe(row));
    }
    return descriptions;
}

cwScanRow rowAt(const ScanResult& scan, const QString& sourcePath)
{
    for (const cwScanRow& row : scan.rows) {
        if (row.sourcePath == sourcePath) {
            return row;
        }
    }
    FAIL("no scan row has sourcePath " << sourcePath.toStdString());
    return cwScanRow();
}

QStringList warningsContaining(const ScanResult& scan, const QString& needle)
{
    QStringList matches;
    for (const QString& warning : scan.warnings) {
        if (warning.contains(needle)) {
            matches.append(warning);
        }
    }
    return matches;
}

} // namespace

TEST_CASE("formatFor recognizes a PocketTopo .top in any case", "[Scanner][PocketTopo]")
{
    using namespace cwExternalCenterlineScanner;
    CHECK(formatFor(QStringLiteral("cave.top")) == Format::PocketTopo);
    CHECK(formatFor(QStringLiteral("CAVE.TOP")) == Format::PocketTopo);
    CHECK(formatFor(QStringLiteral("cave.top.svx")) == Format::Survex);
    CHECK(formatName(Format::PocketTopo) == QStringLiteral("PocketTopo"));
}

TEST_CASE("A PocketTopo node is labeled a PocketTopo file", "[Scanner][PocketTopo]")
{
    CHECK(cwSurveyTreeModel::kindLabel(Kind::PocketTopoFile) == QStringLiteral("PocketTopo file"));
}

TEST_CASE("scan reads a .top as a closure of the one file", "[Scanner][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = writeFixture(dir, twoTripFixture());

    const ScanResult scan = scanned(path);
    CHECK(scan.dependencies == QStringList({QFileInfo(path).canonicalFilePath()}));
    CHECK(scan.entryDirectIncludes.isEmpty());
    CHECK(scan.blocks.isEmpty());
    CHECK(scan.entryHasOwnShots);
    CHECK(scan.rootDate == QDate(2026, 2, 5));
    CHECK(scan.outputCoordinateSystem.isEmpty());
    CHECK(scan.defaultedDatums.isEmpty());
}

TEST_CASE("A PocketTopo file is a root holding one unit per trip, named by date and comment",
          "[Scanner][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const ScanResult scan = scannedFixture(dir, twoTripFixture());

    // The trips are positional; the second has no comment, so its date alone
    // names it. Unit rows carry their root's kind.
    CHECK(describe(scan.rows) == QStringList({
        QStringLiteral("0 PocketTopoFile '' 'cave' '' node 5"),
        QStringLiteral("1 PocketTopoFile '#1' '2026-02-05 Robin survey' '' unit 3"),
        QStringLiteral("1 PocketTopoFile '#2' '2026-03-14' '' unit 3"),
    }));

    // Every station a trip's legs, equates and splays name; a splay's far end
    // is no station. 1.2 belongs to both trips.
    const cwScanRow first = rowAt(scan, QStringLiteral("#1"));
    const cwScanRow second = rowAt(scan, QStringLiteral("#2"));
    CHECK(first.stations == QStringList({QStringLiteral("1.0"), QStringLiteral("1.1"),
                                         QStringLiteral("1.2")}));
    CHECK(second.stations == QStringList({QStringLiteral("1.2"), QStringLiteral("2.0"),
                                          QStringLiteral("2.1")}));

    CHECK(first.date == QDate(2026, 2, 5));
    CHECK(second.date == QDate(2026, 3, 14));

    // PocketTopo stations are file-global: the root's namespace holds them all.
    CHECK(scan.rootStationCount == 5);
}

TEST_CASE("Each PocketTopo unit carries its trip's manual or Auto declination",
          "[Scanner][PocketTopo][Declination]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const ScanResult scan = scannedFixture(dir, twoTripFixture());

    const cwScanRow first = rowAt(scan, QStringLiteral("#1"));
    REQUIRE(first.isUnit);
    CHECK(first.declination.kind == cwFileDeclination::Kind::Value);
    CHECK(first.declination.degrees == Catch::Approx(kEastDeclinationDegrees));

    // The file names no location for Auto, so the node's fix station supplies one.
    const cwScanRow second = rowAt(scan, QStringLiteral("#2"));
    REQUIRE(second.isUnit);
    CHECK(second.declination.kind == cwFileDeclination::Kind::AutoAtNode);
}

TEST_CASE("A PocketTopo file seeds trip metadata from its first trip", "[Scanner][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    SECTION("a manual declination, and the comment's later lines as the team")
    {
        const ScanResult scan = scannedFixture(dir, twoTripFixture());
        CHECK(scan.seededMetadata.date == std::optional<QDate>(QDate(2026, 2, 5)));
        REQUIRE(scan.seededMetadata.declination.has_value());
        CHECK(*scan.seededMetadata.declination == Catch::Approx(kEastDeclinationDegrees));
        CHECK(scan.seededMetadata.team
              == QStringList({QStringLiteral("Becka"), QStringLiteral("Sam Smith")}));
    }

    SECTION("an Auto first trip seeds no value")
    {
        Fixture fixture = twoTripFixture();
        fixture.document.trips[0].declination = Trip::kAutoDeclination;
        const ScanResult scan = scannedFixture(dir, fixture);
        CHECK(scan.seededMetadata.date == std::optional<QDate>(QDate(2026, 2, 5)));
        CHECK_FALSE(scan.seededMetadata.declination.has_value());
    }
}

TEST_CASE("A PocketTopo scan drops shots with no trip, warning about all but the leading row",
          "[Scanner][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    SECTION("the leading zero-length start row goes silently")
    {
        const ScanResult scan = scannedFixture(dir, twoTripFixture());
        CHECK(scan.warnings.isEmpty());
        CHECK(rowAt(scan, QStringLiteral("#1")).stationCount == 3);
    }

    SECTION("any other shot outside the trips is counted once")
    {
        Fixture fixture = twoTripFixture();
        fixture.document.shots.append(shot(dotted(9, 0), dotted(9, 1), 2.0, 0.0, 0.0, kNoTrip));
        fixture.document.shots.append(shot(dotted(9, 1), dotted(9, 2), 2.0, 0.0, 0.0, kOutOfRangeTrip));
        const ScanResult scan = scannedFixture(dir, fixture);

        // Their stations belong to no unit and are no part of the file's count.
        CHECK(describe(scan.rows) == QStringList({
            QStringLiteral("0 PocketTopoFile '' 'cave' '' node 5"),
            QStringLiteral("1 PocketTopoFile '#1' '2026-02-05 Robin survey' '' unit 3"),
            QStringLiteral("1 PocketTopoFile '#2' '2026-03-14' '' unit 3"),
        }));
        CHECK(scan.rootStationCount == 5);

        INFO("warnings: " << scan.warnings.join(QStringLiteral(" | ")).toStdString());
        REQUIRE(scan.warnings.size() == 1);
        CHECK(scan.warnings.first().contains(kFileName));
        CHECK(scan.warnings.first().contains(QStringLiteral("2 shots with no trip")));
    }
}

TEST_CASE("A PocketTopo scan warns once per file with counts of the fields an attach drops",
          "[Scanner][PocketTopo]")
{
    using PocketTopoFixtureWriter::Point;
    using PocketTopoFixtureWriter::Polygon;
    using PocketTopoFixtureWriter::XSection;

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    Fixture fixture = twoTripFixture();
    // Shot 1 and 4 carry comments; shot 5 is flipped.
    fixture.document.shots[1].flags |= Shot::HasComment;
    fixture.document.shots[1].comment = QStringLiteral("first reading");
    fixture.document.shots[4].flags |= Shot::HasComment;
    fixture.document.shots[4].comment = QStringLiteral("wall");
    fixture.document.shots[5].flags |= Shot::Flipped;

    const Polygon polygon{{Point{0, 0}, Point{1000, 1000}}, 1};
    fixture.plan.elements = {polygon, polygon, polygon};
    fixture.side.elements = {XSection{Point{0, 0}, dotted(1, 0), -1}};

    const ScanResult scan = scannedFixture(dir, fixture);
    INFO("warnings: " << scan.warnings.join(QStringLiteral(" | ")).toStdString());
    REQUIRE(scan.warnings.size() == 1);
    const QString& warning = scan.warnings.first();
    CHECK(warning.contains(kFileName));
    CHECK(warning.contains(QStringLiteral("2 shot comments")));
    CHECK(warning.contains(QStringLiteral("1 flipped shot")));
    CHECK(warning.contains(QStringLiteral("3 plan drawing elements")));
    CHECK(warning.contains(QStringLiteral("1 side drawing element")));
}

TEST_CASE("A PocketTopo unit spells a renamed plain station as the translation does, and warns once",
          "[Scanner][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    Fixture fixture;
    fixture.document.trips = {trip(QDate(2026, 2, 5), QString())};
    fixture.document.shots = {
        shot(StationId::plain(0), dotted(0, 0), 2.0, 90.0, 0.0, 0),
        shot(dotted(0, 0), StationId::plain(7), 2.0, 90.0, 0.0, 0),
        shot(StationId::plain(7), dotted(1, 5), 2.0, 90.0, 0.0, 0),
    };
    fixture.document.references = {reference(StationId::plain(0), 0, 0, 0)};

    const ScanResult scan = scannedFixture(dir, fixture);
    CHECK(rowAt(scan, QStringLiteral("#1")).stations
          == QStringList({QStringLiteral("0.0"), QStringLiteral("0_"), QStringLiteral("1.5"),
                          QStringLiteral("7")}));

    INFO("warnings: " << scan.warnings.join(QStringLiteral(" | ")).toStdString());
    CHECK(scan.warnings.size() == 1);
    CHECK(warningsContaining(scan, QStringLiteral("0_")).size() == 1);

    REQUIRE(scan.fixes.size() == 1);
    CHECK(scan.fixes.first().station == QStringLiteral("0_"));
}

TEST_CASE("PocketTopo references are fixes in meters with no coordinate system",
          "[Scanner][PocketTopo][Attach]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    Fixture fixture = twoTripFixture();
    fixture.document.references = {
        reference(dotted(1, 0), kFixEastMm, kFixNorthMm, kFixAltitudeMm),
        // A blank reference fixes nothing.
        reference(dotted(1, 1), Reference::kBlankCoordinate, Reference::kBlankCoordinate,
                  Reference::kBlankAltitude),
    };
    const QString path = writeFixture(dir, fixture);
    const ScanResult scan = scanned(path);

    const ScannedFix expected{.station = QStringLiteral("1.0"),
                              .coordinate = QStringLiteral("500123.456 4500321 812.345"),
                              .coordinateSystem = QString(),
                              .placementSystem = QString(),
                              .file = QFileInfo(path).canonicalFilePath(),
                              .line = 0,
                              .reference = false,
                              .nodePath = QString(),
                              .tripPath = QString()};
    CHECK(scan.fixes == QList<ScannedFix>({expected}));
    CHECK(scan.hasFixWithoutCoordinateSystem());
}

TEST_CASE("scanPocketTopo fails on a file it cannot read", "[Scanner][PocketTopo]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    SECTION("a file that is no .top")
    {
        const QString path = QDir(dir.path()).absoluteFilePath(kFileName);
        QFile file(path);
        REQUIRE(file.open(QFile::WriteOnly));
        file.write(QByteArrayLiteral("not a PocketTopo file"));
        file.close();

        const auto result = cwExternalCenterlineScanner::scanPocketTopo(path);
        CHECK(result.hasError());
    }

    SECTION("a file that does not exist")
    {
        const auto result = cwExternalCenterlineScanner::scanPocketTopo(
            QDir(dir.path()).absoluteFilePath(QStringLiteral("absent.top")));
        CHECK(result.hasError());
    }
}
