// Pins cwEntranceLabel::text, the one line an entrance marker draws above its
// pin: the name, then the cave's length and depth for an Entrance, in the
// project's unit system.

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwEntranceLabel.h"

//Std includes
#include <string>

namespace {
const QString kName = QStringLiteral("Gypsum Hollow");

// std::string, so a failure prints both lines.
std::string label(cwFixStation::Kind kind, double lengthMeters, double depthMeters,
                  cwUnits::UnitSystem unitSystem)
{
    return cwEntranceLabel::text(kName, kind, lengthMeters, depthMeters, unitSystem).toStdString();
}

std::string utf8(const QString& text)
{
    return text.toStdString();
}
}

TEST_CASE("cwEntranceLabel gives an entrance its length and depth in metric", "[cwEntranceLabel]")
{
    SECTION("A length of one kilometer or more reads in km at one decimal") {
        CHECK(label(cwFixStation::Entrance, 1234.0, 85.4, cwUnits::Metric)
              == utf8(QStringLiteral("Gypsum Hollow  \u21921.2 km  \u219385 m")));
    }

    SECTION("A length under one kilometer reads in whole meters") {
        CHECK(label(cwFixStation::Entrance, 999.0, 12.6, cwUnits::Metric)
              == utf8(QStringLiteral("Gypsum Hollow  \u2192999 m  \u219313 m")));
    }

    SECTION("Exactly one kilometer switches to km") {
        CHECK(label(cwFixStation::Entrance, 1000.0, 0.0, cwUnits::Metric)
              == utf8(QStringLiteral("Gypsum Hollow  \u21921.0 km  \u21930 m")));
    }

    SECTION("Depth stays in meters however deep the cave is") {
        CHECK(label(cwFixStation::Entrance, 25000.0, 1520.4, cwUnits::Metric)
              == utf8(QStringLiteral("Gypsum Hollow  \u219225.0 km  \u21931520 m")));
    }
}

TEST_CASE("cwEntranceLabel gives an entrance its length and depth in imperial", "[cwEntranceLabel]")
{
    SECTION("A length under one mile reads in whole feet") {
        // 1234 m is 4048.6 ft.
        CHECK(label(cwFixStation::Entrance, 1234.0, 85.4, cwUnits::Imperial)
              == utf8(QStringLiteral("Gypsum Hollow  \u21924049 ft  \u2193280 ft")));
    }

    SECTION("A length of one mile or more reads in mi at one decimal") {
        // 2000 m is 1.24 mi; 85.4 m is 280.2 ft.
        CHECK(label(cwFixStation::Entrance, 2000.0, 85.4, cwUnits::Imperial)
              == utf8(QStringLiteral("Gypsum Hollow  \u21921.2 mi  \u2193280 ft")));
    }
}

TEST_CASE("cwEntranceLabel gives every other kind its name only", "[cwEntranceLabel]")
{
    for (cwFixStation::Kind kind : {cwFixStation::Well, cwFixStation::RadioLocation, cwFixStation::Other}) {
        INFO("kind " << cwFixStation::kindName(kind).toStdString());
        CHECK(label(kind, 1234.0, 85.4, cwUnits::Metric) == utf8(kName));
        CHECK(label(kind, 1234.0, 85.4, cwUnits::Imperial) == utf8(kName));
    }
}
