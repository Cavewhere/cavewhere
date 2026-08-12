/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch2 includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "BoulderFixtureHelper.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwExporterTask.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterRule.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurvexExporterUtils.h"
#include "cwSurveyDataArtifact.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwGridConvergence.h"
#include "cwGeoPoint.h"

//Qt includes
#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

//Std includes
#include <memory>

namespace {

const QString kUtmZ13N = QStringLiteral("EPSG:32613");
const QString kUtmZ14N = QStringLiteral("EPSG:32614");

// Adds a trip carrying one a1 -> a2 shot, dated in `year`.
cwTrip* addTripWithShot(cwCave* cave, const QString& name, int year)
{
    cave->addTrip();
    cwTrip* trip = cave->trip(cave->tripCount() - 1);
    trip->setName(name);
    trip->setDate(makeUtcDate(year, 6, 1));

    auto chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendNewShot();
    chunk->setData(cwSurveyChunk::StationNameRole, 0, QStringLiteral("a1"));
    chunk->setData(cwSurveyChunk::StationNameRole, 1, QStringLiteral("a2"));
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, QStringLiteral("10"));
    chunk->setData(cwSurveyChunk::ShotCompassRole, 0, QStringLiteral("0"));
    chunk->setData(cwSurveyChunk::ShotClinoRole, 0, QStringLiteral("0"));
    return trip;
}

cwFixStation makeFix(const QString& stationName, const QString& cs,
                     double easting, double northing, double elevation)
{
    cwFixStation fix;
    fix.setStationName(stationName);
    fix.setInputCS(cs);
    fix.setCoordinate(easting, northing, elevation);
    return fix;
}

// A region with one named cave and one trip carrying one shot. No fix station,
// so the cave stays un-georeferenced.
BoulderFixture buildUnfixedFixture(const QString& caveName, const QString& tripName)
{
    auto region = std::make_unique<cwCavingRegion>();
    auto* cave = new cwCave();
    cave->setName(caveName);
    region->addCave(cave);

    cwTrip* trip = addTripWithShot(cave, tripName, 2024);

    return { std::move(region), cave, trip, trip->calibrations() };
}

// The same, plus one fix station placing the cave in UTM zone 13N.
BoulderFixture buildBoulderUtmFixture()
{
    BoulderFixture fixture = buildUnfixedFixture(QStringLiteral("BoulderCave"),
                                                 QStringLiteral("BoulderTrip"));
    fixture.cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("a1"), kUtmZ13N, 478000.0, 4430000.0, 1655.0));
    return fixture;
}

// The convergence the export is expected to subtract from a literal
// declination: read in the system *cs out names, at the first fix's location
// transformed into it. Derived here rather than hard-coded so the assertions
// stay tied to what PROJ says rather than to a snapshot of it.
double expectedConvergence(const cwCavingRegion& region)
{
    const cwSurveyDataArtifact::Region snapshot(&region);
    const QString outputCS = cwSurvexExporterUtils::resolveOutputCS(
        snapshot, QString(), cwSurvexExporterUtils::OutputCSPolicy::Shareable);
    return cwSurvexExporterUtils::gridConvergenceForBlock(
        cwSurvexExporterUtils::makeDeclinationContext(snapshot.caves.first().fixStations),
        outputCS);
}

QString exportRegion(const cwCavingRegion* region)
{
    cwSurveyDataArtifact::Region snapshot(region);
    QByteArray output;
    QBuffer buffer(&output);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    {
        QTextStream stream(&buffer);
        cwSurvexCS::SidecarWriter sidecars;
        cwSurvexExporterRule::writeRegion(stream, sidecars, snapshot);
    }
    return QString::fromUtf8(output);
}

// The other export stack: cwSurvexExporterCaveTask, which the line-plot driver
// and the region exporter both run. Its output has to match the rule path's.
QString exportCaveViaTask(const cwCave* cave, const QString& globalCS)
{
    cwSurvexExporterCaveTask task;
    QByteArray output;
    QBuffer buffer(&output);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    {
        QTextStream stream(&buffer);
        REQUIRE(task.writeCave(stream, cave->data(), globalCS));
    }
    return QString::fromUtf8(output);
}

// Drives an exporter task the way cwSurveyExportManager does — start, wait,
// read the file back — so what's asserted is the file a user gets.
QString runExportTask(cwExporterTask* task, const QTemporaryDir& dir, const QString& name)
{
    const QString path = dir.filePath(name);
    task->setOutputFile(path);
    task->start();
    task->waitToFinish();

    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return QString::fromUtf8(file.readAll());
}

} // namespace

TEST_CASE("cwSurvexExporterRule: the cave block carries one *declination auto for every trip under it",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();
    REQUIRE(fixture.calibration->autoDeclination() == true);
    fixture.calibration->setDeclinationManual(99.0); // sentinel: must not appear in output

    cwTrip* second = addTripWithShot(fixture.cave, QStringLiteral("SecondTrip"), 2025);
    REQUIRE(second->calibrations()->autoDeclination() == true);

    const QString output = exportRegion(fixture.region.get());
    INFO(output.toStdString());

    // Cavern inherits the location into each trip's *begin block and
    // re-evaluates IGRF from that block's own *date, so one line covers both
    // trips no matter how many there are.
    CHECK(output.count(QStringLiteral("*declination auto")) == 1);
    CHECK(output.contains(QStringLiteral("*declination auto 478000.000000000 4430000.000000000 1655.000000000")));

    // ...and it sits in the cave block, ahead of every trip.
    const int declinationAt = output.indexOf(QStringLiteral("*declination auto"));
    const int firstTripAt = output.indexOf(QStringLiteral("*begin ;"));
    REQUIRE(firstTripAt >= 0);
    CHECK(declinationAt < firstTripAt);

    // The *cs the fixes named is still in scope, so the declination doesn't
    // re-declare it.
    CHECK(output.count(QStringLiteral("*cs EPSG:32613")) == 1);

    CHECK_FALSE(output.contains(QStringLiteral("*calibrate DECLINATION")));
    CHECK_FALSE(output.contains(QStringLiteral("99.00")));
}

TEST_CASE("cwSurvexExporterRule: *declination auto re-declares its *cs when a later fix changed the one in scope",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();

    // A second fix under a different system. makeDeclinationContext still picks
    // the first fix, so the system its coordinate is in is no longer the one
    // standing after the *fix block.
    fixture.cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("a2"), kUtmZ14N, 233000.0, 4431000.0, 1650.0));

    const QString output = exportRegion(fixture.region.get());
    INFO(output.toStdString());

    CHECK(output.count(QStringLiteral("*cs EPSG:32614")) == 1);
    CHECK(output.count(QStringLiteral("*cs EPSG:32613")) == 2);

    const int secondSystemAt = output.indexOf(QStringLiteral("*cs EPSG:32614"));
    const int declinationAt = output.indexOf(QStringLiteral("*declination auto"));
    const int reDeclaredAt = output.lastIndexOf(QStringLiteral("*cs EPSG:32613"));
    REQUIRE(declinationAt >= 0);
    CHECK(secondSystemAt < reDeclaredAt);
    CHECK(reDeclaredAt < declinationAt);

    // The coordinate is the first fix's, read under the system just re-declared.
    CHECK(output.contains(QStringLiteral("*declination auto 478000.000000000 4430000.000000000 1655.000000000")));
}

TEST_CASE("cwSurvexExporterRule: autoDeclination off emits literal *calibrate DECLINATION and no auto command",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();
    fixture.calibration->setAutoDeclination(false);
    fixture.calibration->setDeclinationManual(12.34);

    const QString output = exportRegion(fixture.region.get());

    // Nothing under this cave asked for auto, so no location is written at all.
    CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
    CHECK(output.contains(QStringLiteral("*calibrate DECLINATION")));

    // Manual declination is a pure magnetic declination, so the exporter
    // subtracts the fix station's grid convergence before writing the literal
    // (issue #628) — cavern won't do it for *calibrate DECLINATION. writeCalibration
    // then flips the sign, so the emitted value is -(12.34 - convergence).
    const auto convergence = cwGridConvergence::computeAt(
        cwGeoPoint(478000.0, 4430000.0, 1655.0), kUtmZ13N);
    REQUIRE_FALSE(convergence.hasError());
    REQUIRE(convergence.value() != 0.0);
    const double emitted = -(12.34 - convergence.value());
    CHECK(output.contains(QString::number(emitted, 'f', 2)));
    // The plain magnetic value must NOT appear as the calibrate value.
    CHECK_FALSE(output.contains(QStringLiteral("DECLINATION -12.34")));
    // A comment explains the adjustment so the export isn't confusing.
    CHECK(output.contains(QStringLiteral("grid convergence")));
}

TEST_CASE("cwSurvexExporterRule: a manual zero is spelled out when it has to override an inherited *declination auto",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();

    // First trip keeps auto on, so the cave block carries *declination auto.
    // The second asks for a declination of exactly zero — silence there would
    // inherit the auto value instead of the zero that was asked for.
    cwTrip* second = addTripWithShot(fixture.cave, QStringLiteral("ZeroTrip"), 2025);
    second->calibrations()->setAutoDeclination(false);
    second->calibrations()->setDeclinationManual(0.0);

    const QString output = exportRegion(fixture.region.get());
    INFO(output.toStdString());

    // The cave is georeferenced, so the zero is written as the grid convergence
    // that lands a magnetic-zero survey on grid north — still an explicit
    // override of the inherited auto, just not the digits 0.00.
    const double convergence = expectedConvergence(*fixture.region);
    REQUIRE(convergence != 0.0);
    const QString expected = QStringLiteral("*calibrate DECLINATION %1")
                                 .arg(-(0.0 - convergence), 0, 'f', 2);

    REQUIRE(output.count(QStringLiteral("*declination auto")) == 1);
    CHECK(output.contains(expected));
    CHECK(output.indexOf(expected) > output.indexOf(QStringLiteral("*declination auto")));
}

TEST_CASE("cwSurvexExporterRule: a manual zero stays implicit when there is no grid and nothing to override",
          "[cwSurvexExporter_Auto]")
{
    // Un-georeferenced, so there is no *cs out and no grid to converge to. Zero
    // is survex's default, and writing it would be noise in every trip of every
    // project that never touches declination.
    auto fixture = buildUnfixedFixture(QStringLiteral("LocalCave"),
                                       QStringLiteral("LocalTrip"));
    fixture.calibration->setAutoDeclination(false);
    fixture.calibration->setDeclinationManual(0.0);

    const QString output = exportRegion(fixture.region.get());
    INFO(output.toStdString());

    CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
    CHECK_FALSE(output.contains(QStringLiteral("*calibrate DECLINATION")));
}

TEST_CASE("cwSurvexExporterRule: a manual zero on a grid is written as the convergence",
          "[cwSurvexExporter_Auto]")
{
    // A manual zero says "my compass reads true north", not "leave my bearings
    // off the grid" — the plot still has to land in the system *cs out names, so
    // the convergence is written even though the declination itself is nothing.
    auto fixture = buildBoulderUtmFixture();
    fixture.calibration->setAutoDeclination(false);
    fixture.calibration->setDeclinationManual(0.0);

    const QString output = exportRegion(fixture.region.get());
    INFO(output.toStdString());

    const double convergence = expectedConvergence(*fixture.region);
    REQUIRE(convergence != 0.0);

    CHECK(output.contains(QStringLiteral("*calibrate DECLINATION %1")
                              .arg(-(0.0 - convergence), 0, 'f', 2)));
    CHECK(output.contains(QStringLiteral("grid convergence")));
}

TEST_CASE("cwSurvexExporterRule: each cave gets its own *declination auto",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();

    auto* second = new cwCave();
    second->setName(QStringLiteral("OtherCave"));
    fixture.region->addCave(second);
    second->fixStations()->appendFixStation(
        makeFix(QStringLiteral("a1"), kUtmZ13N, 500000.0, 4200000.0, 900.0));
    addTripWithShot(second, QStringLiteral("OtherTrip"), 2024);

    const QString output = exportRegion(fixture.region.get());
    INFO(output.toStdString());

    // Two caves are two locations: declination is a property of where you are.
    CHECK(output.count(QStringLiteral("*declination auto")) == 2);
    CHECK(output.contains(QStringLiteral("*declination auto 478000.000000000 4430000.000000000 1655.000000000")));
    CHECK(output.contains(QStringLiteral("*declination auto 500000.000000000 4200000.000000000 900.000000000")));
}

TEST_CASE("cwSurvexExporterCaveTask: the cave block carries one *declination auto for every trip under it",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();
    addTripWithShot(fixture.cave, QStringLiteral("SecondTrip"), 2025);

    const QString output = exportCaveViaTask(fixture.cave, kUtmZ13N);
    INFO(output.toStdString());

    CHECK(output.count(QStringLiteral("*declination auto")) == 1);
    CHECK(output.count(QStringLiteral("*cs EPSG:32613")) == 1);
    CHECK(output.indexOf(QStringLiteral("*declination auto"))
          < output.indexOf(QStringLiteral("*begin ;")));
    CHECK_FALSE(output.contains(QStringLiteral("*calibrate DECLINATION")));
}

TEST_CASE("cwSurvexExporterRule: autoDeclination on but no fix station falls back to literal",
          "[cwSurvexExporter_Auto]")
{
    BoulderFixture fixture = buildUnfixedFixture(QStringLiteral("UnfixedCave"),
                                                 QStringLiteral("UnfixedTrip"));
    REQUIRE(fixture.calibration->autoDeclination() == true);
    fixture.calibration->setDeclinationManual(7.5);

    const QString output = exportRegion(fixture.region.get());

    CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
    // Resolved declination falls back to manual when no fix is available,
    // so the literal carries the stored manual value (sign-flipped).
    CHECK(output.contains(QStringLiteral("-7.50")));
}

TEST_CASE("cwSurvexExporterTripTask: writeTrip under an enclosing *declination auto writes no declination line",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();
    REQUIRE(fixture.calibration->autoDeclination() == true);
    fixture.calibration->setDeclinationManual(99.0); // sentinel: must not appear in output

    cwSurvexExporterTripTask exporter;
    QByteArray outputData;
    QBuffer buffer(&outputData);
    REQUIRE(buffer.open(QIODevice::WriteOnly));
    {
        QTextStream stream(&buffer);
        exporter.writeTrip(stream, fixture.trip, /*autoDeclinationInScope*/ true);
    }

    const QString output = QString::fromUtf8(outputData);
    INFO(output.toStdString());

    // The location belongs to the cave block; the trip inherits it.
    CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
    CHECK_FALSE(output.contains(QStringLiteral("*cs ")));
    CHECK_FALSE(output.contains(QStringLiteral("*calibrate DECLINATION")));
}

TEST_CASE("cwSurvexExporterTripTask: writeTrip with nothing in scope falls back to the literal",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();

    const auto exportTrip = [&fixture]() {
        cwSurvexExporterTripTask exporter;
        QByteArray outputData;
        QBuffer buffer(&outputData);
        REQUIRE(buffer.open(QIODevice::WriteOnly));
        {
            QTextStream stream(&buffer);
            exporter.writeTrip(stream, fixture.trip, /*autoDeclinationInScope*/ false);
        }
        return QString::fromUtf8(outputData);
    };

    SECTION("auto off writes the manual value") {
        fixture.calibration->setAutoDeclination(false);
        fixture.calibration->setDeclinationManual(4.0);

        const QString output = exportTrip();
        CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
        // survex's literal sign convention is opposite of the stored manual.
        CHECK(output.contains(QStringLiteral("*calibrate DECLINATION -4.00")));
    }

    SECTION("auto on still writes a literal, because nothing above it carries the location") {
        REQUIRE(fixture.calibration->autoDeclination() == true);
        REQUIRE(fixture.calibration->autoDeclinationAvailable() == true);

        const QString output = exportTrip();
        CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
        // The resolved IGRF value, written out rather than left to survex —
        // survex can't compute it without a location it was never given.
        CHECK(output.contains(QStringLiteral("*calibrate DECLINATION")));
    }
}

TEST_CASE("cwSurvexExporterTripTask: a trip exported on its own is a file cavern can read",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();
    REQUIRE(fixture.calibration->autoDeclination() == true);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    cwSurvexExporterTripTask exporter;
    exporter.setData(fixture.trip->data());
    exporter.setCaveFixStations(fixture.cave->fixStations()->fixStations());
    const QString output = runExportTask(&exporter, dir, QStringLiteral("trip.svx"));
    INFO(output.toStdString());

    // The shots have to survive the trip through cwTripData.
    CHECK(output.contains(QStringLiteral("a1")));
    CHECK(output.contains(QStringLiteral("a2")));

    // Nothing encloses this trip, so it names the frame and the location
    // itself. *declination auto without *cs out is a cavern error, since the
    // convergence it subtracts is a property of the output system.
    CHECK(output.contains(QStringLiteral("*cs out EPSG:32613")));
    CHECK(output.contains(QStringLiteral("*cs EPSG:32613")));
    CHECK(output.count(QStringLiteral("*declination auto")) == 1);
    CHECK(output.indexOf(QStringLiteral("*cs out")) < output.indexOf(QStringLiteral("*declination auto")));
    CHECK_FALSE(output.contains(QStringLiteral("*calibrate DECLINATION")));

    // Fix stations belong to the cave. A lone trip solves from the origin.
    CHECK_FALSE(output.contains(QStringLiteral("*fix")));
}

TEST_CASE("cwSurvexExporterTripTask: a trip with auto declination off asks for no location",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();
    fixture.calibration->setAutoDeclination(false);
    fixture.calibration->setDeclinationManual(7.5);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    cwSurvexExporterTripTask exporter;
    exporter.setData(fixture.trip->data());
    exporter.setCaveFixStations(fixture.cave->fixStations()->fixStations());
    const QString output = runExportTask(&exporter, dir, QStringLiteral("manual_trip.svx"));
    INFO(output.toStdString());

    // The cave is georeferenced, but this trip carries its own declination, so
    // there is nothing for a location to feed. Naming a frame the trip doesn't
    // use would claim its origin-relative solve sits at those UTM coordinates.
    CHECK_FALSE(output.contains(QStringLiteral("*cs")));
    CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
    CHECK(output.contains(QStringLiteral("*calibrate DECLINATION -7.50")));
}

TEST_CASE("cwSurvexExporterTripTask: a trip from an unfixed cave names no coordinate system at all",
          "[cwSurvexExporter_Auto]")
{
    BoulderFixture fixture = buildUnfixedFixture(QStringLiteral("UnfixedCave"),
                                                 QStringLiteral("UnfixedTrip"));
    fixture.calibration->setDeclinationManual(7.5);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    cwSurvexExporterTripTask exporter;
    exporter.setData(fixture.trip->data());
    exporter.setCaveFixStations(fixture.cave->fixStations()->fixStations());
    const QString output = runExportTask(&exporter, dir, QStringLiteral("unfixed_trip.svx"));
    INFO(output.toStdString());

    CHECK_FALSE(output.contains(QStringLiteral("*cs")));
    CHECK_FALSE(output.contains(QStringLiteral("*declination auto")));
    CHECK(output.contains(QStringLiteral("-7.50")));
}

TEST_CASE("cwSurvexExporterCaveTask: a cave exported on its own names its own *cs out",
          "[cwSurvexExporter_Auto]")
{
    auto fixture = buildBoulderUtmFixture();

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    cwSurvexExporterCaveTask exporter;
    exporter.setData(fixture.cave->data());
    const QString output = runExportTask(&exporter, dir, QStringLiteral("cave.svx"));
    INFO(output.toStdString());

    // Cavern refuses a *fix whose input system has no output system to land in,
    // so the *cs out the region writer normally supplies has to come from here.
    const int outputCSAt = output.indexOf(QStringLiteral("*cs out EPSG:32613"));
    const int fixAt = output.indexOf(QStringLiteral("*fix a1"));
    REQUIRE(outputCSAt >= 0);
    REQUIRE(fixAt >= 0);
    CHECK(outputCSAt < fixAt);
    CHECK(output.count(QStringLiteral("*declination auto")) == 1);
}

TEST_CASE("cwSurvexExporterCaveTask: an unfixed cave exported on its own names no coordinate system",
          "[cwSurvexExporter_Auto]")
{
    BoulderFixture fixture = buildUnfixedFixture(QStringLiteral("UnfixedCave"),
                                                 QStringLiteral("UnfixedTrip"));

    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    cwSurvexExporterCaveTask exporter;
    exporter.setData(fixture.cave->data());
    const QString output = runExportTask(&exporter, dir, QStringLiteral("unfixed_cave.svx"));
    INFO(output.toStdString());

    // Un-fixed projects keep their pre-CS behavior: cavern fixes the first
    // station at the origin.
    CHECK_FALSE(output.contains(QStringLiteral("*cs")));
    CHECK(output.contains(QStringLiteral("*fix a1 0 0 0")));
}

TEST_CASE("cwSurvexExporterUtils::makeDeclinationContext picks the first fix and its own inputCS",
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

    const auto ctx = cwSurvexExporterUtils::makeDeclinationContext({ a, b });
    REQUIRE(ctx.has_value());
    CHECK(ctx->inputCS == kUtmZ13N);
    CHECK(ctx->easting == 478000.0);
    CHECK(ctx->northing == 4430000.0);
    CHECK(ctx->elevation == 1655.0);
}

TEST_CASE("cwSurvexExporterUtils::makeDeclinationContext does not borrow the project's CS",
          "[cwSurvexExporter_Auto]")
{
    // The region's CS is not a stand-in for one the fix never declared: survex
    // would then anchor the station under a projection nobody chose for it.
    cwFixStation a;
    a.setStationName(QStringLiteral("a1"));
    // Written in one call so the numbers actually survive a fix with no CS to
    // read them under — set one at a time they would collapse to 0, and this
    // would be a test about an empty fix rather than a CS-less one.
    a.setCoordinate(478000.0, 4430000.0, 1655.0);
    REQUIRE(a.inputCS().isEmpty());
    REQUIRE(a.coordinate() == QStringLiteral("478000, 4430000, 1655m"));

    CHECK_FALSE(cwSurvexExporterUtils::makeDeclinationContext({ a }).has_value());

    // The numbers were never the problem — naming the system is all it takes,
    // which is what makes the case above a refusal rather than an empty fix.
    a.setInputCS(kUtmZ13N);
    const auto named = cwSurvexExporterUtils::makeDeclinationContext({ a });
    REQUIRE(named.has_value());
    CHECK(named->inputCS == kUtmZ13N);
    CHECK(named->easting == 478000.0);
}

TEST_CASE("cwSurvexExporterUtils::makeDeclinationContext is invalid with no fix",
          "[cwSurvexExporter_Auto]")
{
    const auto ctx = cwSurvexExporterUtils::makeDeclinationContext({});
    CHECK_FALSE(ctx.has_value());
}
