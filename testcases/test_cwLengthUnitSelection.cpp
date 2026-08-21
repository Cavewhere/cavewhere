/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwLengthUnitSelection.h"
#include "cwUnits.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QStringList>

using namespace Catch;

TEST_CASE("cwLengthUnitSelection exposes the curated set", "[cwLengthUnitSelection]")
{
    cwLengthUnitSelection selection;

    SECTION("names are the curated set in menu order") {
        CHECK(selection.names()
              == QStringList({QStringLiteral("m"), QStringLiteral("km"),
                              QStringLiteral("ft"), QStringLiteral("mi")}));
        // The static accessor is the same single source of truth.
        CHECK(cwLengthUnitSelection::unitNames() == selection.names());
    }

    SECTION("defaults to metres at index 0") {
        CHECK(selection.unit() == cwUnits::Meters);
        CHECK(selection.index() == 0);
        CHECK(selection.name() == QStringLiteral("m"));
    }

    SECTION("index maps to the curated unit both ways") {
        selection.setIndex(2);
        CHECK(selection.unit() == cwUnits::Feet);
        CHECK(selection.index() == 2);
        CHECK(selection.name() == QStringLiteral("ft"));

        selection.setUnit(cwUnits::Miles);
        CHECK(selection.index() == 3);
    }

    SECTION("out-of-range indices are ignored") {
        selection.setIndex(2);
        selection.setIndex(-1);
        selection.setIndex(99);
        CHECK(selection.unit() == cwUnits::Feet);
    }

    SECTION("an unlisted unit coerces to metres") {
        // Inches isn't in the curated set, so it can't be selected.
        selection.setUnit(cwUnits::Inches);
        CHECK(selection.unit() == cwUnits::Meters);
    }

    SECTION("converts to and from the selected unit") {
        selection.setUnit(cwUnits::Feet);
        CHECK(selection.fromMeters(60.0) == Approx(196.8503937));
        CHECK(selection.toMeters(196.8503937) == Approx(60.0));
    }
}

TEST_CASE("cwLengthUnitSelection formats a length in the selected unit",
          "[cwLengthUnitSelection]")
{
    cwLengthUnitSelection selection;

    // What this class adds to cwUnits::formatLength is the selected unit; the
    // rendering itself (decimals, suffix, sign collapse) is pinned by [cwUnits].
    SECTION("converts to the selected unit before formatting") {
        CHECK(selection.format(60.0) == QStringLiteral("60.000 m"));

        selection.setUnit(cwUnits::Feet);
        CHECK(selection.format(60.0) == QStringLiteral("196.85 ft"));
    }

    SECTION("the signed flag reaches the formatter") {
        selection.setUnit(cwUnits::Feet);
        CHECK(selection.format(60.0, true) == QStringLiteral("+196.85 ft"));
        CHECK(selection.format(60.0) == QStringLiteral("196.85 ft"));
    }
}

TEST_CASE("cwLengthUnitSelection holds no persisted or shared state",
          "[cwLengthUnitSelection]")
{
    // The selection is in-memory only: its owner re-seeds it from the project's
    // unit system, so one instance's choice must never leak into another — as a
    // persisted global unit once did, defeating the project default (#614).
    cwLengthUnitSelection first;
    first.setUnit(cwUnits::Feet);
    CHECK(first.unit() == cwUnits::Feet);

    cwLengthUnitSelection second;
    CHECK(second.unit() == cwUnits::Meters);
}
