/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwShotMeasurement.h"

//Test includes
#include "SplayFixtureHelper.h"

TEST_CASE("cwShotMeasurement::reversed turns a wall shot around", "[cwShotMeasurement][SplayShot]") {
    SECTION("the bearing goes half a turn and the clino flips sign") {
        CHECK(makeSplay("5", "90", "-10").reversed() == makeSplay("5", "270", "10"));
        CHECK(makeSplay("5", "10", "20").reversed() == makeSplay("5", "190", "-20"));
    }

    SECTION("a bearing past half a turn wraps instead of running past 360") {
        CHECK(makeSplay("5", "270", "0").reversed() == makeSplay("5", "90", "0"));
        CHECK(makeSplay("5", "359", "0").reversed() == makeSplay("5", "179", "0"));
    }

    SECTION("a level shot stays at zero rather than reading -0") {
        const cwShotMeasurement reversed = makeSplay("5", "180", "0").reversed();
        CHECK(reversed.compass.value() == QStringLiteral("0"));
        CHECK(reversed.clino.value() == QStringLiteral("0"));
    }

    SECTION("a plumb swaps up for down and keeps its empty bearing") {
        const cwShotMeasurement up(cwDistanceReading(QStringLiteral("2")),
                                   cwCompassReading(),
                                   cwClinoReading(QStringLiteral("up")));

        const cwShotMeasurement reversed = up.reversed();
        CHECK(reversed.clino.state() == cwClinoReading::State::Down);
        CHECK(reversed.compass.state() == cwCompassReading::State::Empty);
        CHECK(reversed.reversed().clino.state() == cwClinoReading::State::Up);
    }

    SECTION("the distance is left alone, since turning around doesn't move the wall") {
        CHECK(makeSplay("7.25", "0", "0").reversed().distance.value()
              == QStringLiteral("7.25"));
    }

    SECTION("a reading nobody can read comes back untouched") {
        const cwShotMeasurement junk(cwDistanceReading(QStringLiteral("5")),
                                     cwCompassReading(QStringLiteral("not a bearing")),
                                     cwClinoReading(QStringLiteral("sideways")));

        const cwShotMeasurement reversed = junk.reversed();
        CHECK(reversed.compass.value() == QStringLiteral("not a bearing"));
        CHECK(reversed.clino.value() == QStringLiteral("sideways"));
    }

    SECTION("turning a shot around twice gives back where it started") {
        const cwShotMeasurement original = makeSplay("5", "123.5", "-42.5");
        CHECK(original.reversed().reversed() == original);
    }
}
