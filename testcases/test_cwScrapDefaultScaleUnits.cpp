/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwCave.h"
#include "cwLength.h"
#include "cwNote.h"
#include "cwNoteTranformation.h"
#include "cwScale.h"
#include "cwScrap.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "cwUnits.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace Catch;

// The shared paper-scale default both the sketch map scale and a new scrap's
// note-transformation scale round to.
TEST_CASE("cwScale::defaultData gives the project unit system's paper scale",
          "[ScrapDefaultScale]")
{
    SECTION("metric is 1 cm = 2.5 m") {
        const cwScale::Data d = cwScale::defaultData(cwUnits::Metric);
        CHECK(d.scaleNumerator.unit == cwUnits::Centimeters);
        CHECK(d.scaleNumerator.value == Approx(1.0));
        CHECK(d.scaleDenominator.unit == cwUnits::Meters);
        CHECK(d.scaleDenominator.value == Approx(2.5));
    }

    SECTION("imperial is 1 in = 20 ft") {
        const cwScale::Data d = cwScale::defaultData(cwUnits::Imperial);
        CHECK(d.scaleNumerator.unit == cwUnits::Inches);
        CHECK(d.scaleNumerator.value == Approx(1.0));
        CHECK(d.scaleDenominator.unit == cwUnits::Feet);
        CHECK(d.scaleDenominator.value == Approx(20.0));
    }
}

// A scrap's note-transformation scale is auto-calculated, but the display units
// it reads in should follow the project unit system, not the raw-inches default.
TEST_CASE("cwScrap::seedDefaultScale sets the note-transformation display units",
          "[ScrapDefaultScale]")
{
    cwScrap scrap;

    SECTION("metric seeds cm / m") {
        scrap.seedDefaultScale(cwUnits::Metric);
        CHECK(scrap.noteTransformation()->scaleNumerator()->unit() == cwUnits::Centimeters);
        CHECK(scrap.noteTransformation()->scaleDenominator()->unit() == cwUnits::Meters);
    }

    SECTION("imperial seeds in / ft") {
        scrap.seedDefaultScale(cwUnits::Imperial);
        CHECK(scrap.noteTransformation()->scaleNumerator()->unit() == cwUnits::Inches);
        CHECK(scrap.noteTransformation()->scaleDenominator()->unit() == cwUnits::Feet);
    }
}

// Regression guard: the calculator emits the scale in raw inches, so a live
// auto-recompute must re-express it in the scrap's seeded display units rather
// than snapping the display back to inches.
TEST_CASE("Auto note-transform recompute keeps the scrap's display units",
          "[ScrapDefaultScale]")
{
    cwCave cave;
    auto* trip = new cwTrip(&cave);
    cave.addTrip(trip);
    auto* note = new cwNote();
    trip->notes()->addNotes({ note });
    auto* scrap = new cwScrap();
    note->addScrap(scrap);

    REQUIRE(scrap->calculateNoteTransform());

    scrap->seedDefaultScale(cwUnits::Metric);
    scrap->updateNoteTransformation();

    CHECK(scrap->noteTransformation()->scaleNumerator()->unit() == cwUnits::Centimeters);
    CHECK(scrap->noteTransformation()->scaleDenominator()->unit() == cwUnits::Meters);
}
