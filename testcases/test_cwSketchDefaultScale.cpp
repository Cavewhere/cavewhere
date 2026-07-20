/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwLength.h"
#include "cwScale.h"
#include "cwSketch.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwTrip.h"
#include "cwUnits.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace Catch;

namespace {
    // The two round sketching defaults A3 seeds: metric 1 cm = 2.5 m (1:250) and
    // imperial 1 in = 20 ft (1:240). The scale ratio is unit-independent, so it
    // is asserted with Approx rather than an exact compare.
    constexpr double kMetricRatio = 1.0 / 250.0;
    constexpr double kImperialRatio = 1.0 / 240.0;

    void checkMetricScale(const cwScale* scale)
    {
        CHECK(scale->scaleNumerator()->unit() == cwUnits::Centimeters);
        CHECK(scale->scaleNumerator()->value() == Approx(1.0));
        CHECK(scale->scaleDenominator()->unit() == cwUnits::Meters);
        CHECK(scale->scaleDenominator()->value() == Approx(2.5));
        CHECK(scale->scale() == Approx(kMetricRatio));
    }

    void checkImperialScale(const cwScale* scale)
    {
        CHECK(scale->scaleNumerator()->unit() == cwUnits::Inches);
        CHECK(scale->scaleNumerator()->value() == Approx(1.0));
        CHECK(scale->scaleDenominator()->unit() == cwUnits::Feet);
        CHECK(scale->scaleDenominator()->value() == Approx(20.0));
        CHECK(scale->scale() == Approx(kImperialRatio));
    }
}

// seedDefaultScale is the single source of the per-system default sketch scale.
TEST_CASE("cwSketch::seedDefaultScale seeds the project unit system's default scale",
          "[SketchDefaultScale]")
{
    cwSketch sketch;

    SECTION("metric seeds 1 cm = 2.5 m") {
        sketch.seedDefaultScale(cwUnits::Metric);
        checkMetricScale(sketch.mapScale());
    }

    SECTION("imperial seeds 1 in = 20 ft") {
        sketch.seedDefaultScale(cwUnits::Imperial);
        checkImperialScale(sketch.mapScale());
    }
}

// A new sketch added to a trip inherits the paper-scale units from its region's
// unit system — the same conduit cwCave uses to seed a new trip's survey unit.
TEST_CASE("cwSurveyNoteSketchModel::addSketch seeds the region's unit system",
          "[SketchDefaultScale]")
{
    cwCavingRegion region;
    auto* cave = new cwCave();
    auto* trip = new cwTrip();
    cave->addTrip(trip);
    region.addCave(cave);

    SECTION("metric region → metric sketch scale") {
        region.setUnitSystem(cwUnits::Metric);
        cwSketch* sketch = trip->notesSketch()->addSketch(cwSketch::Plan);
        REQUIRE(sketch != nullptr);
        checkMetricScale(sketch->mapScale());
    }

    SECTION("imperial region → imperial sketch scale") {
        region.setUnitSystem(cwUnits::Imperial);
        cwSketch* sketch = trip->notesSketch()->addSketch(cwSketch::Plan);
        REQUIRE(sketch != nullptr);
        checkImperialScale(sketch->mapScale());
    }
}

// A trip not yet attached to a region has no project default to read, so a new
// sketch falls back to metric — matching cwCave's region-less trip fallback.
TEST_CASE("cwSurveyNoteSketchModel::addSketch falls back to metric with no region",
          "[SketchDefaultScale]")
{
    cwTrip trip;
    cwSketch* sketch = trip.notesSketch()->addSketch(cwSketch::Plan);
    REQUIRE(sketch != nullptr);
    checkMetricScale(sketch->mapScale());
}
