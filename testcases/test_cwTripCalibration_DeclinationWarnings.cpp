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
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

//Qt includes
#include <QDateTime>
#include <QSignalSpy>
#include <QTimeZone>

//Std includes
#include <memory>

namespace {

const QString Wgs84 = QStringLiteral("EPSG:4326");

QDateTime makeDate(int year, int month, int day)
{
    return QDateTime(QDate(year, month, day), QTime(0, 0), QTimeZone::UTC);
}

struct Fixture {
    std::unique_ptr<cwCavingRegion> region;
    cwCave* cave;
    cwTrip* trip;
    cwTripCalibration* calibration;
};

Fixture buildBoulderFixture()
{
    auto region = std::make_unique<cwCavingRegion>();
    auto* cave = new cwCave();
    region->addCave(cave);

    cwFixStation fix;
    fix.setStationName(QStringLiteral("Entrance"));
    fix.setInputCS(Wgs84);
    fix.setEasting(-105.27);
    fix.setNorthing(40.015);
    fix.setElevation(1655.0);
    cave->fixStations()->appendFixStation(fix);

    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setDate(makeDate(2024, 6, 1));

    return { std::move(region), cave, trip, trip->calibrations() };
}

int tripWarningCount(cwTrip* trip)
{
    return trip->errorModel()->warningCount();
}

bool tripHasMessage(cwTrip* trip, const QString& substring)
{
    const auto errors = trip->errorModel()->errors()->toList();
    for (const auto& err : errors) {
        if (err.message().contains(substring)) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("declinationWarning: silent when auto resolves and matches manual closely",
          "[cwTripCalibration_DeclinationWarnings]")
{
    // Manual within 0.5° of computed is silent.
    auto f = buildBoulderFixture();
    const double autoValue = f.calibration->declination();

    f.calibration->setAutoDeclination(false);
    f.calibration->setDeclinationManual(autoValue + 0.4);

    CHECK(f.calibration->declinationWarning().isEmpty());
    CHECK(tripWarningCount(f.trip) == 0);
}

TEST_CASE("declinationWarning: warns when manual differs from computed by >=0.5°",
          "[cwTripCalibration_DeclinationWarnings]")
{
    auto f = buildBoulderFixture();
    const double autoValue = f.calibration->declination();

    f.calibration->setAutoDeclination(false);
    f.calibration->setDeclinationManual(autoValue + 0.6);

    CHECK(!f.calibration->declinationWarning().isEmpty());
    CHECK(f.calibration->declinationWarning().contains(QStringLiteral("differs")));
    CHECK(tripHasMessage(f.trip,QStringLiteral("differs")));
    CHECK(tripWarningCount(f.trip) == 1);
}

TEST_CASE("declinationWarning: warning clears when manual is brought back into range",
          "[cwTripCalibration_DeclinationWarnings]")
{
    auto f = buildBoulderFixture();
    const double autoValue = f.calibration->declination();
    f.calibration->setAutoDeclination(false);
    f.calibration->setDeclinationManual(autoValue + 3.0);
    REQUIRE(!f.calibration->declinationWarning().isEmpty());
    REQUIRE(tripWarningCount(f.trip) == 1);

    f.calibration->setDeclinationManual(autoValue);
    CHECK(f.calibration->declinationWarning().isEmpty());
    CHECK(tripWarningCount(f.trip) == 0);
}

TEST_CASE("declinationWarning: auto-mode + missing date fires missing-date warning",
          "[cwTripCalibration_DeclinationWarnings]")
{
    auto f = buildBoulderFixture();
    REQUIRE(f.calibration->autoDeclination() == true);

    f.trip->setDate(QDateTime());
    CHECK(!f.calibration->declinationWarning().isEmpty());
    CHECK(f.calibration->declinationWarning().contains(QStringLiteral("no date")));
    CHECK(tripHasMessage(f.trip,QStringLiteral("no date")));
}

TEST_CASE("declinationWarning: manual-mode + missing date does NOT warn",
          "[cwTripCalibration_DeclinationWarnings]")
{
    auto f = buildBoulderFixture();
    f.calibration->setAutoDeclination(false);
    f.calibration->setDeclinationManual(0.0);

    f.trip->setDate(QDateTime());
    // No date → resolveAuto fails → rule 2 can't fire, rule 1 needs auto=on.
    CHECK(f.calibration->declinationWarning().isEmpty());
    CHECK(tripWarningCount(f.trip) == 0);
}

TEST_CASE("declinationWarning: missing-date warning clears when date is restored",
          "[cwTripCalibration_DeclinationWarnings]")
{
    auto f = buildBoulderFixture();
    f.trip->setDate(QDateTime());
    REQUIRE(!f.calibration->declinationWarning().isEmpty());
    REQUIRE(tripWarningCount(f.trip) == 1);

    f.trip->setDate(makeDate(2024, 6, 1));
    CHECK(f.calibration->declinationWarning().isEmpty());
    CHECK(tripWarningCount(f.trip) == 0);
}

TEST_CASE("declinationWarning: no fix station → no warning even when manual is far from zero",
          "[cwTripCalibration_DeclinationWarnings]")
{
    // resolveAuto fails (no fix station), so rule 2 cannot fire — we don't
    // know what the "computed" value should be, so silence is correct.
    auto region = std::make_unique<cwCavingRegion>();
    auto* cave = new cwCave();
    region->addCave(cave);
    cave->addTrip();
    cwTrip* trip = cave->trip(0);
    trip->setDate(makeDate(2024, 6, 1));

    cwTripCalibration* calibration = trip->calibrations();
    calibration->setAutoDeclination(false);
    calibration->setDeclinationManual(42.0);

    CHECK(calibration->declinationWarning().isEmpty());
    CHECK(tripWarningCount(trip) == 0);
}

TEST_CASE("declinationWarning: declinationWarningChanged emits on transitions",
          "[cwTripCalibration_DeclinationWarnings]")
{
    auto f = buildBoulderFixture();
    const double autoValue = f.calibration->declination();
    f.calibration->setAutoDeclination(false);
    f.calibration->setDeclinationManual(autoValue);

    QSignalSpy spy(f.calibration, &cwTripCalibration::declinationWarningChanged);

    f.calibration->setDeclinationManual(autoValue + 5.0);
    CHECK(spy.count() == 1);

    // Re-edit to a still-warning value with a different number — the message
    // string changes, so the signal fires again.
    f.calibration->setDeclinationManual(autoValue + 3.0);
    CHECK(spy.count() == 2);

    f.calibration->setDeclinationManual(autoValue);
    CHECK(spy.count() == 3);
    CHECK(f.calibration->declinationWarning().isEmpty());
}
