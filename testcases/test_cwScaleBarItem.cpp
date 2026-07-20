/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScaleBarItem.h"
#include "cwUnits.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace Catch;

// The export scale bar (#470) must label round numbers in the project's unit
// system — round metres/kilometres for Metric, round feet/miles for Imperial —
// never a converted-from-metric fraction. selectScale() is the pure label math.
TEST_CASE("cwScaleBarItem selects round scale lengths per unit system", "[ScaleBarItem][UnitSystem]")
{
    // A wide frame so the smallest candidate that reaches the minimum bar width
    // wins deterministically.
    constexpr double kWideInches = 4.0;

    SECTION("metric picks round metres at a small scale") {
        const auto s = cwScaleBarItem::selectScale(1.0 / 100.0, kWideInches, cwUnits::Metric);
        REQUIRE(s.valid);
        CHECK(s.value == Approx(2.0));
        CHECK(s.unit == cwUnits::Meters);
    }

    SECTION("metric crosses to round kilometres at a large scale") {
        const auto s = cwScaleBarItem::selectScale(1.0 / 100000.0, kWideInches, cwUnits::Metric);
        REQUIRE(s.valid);
        CHECK(s.value == Approx(2.0));
        CHECK(s.unit == cwUnits::Kilometers);
    }

    SECTION("imperial picks round feet at a small scale") {
        const auto s = cwScaleBarItem::selectScale(1.0 / 100.0, kWideInches, cwUnits::Imperial);
        REQUIRE(s.valid);
        CHECK(s.value == Approx(10.0));
        CHECK(s.unit == cwUnits::Feet);
    }

    SECTION("imperial crosses to round miles at a large scale") {
        const auto s = cwScaleBarItem::selectScale(1.0 / 100000.0, kWideInches, cwUnits::Imperial);
        REQUIRE(s.valid);
        CHECK(s.value == Approx(2.0));
        CHECK(s.unit == cwUnits::Miles);
    }

    SECTION("the same paper scale yields different round lengths per system") {
        // A 1:20000 map: metric reads 500 m, imperial reads round feet — each a
        // round number in its own unit, not a conversion of the other.
        const auto metric = cwScaleBarItem::selectScale(1.0 / 20000.0, kWideInches, cwUnits::Metric);
        const auto imperial = cwScaleBarItem::selectScale(1.0 / 20000.0, kWideInches, cwUnits::Imperial);
        REQUIRE(metric.valid);
        REQUIRE(imperial.valid);
        CHECK(metric.unit == cwUnits::Meters);
        CHECK(metric.value == Approx(500.0));
        CHECK(imperial.unit == cwUnits::Feet);
        // A round foot count, not 500 m converted (which would be ~1640 ft).
        CHECK(imperial.value == Approx(2000.0));
    }
}

TEST_CASE("cwScaleBarItem selectScale reports when nothing fits", "[ScaleBarItem]")
{
    SECTION("a non-positive scale ratio is invalid") {
        CHECK_FALSE(cwScaleBarItem::selectScale(0.0, 4.0, cwUnits::Metric).valid);
        CHECK_FALSE(cwScaleBarItem::selectScale(-1.0, 4.0, cwUnits::Metric).valid);
    }

    SECTION("a non-positive available width is invalid") {
        CHECK_FALSE(cwScaleBarItem::selectScale(1.0 / 100.0, 0.0, cwUnits::Metric).valid);
    }

    SECTION("too little room for even the smallest bar is invalid") {
        // At 1:100 the smallest candidate still needs more than this width.
        CHECK_FALSE(cwScaleBarItem::selectScale(1.0 / 100.0, 0.001, cwUnits::Metric).valid);
    }
}
