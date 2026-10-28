// Pins cwEntranceLabel::statsText, the line an entrance marker draws under its
// name: the cave's length and depth for an Entrance, in the project's unit
// system, and nothing for any other kind.

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwEntranceLabel.h"

//Std includes
#include <string>

namespace {
// std::string, so a failure prints both lines.
std::string stats(cwFixStation::Kind kind, double lengthMeters, double depthMeters,
                  cwUnits::UnitSystem unitSystem)
{
    return cwEntranceLabel::statsText(kind, lengthMeters, depthMeters, unitSystem).toStdString();
}

std::string utf8(const QString& text)
{
    return text.toStdString();
}
}

TEST_CASE("cwEntranceLabel gives an entrance its length and depth in metric", "[cwEntranceLabel]")
{
    SECTION("A length of one kilometer or more reads in km at one decimal") {
        CHECK(stats(cwFixStation::Entrance, 1234.0, 85.4, cwUnits::Metric)
              == utf8(QStringLiteral("→1.2 km  ↓85 m")));
    }

    SECTION("A length under one kilometer reads in whole meters") {
        CHECK(stats(cwFixStation::Entrance, 999.0, 12.6, cwUnits::Metric)
              == utf8(QStringLiteral("→999 m  ↓13 m")));
    }

    SECTION("Exactly one kilometer switches to km") {
        CHECK(stats(cwFixStation::Entrance, 1000.0, 0.0, cwUnits::Metric)
              == utf8(QStringLiteral("→1.0 km  ↓0 m")));
    }

    SECTION("Depth stays in meters however deep the cave is") {
        CHECK(stats(cwFixStation::Entrance, 25000.0, 1520.4, cwUnits::Metric)
              == utf8(QStringLiteral("→25.0 km  ↓1520 m")));
    }
}

TEST_CASE("cwEntranceLabel gives an entrance its length and depth in imperial", "[cwEntranceLabel]")
{
    SECTION("A length under one mile reads in whole feet") {
        // 1234 m is 4048.6 ft.
        CHECK(stats(cwFixStation::Entrance, 1234.0, 85.4, cwUnits::Imperial)
              == utf8(QStringLiteral("→4049 ft  ↓280 ft")));
    }

    SECTION("A length of one mile or more reads in mi at one decimal") {
        // 2000 m is 1.24 mi; 85.4 m is 280.2 ft.
        CHECK(stats(cwFixStation::Entrance, 2000.0, 85.4, cwUnits::Imperial)
              == utf8(QStringLiteral("→1.2 mi  ↓280 ft")));
    }
}

TEST_CASE("cwEntranceLabel gives every other kind an empty stats line", "[cwEntranceLabel]")
{
    // The positive control: the same numbers give an Entrance its line.
    REQUIRE_FALSE(stats(cwFixStation::Entrance, 1234.0, 85.4, cwUnits::Metric).empty());

    for (cwFixStation::Kind kind : {cwFixStation::Well, cwFixStation::RadioLocation, cwFixStation::Other}) {
        INFO("kind " << cwFixStation::kindName(kind).toStdString());
        CHECK(stats(kind, 1234.0, 85.4, cwUnits::Metric).empty());
        CHECK(stats(kind, 1234.0, 85.4, cwUnits::Imperial).empty());
    }
}
