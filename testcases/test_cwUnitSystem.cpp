/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QStringList>

//Our includes
#include "cwUnits.h"

// The derivation helpers are constexpr — prove it holds at compile time so a
// future change that drops constexpr is caught here, not just at runtime.
static_assert(cwUnits::surveyUnit(cwUnits::Metric) == cwUnits::Meters);
static_assert(cwUnits::surveyUnit(cwUnits::Imperial) == cwUnits::Feet);
static_assert(cwUnits::largeLengthUnit(cwUnits::Metric) == cwUnits::Kilometers);
static_assert(cwUnits::magnitudeUnit(999.0, cwUnits::Metric) == cwUnits::Meters);
static_assert(cwUnits::magnitudeUnit(1000.0, cwUnits::Metric) == cwUnits::Kilometers);

TEST_CASE("cwUnits derives concrete units from a UnitSystem", "[UnitSystem]")
{
    SECTION("Survey and depth units are metres or feet") {
        CHECK(cwUnits::surveyUnit(cwUnits::Metric) == cwUnits::Meters);
        CHECK(cwUnits::surveyUnit(cwUnits::Imperial) == cwUnits::Feet);
        CHECK(cwUnits::depthUnit(cwUnits::Metric) == cwUnits::Meters);
        CHECK(cwUnits::depthUnit(cwUnits::Imperial) == cwUnits::Feet);
    }

    SECTION("Large-distance units are kilometres or miles") {
        CHECK(cwUnits::largeLengthUnit(cwUnits::Metric) == cwUnits::Kilometers);
        CHECK(cwUnits::largeLengthUnit(cwUnits::Imperial) == cwUnits::Miles);
    }
}

TEST_CASE("cwUnits::magnitudeUnit crosses over at one large unit", "[UnitSystem]")
{
    SECTION("Metric crosses over at 1 km") {
        CHECK(cwUnits::magnitudeUnit(0.0, cwUnits::Metric) == cwUnits::Meters);
        CHECK(cwUnits::magnitudeUnit(999.0, cwUnits::Metric) == cwUnits::Meters);
        CHECK(cwUnits::magnitudeUnit(1000.0, cwUnits::Metric) == cwUnits::Kilometers);
        CHECK(cwUnits::magnitudeUnit(1001.0, cwUnits::Metric) == cwUnits::Kilometers);
    }

    SECTION("Imperial crosses over at 1 mi (~1609.34 m)") {
        CHECK(cwUnits::magnitudeUnit(1600.0, cwUnits::Imperial) == cwUnits::Feet);
        CHECK(cwUnits::magnitudeUnit(1609.34, cwUnits::Imperial) == cwUnits::Miles);
        CHECK(cwUnits::magnitudeUnit(1700.0, cwUnits::Imperial) == cwUnits::Miles);
    }

    SECTION("Negative magnitudes use the absolute distance") {
        CHECK(cwUnits::magnitudeUnit(-999.0, cwUnits::Metric) == cwUnits::Meters);
        CHECK(cwUnits::magnitudeUnit(-1000.0, cwUnits::Metric) == cwUnits::Kilometers);
    }
}

TEST_CASE("cwUnits names unit systems for the UI", "[UnitSystem]")
{
    CHECK(cwUnits::unitName(cwUnits::Metric) == QStringLiteral("Metric"));
    CHECK(cwUnits::unitName(cwUnits::Imperial) == QStringLiteral("Imperial"));

    const QStringList names = cwUnits::unitSystemNames();
    REQUIRE(names.size() == 2);
    CHECK(names.at(0) == QStringLiteral("Metric"));
    CHECK(names.at(1) == QStringLiteral("Imperial"));
}
