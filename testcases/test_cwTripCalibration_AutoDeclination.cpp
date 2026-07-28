/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch2 includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Our includes
#include "BoulderFixtureHelper.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwDeclination.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QDateTime>
#include <QSignalSpy>

//Std includes
#include <memory>

using Catch::Matchers::WithinAbs;

TEST_CASE("cwTripCalibration: default-constructed defaults", "[cwTripCalibration_Auto]")
{
    cwTripCalibration calibration;
    CHECK(calibration.autoDeclination() == true);
    CHECK(calibration.declinationManual() == 0.0);
    CHECK(calibration.autoDeclinationAvailable() == false);
    CHECK(calibration.declination() == 0.0);
}

TEST_CASE("cwTripCalibration: auto on + fix station + trip date resolves to IGRF value", "[cwTripCalibration_Auto]")
{
    auto fixture = buildBoulderFixture();
    fixture.calibration->setDeclinationManual(99.0); // sentinel to prove manual is ignored

    REQUIRE(fixture.calibration->autoDeclination() == true);
    REQUIRE(fixture.calibration->autoDeclinationAvailable() == true);

    const double resolved = fixture.calibration->declination();
    CHECK(resolved > 5.0);
    CHECK(resolved < 11.0);
    CHECK(resolved != 99.0);
}

TEST_CASE("cwTripCalibration: auto off returns the stored manual value even when fix station exists", "[cwTripCalibration_Auto]")
{
    auto fixture = buildBoulderFixture();
    fixture.calibration->setAutoDeclination(false);
    fixture.calibration->setDeclinationManual(12.34);

    CHECK(fixture.calibration->autoDeclinationAvailable() == true);
    CHECK(fixture.calibration->declination() == 12.34);
}

TEST_CASE("cwTripCalibration: no fix station → autoDeclinationAvailable false, falls back to manual", "[cwTripCalibration_Auto]")
{
    auto region = std::make_unique<cwCavingRegion>();
    auto* cave = new cwCave();
    region->addCave(cave);
    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setDate(makeUtcDate(2024, 6, 1));

    cwTripCalibration* calibration = trip->calibrations();
    calibration->setDeclinationManual(7.5);

    CHECK(calibration->autoDeclination() == true);
    CHECK(calibration->autoDeclinationAvailable() == false);
    CHECK(calibration->declination() == 7.5);
}

TEST_CASE("cwTripCalibration: declinationChanged fires when fix station is added to parent cave", "[cwTripCalibration_Auto]")
{
    auto region = std::make_unique<cwCavingRegion>();
    auto* cave = new cwCave();
    region->addCave(cave);
    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setDate(makeUtcDate(2024, 6, 1));
    cwTripCalibration* calibration = trip->calibrations();

    REQUIRE(calibration->autoDeclinationAvailable() == false);
    REQUIRE(calibration->declination() == 0.0);

    QSignalSpy declinationSpy(calibration, &cwTripCalibration::declinationChanged);
    QSignalSpy availableSpy(calibration, &cwTripCalibration::autoDeclinationAvailableChanged);

    cwFixStation fix;
    fix.setStationName(QStringLiteral("Entrance"));
    fix.setInputCS(kWgs84);
    fix.setEasting(-105.27);
    fix.setNorthing(40.015);
    fix.setElevation(1655.0);
    cave->fixStations()->appendFixStation(fix);

    CHECK(availableSpy.count() == 1);
    CHECK(calibration->autoDeclinationAvailable() == true);
    CHECK(declinationSpy.count() == 1);
    CHECK(calibration->declination() > 5.0);
}

TEST_CASE("cwTripCalibration: declinationChanged fires when trip date changes", "[cwTripCalibration_Auto]")
{
    auto fixture = buildBoulderFixture();

    const double resolvedAt2024 = fixture.calibration->declination();
    QSignalSpy declinationSpy(fixture.calibration, &cwTripCalibration::declinationChanged);

    // 44-year IGRF drift at Boulder is well above numerical noise.
    fixture.trip->setDate(makeUtcDate(1980, 6, 1));

    CHECK(declinationSpy.count() >= 1);
    const double resolvedAt1980 = fixture.calibration->declination();
    CHECK(resolvedAt2024 != resolvedAt1980);
}

TEST_CASE("cwTripCalibration: declinationManualChanged fires only on manual edit; declinationChanged fires only when resolved moves", "[cwTripCalibration_Auto]")
{
    auto fixture = buildBoulderFixture();
    const double autoValue = fixture.calibration->declination();
    REQUIRE(fixture.calibration->autoDeclination() == true);

    QSignalSpy declinationSpy(fixture.calibration, &cwTripCalibration::declinationChanged);
    QSignalSpy manualSpy(fixture.calibration, &cwTripCalibration::declinationManualChanged);

    fixture.calibration->setDeclinationManual(42.0);
    CHECK(manualSpy.count() == 1);
    CHECK(declinationSpy.count() == 0);
    CHECK(fixture.calibration->declinationManual() == 42.0);
    CHECK(fixture.calibration->declination() == autoValue);

    fixture.calibration->setAutoDeclination(false);
    CHECK(declinationSpy.count() == 1);
    CHECK(fixture.calibration->declination() == 42.0);
}

TEST_CASE("cwTripCalibration: setData round-trips the autoDeclination flag through cwTripCalibrationData", "[cwTripCalibration_Auto]")
{
    cwTripCalibration source;
    source.setAutoDeclination(false);
    source.setDeclinationManual(3.25);

    cwTripCalibrationData snapshot = source.data();
    CHECK(snapshot.autoDeclination() == false);
    CHECK(snapshot.declinationManual() == 3.25);

    cwTripCalibration sink;
    REQUIRE(sink.autoDeclination() == true);
    sink.setData(snapshot);
    CHECK(sink.autoDeclination() == false);
    CHECK(sink.declinationManual() == 3.25);
}

TEST_CASE("cwTripCalibration: a fix row with no coordinate yet resolves nothing",
          "[cwTripCalibration_Auto]")
{
    // "Mark Station as Fixed" creates a row that carries a coordinate system
    // from the moment it exists, before the user has typed anything. Reading its
    // components anyway resolves IGRF at (0, 0) — the Gulf of Guinea — and
    // silently rotates every trip in the cave away from the manual value.
    auto region = std::make_unique<cwCavingRegion>();
    auto* cave = new cwCave();
    region->addCave(cave);
    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setDate(makeUtcDate(2024, 6, 1));

    cwTripCalibration* calibration = trip->calibrations();
    calibration->setDeclinationManual(7.5);

    cave->fixStations()->addFixStation(QStringLiteral("Entrance"));
    REQUIRE(cave->fixStations()->count() == 1);
    REQUIRE(cave->fixStations()->fixStationAt(0).inputCS() == cwCoordinateTransform::Wgs84);
    REQUIRE(cave->fixStations()->fixStationAt(0).state() == cwFixStation::Empty);

    CHECK(calibration->autoDeclinationAvailable() == false);
    CHECK(calibration->declination() == 7.5);

    // The premise: a row that does carry a coordinate still resolves, so the
    // checks above are about the missing coordinate and not about the row.
    const QModelIndex index = cave->fixStations()->index(0);
    cave->fixStations()->setData(index, -105.27, cwFixStationModel::EastingRole);
    cave->fixStations()->setData(index, 40.015, cwFixStationModel::NorthingRole);
    cave->fixStations()->setData(index, 1655.0, cwFixStationModel::ElevationRole);

    CHECK(calibration->autoDeclinationAvailable() == true);
    CHECK(calibration->declination() > 5.0);
}

TEST_CASE("cwTripCalibration: a later fix supplies the declination when the first can't",
          "[cwTripCalibration_Auto]")
{
    // The first row is the scaffold one; the real fix is behind it. Taking row 0
    // and giving up would leave the whole cave on its manual declination.
    auto region = std::make_unique<cwCavingRegion>();
    auto* cave = new cwCave();
    region->addCave(cave);
    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setDate(makeUtcDate(2024, 6, 1));

    cwTripCalibration* calibration = trip->calibrations();
    calibration->setDeclinationManual(7.5);

    cave->fixStations()->addFixStation(QStringLiteral("Scaffold"));

    cwFixStation real;
    real.setStationName(QStringLiteral("Entrance"));
    real.setInputCS(kWgs84);
    // Easting is the longitude on a geographic system.
    real.setCoordinate(-105.27, 40.015, 1655.0);
    cave->fixStations()->appendFixStation(real);

    CHECK(calibration->autoDeclinationAvailable() == true);
    CHECK(calibration->declination() > 5.0);
    CHECK(calibration->declination() != 7.5);
}
