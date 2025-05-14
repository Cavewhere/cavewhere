// cwShot_test.cpp
// Catch2 unit tests for cwShot

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwShot.h"
#include "cwShotMeasurement.h"

TEST_CASE("Default ctor measurementCount and flags", "[cwShot]") {
    cwShot shot;
    REQUIRE(shot.measurementCount() == 0);
    REQUIRE(shot.isDistanceIncluded() == true);
    REQUIRE_FALSE(shot.isValid());
}

TEST_CASE("Legacy ctor creates two measurements", "[cwShot]") {
    cwShot shot("12.34", "123.4", "234.5", "10.0", "20.0");
    REQUIRE(shot.measurementCount() == 2);

    const auto& m0 = shot.measurementAt(0);
    REQUIRE(m0.direction == cwShotMeasurement::Direction::Front);
    REQUIRE(m0.distance.value().toStdString() == "12.34");
    REQUIRE(m0.compass.value().toStdString() == "123.4");
    REQUIRE(m0.clino.value().toStdString() == "10.0");

    const auto& m1 = shot.measurementAt(1);
    REQUIRE(m1.direction == cwShotMeasurement::Direction::Back);
    REQUIRE(m1.distance.value().toStdString() == "12.34");
    REQUIRE(m1.compass.value().toStdString() == "234.5");
    REQUIRE(m1.clino.value().toStdString() == "20.0");

    REQUIRE(shot.isValid());
}

TEST_CASE("Legacy API getters and setters on empty shot", "[cwShot]") {
    cwShot shot;
    REQUIRE(shot.measurementCount() == 0);

    shot.setDistance(cwDistanceReading("5.5"));
    REQUIRE(shot.measurementCount() == 1);
    REQUIRE(shot.distance().value().toStdString() == "5.5");

    shot.setCompass(cwCompassReading("45"));
    REQUIRE(shot.compass().value().toStdString() == "45");

    shot.setClino(cwClinoReading("3"));
    REQUIRE(shot.clino().value().toStdString() == "3");

    REQUIRE(shot.isValid());
}

TEST_CASE("Back getters and setters", "[cwShot]") {
    cwShot shot;
    shot.setDistance(cwDistanceReading("1"));
    shot.setBackCompass(cwCompassReading("180"));
    REQUIRE(shot.measurementCount() == 2);
    REQUIRE(shot.backCompass().value().toStdString() == "180");

    shot.setBackClino(cwClinoReading("5"));
    REQUIRE(shot.backClino().value().toStdString() == "5");

    REQUIRE(shot.compass().value().isEmpty());
    REQUIRE(shot.clino().value().isEmpty());
}

TEST_CASE("Multi-measurement API", "[cwShot]") {
    cwShot shot;
    cwShotMeasurement mA;
    mA.direction = cwShotMeasurement::Direction::Front;
    shot.addMeasurement(mA);
    REQUIRE(shot.measurementCount() == 1);

    cwShotMeasurement mB;
    mB.direction = cwShotMeasurement::Direction::Back;
    shot.addMeasurement(mB);
    REQUIRE(shot.measurementCount() == 2);

    shot.removeMeasurementAt(0);
    REQUIRE(shot.measurementCount() == 1);
    REQUIRE(shot.measurementAt(0).direction == cwShotMeasurement::Direction::Back);
}

TEST_CASE("Include distance flag toggling", "[cwShot]") {
    cwShot shot;
    REQUIRE(shot.isDistanceIncluded());
    shot.setDistanceIncluded(false);
    REQUIRE_FALSE(shot.isDistanceIncluded());
    shot.setDistanceIncluded(true);
    REQUIRE(shot.isDistanceIncluded());
}
