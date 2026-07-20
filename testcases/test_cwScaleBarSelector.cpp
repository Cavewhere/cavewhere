/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwScaleBarSelector.h"
#include "cwUnits.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace Catch;

// The on-screen view bar (ScaleBar.qml) rounds its per-cell increment to the
// same {1,2,5}×10ⁿ candidates as the printed export bar, so the two can no
// longer drift. selectViewScale() is the pure choice the QML bar invokes.
TEST_CASE("cwScaleBarSelector picks a round view increment that best fills the span",
          "[ScaleBarSelector][UnitSystem]")
{
    cwScaleBarSelector selector;

    SECTION("metric picks round metres for a small span") {
        // 10 px/m, 5 cells: candidate V metres draws 50·V px. The [100,400] band
        // admits 2 m (100 px) and 5 m (250 px); best (350 px) is nearer 5 m.
        const auto s = selector.selectViewScale(10.0, 100.0, 400.0, 5, cwUnits::Metric);
        REQUIRE(s.valid);
        CHECK(s.value == Approx(5.0));
        CHECK(s.unit == cwUnits::Meters);
    }

    SECTION("metric crosses to round kilometres for a large span") {
        // 0.001 px/m, 5 cells: candidate W km draws 5·W px. The [5,40] band admits
        // 1/2/5 km; best (37.5 px) is nearest 5 km.
        const auto s = selector.selectViewScale(0.001, 5.0, 40.0, 5, cwUnits::Metric);
        REQUIRE(s.valid);
        CHECK(s.value == Approx(5.0));
        CHECK(s.unit == cwUnits::Kilometers);
    }

    SECTION("imperial rounds in feet, not converted metres") {
        // Same 1:1 px/m geometry as the metric case but imperial: the increment
        // is a round foot count in its own unit, never a metre conversion.
        const auto s = selector.selectViewScale(10.0, 100.0, 400.0, 5, cwUnits::Imperial);
        REQUIRE(s.valid);
        CHECK(s.unit == cwUnits::Feet);
    }
}

TEST_CASE("cwScaleBarSelector reports invalid when no round increment fits",
          "[ScaleBarSelector]")
{
    cwScaleBarSelector selector;

    SECTION("a non-positive scale is invalid") {
        CHECK_FALSE(selector.selectViewScale(0.0, 100.0, 400.0, 5, cwUnits::Metric).valid);
    }

    SECTION("a non-positive cell count is invalid") {
        CHECK_FALSE(selector.selectViewScale(10.0, 100.0, 400.0, 0, cwUnits::Metric).valid);
    }

    SECTION("a band too narrow to admit any candidate is invalid") {
        // At 10 px/m the nearest candidates draw 100 px (2 m) and 250 px (5 m);
        // a [120,130] px band admits neither.
        CHECK_FALSE(selector.selectViewScale(10.0, 120.0, 130.0, 5, cwUnits::Metric).valid);
    }
}

// The two bars must round to the same numbers: every export candidate is a nice
// number the view bar can also land on, because both read one candidate list.
TEST_CASE("cwScaleBarSelector candidate lengths are shared by both bars",
          "[ScaleBarSelector]")
{
    const auto metric = cwScaleBarSelector::niceCandidates(cwUnits::Metric);
    REQUIRE_FALSE(metric.empty());

    // Ascending by metres, and each candidate labels in the unit cwUnits picks
    // for its magnitude — the crossover is owned by cwUnits, not restated here.
    for (std::size_t i = 1; i < metric.size(); ++i) {
        CHECK(metric[i - 1].meters <= metric[i].meters);
    }
    for (const auto& candidate : metric) {
        CHECK(candidate.unit == cwUnits::magnitudeUnit(candidate.meters, cwUnits::Metric));
    }
}
