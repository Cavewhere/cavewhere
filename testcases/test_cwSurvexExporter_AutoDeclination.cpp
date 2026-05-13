/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch2 includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterRule.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurvexExporterUtils.h"
#include "cwSurveyDataArtifact.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QBuffer>
#include <QDateTime>
#include <QSignalSpy>
#include <QTextStream>
#include <QTimeZone>

//Std includes
#include <memory>

namespace {

const QString kUtmZ13N = QStringLiteral("EPSG:32613");

QDateTime makeDate(int year, int month, int day)
{
    return QDateTime(QDate(year, month, day), QTime(0, 0), QTimeZone::UTC);
}

// Build a region with one cave, one trip carrying one shot, and one fix
// station in the named CS. Region returned by unique_ptr so QObject parenting
// cleans up on REQUIRE early-exits.
struct ExporterFixture {
    std::unique_ptr<cwCavingRegion> region;
    cwCave* cave;
    cwTrip* trip;
    cwTripCalibration* calibration;
};

ExporterFixture buildBoulderFixture()
{
    auto region = std::make_unique<cwCavingRegion>();
    region->setGlobalCS(kUtmZ13N);
    auto* cave = new cwCave();
    cave->setName(QStringLiteral("BoulderCave"));
    region->addCave(cave);

    cwFixStation fix;
    fix.setStationName(QStringLiteral("a1"));
    fix.setInputCS(kUtmZ13N);
    fix.setEasting(478000.0);
    fix.setNorthing(4430000.0);
    fix.setElevation(1655.0);
    cave->fixStations()->appendFixStation(fix);

    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setName(QStringLiteral("BoulderTrip"));
    trip->setDate(makeDate(2024, 6, 1));

    auto chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendNewShot();
    chunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("a1"));
    chunk->setData(cwSurveyChunk::StationNameRole, 1, QStringLiteral("a2"));
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, QStringLiteral("10"));
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("0"));
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, QStringLiteral("0"));

    return { std::move(region), cave, trip, trip->calibrations() };
}

QString exportRegion(const cwCavingRegion* region)
{
    cwSurveyDataArtifact::Region snapshot(region);
    QByteArray output;
    QBuffer buffer(&output);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    {
        QTextStream stream(&buffer);
        cwSurvexExporterRule::writeRegion(stream, snapshot);
    }
    return QString::fromUtf8(output);
}

} // namespace

TEST_CASE("cwSurvexExporterRule: autoDeclination on + fix station emits *declination auto and suppresses *calibrate DECLINATION",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderFixture();
    REQUIRE(fixture.calibration->autoDeclination() == true);
    fixture.calibration->setDeclinationManual(99.0); // sentinel: must not appear in output

    const QString output = exportRegion(fixture.region.get());

    CHECK(output.contains(QStringLiteral("*declination auto")));
    CHECK(output.contains(QStringLiteral("478000")));
    CHECK(output.contains(QStringLiteral("4430000")));
    CHECK(output.contains(QStringLiteral("1655")));

    // Should NOT emit literal calibrate DECLINATION when auto is on
    CHECK_FALSE(output.contains(QStringLiteral("*calibrate DECLINATION")));
    CHECK_FALSE(output.contains(QStringLiteral("99.00")));
}

TEST_CASE("cwSurvexExporterRule: autoDeclination off emits literal *calibrate DECLINATION and no auto command",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderFixture();
    fixture.calibration->setAutoDeclination(false);
    fixture.calibration->setDeclinationManual(12.34);

    const QString output = exportRegion(fixture.region.get());

    CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
    CHECK(output.contains(QStringLiteral("*calibrate DECLINATION")));
    // survex literal sign convention is opposite of stored manual: stored
    // 12.34 → emitted -12.34
    CHECK(output.contains(QStringLiteral("-12.34")));
}

TEST_CASE("cwSurvexExporterRule: autoDeclination on but no fix station falls back to literal",
          "[cwSurvexExporter_Auto]")
{
    auto region = std::make_unique<cwCavingRegion>();
    auto* cave = new cwCave();
    cave->setName(QStringLiteral("UnfixedCave"));
    region->addCave(cave);

    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setName(QStringLiteral("UnfixedTrip"));
    trip->setDate(makeDate(2024, 6, 1));
    auto chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendNewShot();
    chunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("a1"));
    chunk->setData(cwSurveyChunk::StationNameRole, 1, QStringLiteral("a2"));
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, QStringLiteral("10"));
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("0"));
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, QStringLiteral("0"));

    cwTripCalibration* calibration = trip->calibrations();
    REQUIRE(calibration->autoDeclination() == true);
    calibration->setDeclinationManual(7.5);

    const QString output = exportRegion(region.get());

    CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
    // Resolved declination falls back to manual when no fix is available,
    // so the literal carries the stored manual value (sign-flipped).
    CHECK(output.contains(QStringLiteral("-7.50")));
}

TEST_CASE("cwSurvexExporterTripTask: writeTrip with a valid DeclinationContext emits *declination auto",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderFixture();
    REQUIRE(fixture.calibration->autoDeclination() == true);

    const cwSurvexExporterUtils::DeclinationContext ctx{
        kUtmZ13N, 478000.0, 4430000.0, 1655.0
    };

    cwSurvexExporterTripTask exporter;
    QByteArray outputData;
    QBuffer buffer(&outputData);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    {
        QTextStream stream(&buffer);
        exporter.writeTrip(stream, fixture.trip, ctx);
    }

    const QString output = QString::fromUtf8(outputData);
    CHECK(output.contains(QStringLiteral("*cs EPSG:32613")));
    CHECK(output.contains(QStringLiteral("*declination auto")));
    CHECK_FALSE(output.contains(QStringLiteral("*calibrate DECLINATION")));
}

TEST_CASE("cwSurvexExporterTripTask: writeTrip with no DeclinationContext falls back to literal",
          "[cwSurvexExporter_Auto]")
{
    // Standalone trip export path: free-floating trip with no parent cave.
    // The default-constructed context (valid==false) keeps existing literal
    // behavior. Auto can't resolve here (no parent cave), so the literal
    // carries the stored manual value.
    cwTrip trip;
    trip.setName(QStringLiteral("FreeTrip"));
    trip.setDate(makeDate(2024, 6, 1));
    auto chunk = new cwSurveyChunk();
    trip.addChunk(chunk);
    chunk->appendNewShot();
    chunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("a1"));
    chunk->setData(cwSurveyChunk::StationNameRole, 1, QStringLiteral("a2"));
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, QStringLiteral("10"));
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("0"));
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, QStringLiteral("0"));

    trip.calibrations()->setDeclinationManual(4.0);
    REQUIRE(trip.calibrations()->autoDeclination() == true);
    REQUIRE(trip.calibrations()->autoDeclinationAvailable() == false);

    cwSurvexExporterTripTask exporter;
    QByteArray outputData;
    QBuffer buffer(&outputData);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    {
        QTextStream stream(&buffer);
        exporter.writeTrip(stream, &trip);
    }

    const QString output = QString::fromUtf8(outputData);
    CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
    CHECK(output.contains(QStringLiteral("*calibrate DECLINATION")));
    CHECK(output.contains(QStringLiteral("-4.00")));
}

TEST_CASE("cwSurvexExporterUtils::makeDeclinationContext picks first fix and prefers fix's inputCS",
          "[cwSurvexExporter_Auto]")
{
    cwFixStation a;
    a.setStationName(QStringLiteral("a1"));
    a.setInputCS(kUtmZ13N);
    a.setEasting(478000.0);
    a.setNorthing(4430000.0);
    a.setElevation(1655.0);

    cwFixStation b;
    b.setStationName(QStringLiteral("b1"));
    b.setInputCS(QStringLiteral("EPSG:32614"));
    b.setEasting(123.0);
    b.setNorthing(456.0);
    b.setElevation(789.0);

    const auto ctx = cwSurvexExporterUtils::makeDeclinationContext({ a, b }, QStringLiteral("EPSG:4326"));
    REQUIRE(ctx.has_value());
    CHECK(ctx->inputCS == kUtmZ13N);
    CHECK(ctx->easting == 478000.0);
    CHECK(ctx->northing == 4430000.0);
    CHECK(ctx->elevation == 1655.0);
}

TEST_CASE("cwSurvexExporterUtils::makeDeclinationContext falls back to globalCS when fix has no inputCS",
          "[cwSurvexExporter_Auto]")
{
    cwFixStation a;
    a.setStationName(QStringLiteral("a1"));
    // No inputCS on the fix.
    a.setEasting(1.0);
    a.setNorthing(2.0);
    a.setElevation(3.0);

    const auto ctx = cwSurvexExporterUtils::makeDeclinationContext({ a }, kUtmZ13N);
    REQUIRE(ctx.has_value());
    CHECK(ctx->inputCS == kUtmZ13N);
}

TEST_CASE("cwSurvexExporterUtils::makeDeclinationContext is invalid with no fix or no CS",
          "[cwSurvexExporter_Auto]")
{
    SECTION("Empty fix list") {
        const auto ctx = cwSurvexExporterUtils::makeDeclinationContext({}, kUtmZ13N);
        CHECK_FALSE(ctx.has_value());
    }

    SECTION("Fix without inputCS and no globalCS") {
        cwFixStation a;
        a.setStationName(QStringLiteral("a1"));
        const auto ctx = cwSurvexExporterUtils::makeDeclinationContext({ a }, QString());
        CHECK_FALSE(ctx.has_value());
    }
}
