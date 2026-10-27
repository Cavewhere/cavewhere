/**************************************************************************
**
**    Copyright (C) 2026
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineScanner.h"
#include "cwSaveLoad.h"
#include "cwTrip.h"
#include "cavewhere.pb.h"

// Test helpers
#include "LoadProjectHelper.h"

// Qt
#include <QSignalSpy>
#include <QString>

// Protobuf
#include <google/protobuf/util/json_util.h>

// Std
#include <algorithm>
#include <optional>

namespace {

constexpr const char* kEntryFile = "mycave.svx";
constexpr const char* kAlternateEntryFile = "branches/passage.svx";

// A row's meter text keeps a millionth of a meter, its degree text a billionth
// of a degree.
constexpr double kMeterTextMargin = 1e-6;
constexpr double kDegreeTextMargin = 1e-9;
constexpr double kMinutesPerDegree = 60.0;
constexpr double kSecondsPerDegree = 3600.0;

double degrees(double wholeDegrees, double minutes, double seconds)
{
    return wholeDegrees + minutes / kMinutesPerDegree + seconds / kSecondsPerDegree;
}

//! The row the manager lists for \a scanned, as far as placement reads it.
cwAttachedFix attachedFixFrom(const cwExternalCenterlineScanner::ScannedFix& scanned)
{
    cwAttachedFix fix;
    fix.station = scanned.station;
    fix.coordinate = scanned.coordinate;
    fix.coordinateSystem = scanned.coordinateSystem;
    fix.placementSystem = scanned.placementSystem;
    return fix;
}

//! Where an attached fix should land: the system and the point in its axis order.
struct ExpectedPlacement
{
    QString station;
    QString cs;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double horizontalMargin = kMeterTextMargin;
};

void checkPlacements(const QString& entryFile, const QList<ExpectedPlacement>& expected)
{
    const auto scan = cwExternalCenterlineScanner::scan(entryFile);
    INFO("scan: " << scan.errorMessage().toStdString());
    REQUIRE_FALSE(scan.hasError());
    for (const ExpectedPlacement& placementWanted : expected) {
        INFO("station " << placementWanted.station.toStdString());
        const auto& fixes = scan.value().fixes;
        const auto scanned = std::find_if(fixes.cbegin(), fixes.cend(),
                                          [&](const cwExternalCenterlineScanner::ScannedFix& fix) {
                                              return fix.station == placementWanted.station;
                                          });
        REQUIRE(scanned != fixes.cend());
        const cwAttachedFix fix = attachedFixFrom(*scanned);
        INFO("coordinate " << fix.coordinate.toStdString() << ", solve system "
                           << fix.solveSystem().toStdString());
        const std::optional<cwAttachedFix::Placement> placement = fix.placementIn(fix.solveSystem());
        REQUIRE(placement.has_value());
        CHECK(placement->cs.toStdString() == placementWanted.cs.toStdString());
        CHECK(placement->point.x == Catch::Approx(placementWanted.x).margin(placementWanted.horizontalMargin));
        CHECK(placement->point.y == Catch::Approx(placementWanted.y).margin(placementWanted.horizontalMargin));
        CHECK(placement->point.z == Catch::Approx(placementWanted.z).margin(kMeterTextMargin));
    }
}

} // namespace

TEST_CASE("cwExternalCenterline default constructs empty", "[ExternalCenterline]")
{
    const cwExternalCenterline value;
    CHECK(value.entryFile().isEmpty());
    CHECK(value.isEmpty());
}

TEST_CASE("cwExternalCenterline equality is based on entryFile", "[ExternalCenterline]")
{
    cwExternalCenterline a(QString::fromLatin1(kEntryFile));
    cwExternalCenterline b(QString::fromLatin1(kEntryFile));
    cwExternalCenterline c(QString::fromLatin1(kAlternateEntryFile));

    CHECK(a == b);
    CHECK_FALSE(a != b);
    CHECK(a != c);
    CHECK_FALSE(a == c);

    // setEntryFile mutates equality
    a.setEntryFile(QString::fromLatin1(kAlternateEntryFile));
    CHECK(a == c);
    CHECK(a != b);
}

TEST_CASE("cwExternalCenterline qHash matches equality contract", "[ExternalCenterline]")
{
    const cwExternalCenterline a(QString::fromLatin1(kEntryFile));
    const cwExternalCenterline b(QString::fromLatin1(kEntryFile));
    const cwExternalCenterline c(QString::fromLatin1(kAlternateEntryFile));

    CHECK(qHash(a) == qHash(b));
    // Different inputs can collide but the same input must always hash the same.
    CHECK(qHash(a) == qHash(a));
    // Empty value's hash is the hash of an empty QString.
    CHECK(qHash(cwExternalCenterline()) == qHash(QString()));
    Q_UNUSED(c);
}

TEST_CASE("cwExternalCenterline survives proto JSON round-trip on Cave", "[ExternalCenterline]")
{
    auto cave = std::make_unique<cwCave>();
    cave->setName(QStringLiteral("RoundTripCave"));
    cave->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));

    auto proto = cwSaveLoad::toProtoCave(cave.get());
    REQUIRE(proto != nullptr);
    REQUIRE(proto->has_external_centerline());
    CHECK(proto->external_centerline().entry_file() == kEntryFile);

    std::string json;
    google::protobuf::util::JsonPrintOptions printOptions;
    printOptions.add_whitespace = true;
    const auto printStatus =
        google::protobuf::util::MessageToJsonString(*proto, &json, printOptions);
    REQUIRE(printStatus.ok());
    REQUIRE_FALSE(json.empty());

    CavewhereProto::Cave parsed;
    const auto parseStatus =
        google::protobuf::util::JsonStringToMessage(json, &parsed);
    REQUIRE(parseStatus.ok());
    REQUIRE(parsed.has_external_centerline());
    CHECK(parsed.external_centerline().entry_file() == kEntryFile);
}

TEST_CASE("cwExternalCenterline survives proto JSON round-trip on Trip", "[ExternalCenterline]")
{
    auto trip = std::make_unique<cwTrip>();
    trip->setName(QStringLiteral("RoundTripTrip"));
    trip->setExternalCenterline(
        cwExternalCenterline(QString::fromLatin1(kAlternateEntryFile)));

    auto proto = cwSaveLoad::toProtoTrip(trip.get());
    REQUIRE(proto != nullptr);
    REQUIRE(proto->has_external_centerline());
    CHECK(proto->external_centerline().entry_file() == kAlternateEntryFile);

    std::string json;
    google::protobuf::util::JsonPrintOptions printOptions;
    printOptions.add_whitespace = true;
    const auto printStatus =
        google::protobuf::util::MessageToJsonString(*proto, &json, printOptions);
    REQUIRE(printStatus.ok());

    CavewhereProto::Trip parsed;
    const auto parseStatus =
        google::protobuf::util::JsonStringToMessage(json, &parsed);
    REQUIRE(parseStatus.ok());
    REQUIRE(parsed.has_external_centerline());
    CHECK(parsed.external_centerline().entry_file() == kAlternateEntryFile);

    const cwTripData reloaded = cwSaveLoad::tripDataFromProtoTrip(parsed);
    CHECK(reloaded.externalCenterline.entryFile()
          == QString::fromLatin1(kAlternateEntryFile));
}

TEST_CASE("cwExternalCenterline absent on Cave when empty", "[ExternalCenterline]")
{
    auto cave = std::make_unique<cwCave>();
    cave->setName(QStringLiteral("NativeCave"));
    REQUIRE(cave->externalCenterline().isEmpty());

    auto proto = cwSaveLoad::toProtoCave(cave.get());
    REQUIRE(proto != nullptr);
    CHECK_FALSE(proto->has_external_centerline());
}

TEST_CASE("cwExternalCenterline absent on Trip when empty", "[ExternalCenterline]")
{
    auto trip = std::make_unique<cwTrip>();
    trip->setName(QStringLiteral("NativeTrip"));
    REQUIRE(trip->externalCenterline().isEmpty());

    auto proto = cwSaveLoad::toProtoTrip(trip.get());
    REQUIRE(proto != nullptr);
    CHECK_FALSE(proto->has_external_centerline());
}

TEST_CASE("cwCave setExternalCenterline fires changed signal on set and clear",
          "[ExternalCenterline]")
{
    auto cave = std::make_unique<cwCave>();
    QSignalSpy spy(cave.get(), &cwCave::externalCenterlineChanged);
    REQUIRE(spy.isValid());

    cave->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));
    CHECK(spy.size() == 1);

    // Idempotent set is a no-op.
    cave->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));
    CHECK(spy.size() == 1);

    cave->setExternalCenterline(cwExternalCenterline());
    CHECK(spy.size() == 2);
    CHECK(cave->externalCenterline().isEmpty());
}

TEST_CASE("cwTrip setExternalCenterline fires changed signal on set and clear",
          "[ExternalCenterline]")
{
    auto trip = std::make_unique<cwTrip>();
    QSignalSpy spy(trip.get(), &cwTrip::externalCenterlineChanged);
    REQUIRE(spy.isValid());

    trip->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));
    CHECK(spy.size() == 1);

    trip->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));
    CHECK(spy.size() == 1);

    trip->setExternalCenterline(cwExternalCenterline());
    CHECK(spy.size() == 2);
    CHECK(trip->externalCenterline().isEmpty());
}

TEST_CASE("cwCave::data() round-trips externalCenterline through setData",
          "[ExternalCenterline]")
{
    auto cave = std::make_unique<cwCave>();
    cave->setName(QStringLiteral("DataRoundTrip"));
    cave->setExternalCenterline(cwExternalCenterline(QString::fromLatin1(kEntryFile)));

    const cwCaveData snapshot = cave->data();
    CHECK(snapshot.externalCenterline.entryFile() == QString::fromLatin1(kEntryFile));

    auto destination = std::make_unique<cwCave>();
    destination->setData(snapshot);
    CHECK(destination->externalCenterline().entryFile()
          == QString::fromLatin1(kEntryFile));
}

TEST_CASE("cwTrip::data() round-trips externalCenterline through setData",
          "[ExternalCenterline]")
{
    auto trip = std::make_unique<cwTrip>();
    trip->setName(QStringLiteral("DataRoundTrip"));
    trip->setExternalCenterline(
        cwExternalCenterline(QString::fromLatin1(kAlternateEntryFile)));

    const cwTripData snapshot = trip->data();
    CHECK(snapshot.externalCenterline.entryFile()
          == QString::fromLatin1(kAlternateEntryFile));

    auto destination = std::make_unique<cwTrip>();
    destination->setData(snapshot);
    CHECK(destination->externalCenterline().entryFile()
          == QString::fromLatin1(kAlternateEntryFile));
}

TEST_CASE("A Walls #FIX row places where cavern reads the fix",
          "[ExternalCenterline][Walls][Attach]")
{
    // METERS.SRV writes north first with an F-suffixed elevation, FEET.SRV is
    // in feet, LATLONG.SRV in degrees, minutes and seconds; a lat/long fix
    // places longitude first, as cavern reads it.
    const QString nad27Utm16 = QStringLiteral("EPSG:26716");
    checkPlacements(testcasesDatasetSourcePath(
                        QStringLiteral("external-centerlines/walls_fix_units/walls_fix_units.wpj")),
                    {{QStringLiteral("M1"), nad27Utm16, 520000.0, 4040000.0, 233.534712},
                     {QStringLiteral("F1"), nad27Utm16, 520293.6, 4040428.8, 304.8},
                     {QStringLiteral("L1"), QStringLiteral("EPSG:4267"), -degrees(86.0, 57.0, 30.5),
                      degrees(36.0, 31.0, 15.25), 120.0, kDegreeTextMargin}});
}

TEST_CASE("A Compass .mak row places in its datum and zone's code",
          "[ExternalCenterline][Attach]")
{
    checkPlacements(testcasesDatasetSourcePath(
                        QStringLiteral("external-centerlines/compass_zone_no_datum.mak")),
                    {{QStringLiteral("compass_zone_no_datum.A1"), QStringLiteral("EPSG:26713"),
                      478000.0092, 4429999.98936, 1655.00304}});
}

TEST_CASE("An attached fix solves in its placement system ahead of the name its row shows",
          "[ExternalCenterline]")
{
    cwAttachedFix fix;
    fix.coordinate = QStringLiteral("520000 4040000 233.534712");
    fix.coordinateSystem = QStringLiteral("NAD27 CONUS, UTM zone 16N");
    fix.placementSystem = QStringLiteral("EPSG:26716");
    CHECK(fix.solveSystem().toStdString() == "EPSG:26716");

    // A fix the file names no system for still solves in its read-in system.
    cwAttachedFix bare;
    bare.coordinate = QStringLiteral("0 0 0");
    bare.readInSystem = QStringLiteral("EPSG:32616");
    CHECK(bare.solveSystem().toStdString() == "EPSG:32616");
}
