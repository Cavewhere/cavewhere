//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace Catch;

//Our includes
#include "cwUnits.h"

TEST_CASE("cwUnits length conversions are consistent", "[cwUnits]") {
    const double value = 123.456;
    const cwUnits::LengthUnit units[] = {
        cwUnits::Inches,
        cwUnits::Feet,
        cwUnits::Yards,
        cwUnits::Meters,
        cwUnits::Millimeters,
        cwUnits::Centimeters,
        cwUnits::Kilometers,
        cwUnits::Miles
    };

    for (const auto from : units) {
        const double toMeters = cwUnits::convert(value, from, cwUnits::Meters);
        const double back = cwUnits::convert(toMeters, cwUnits::Meters, from);
        CHECK(back == Approx(value));
    }
}

TEST_CASE("cwUnits length unit names and parsing", "[cwUnits]") {
    CHECK(cwUnits::unitName(cwUnits::Inches) == "in");
    CHECK(cwUnits::unitName(cwUnits::Feet) == "ft");
    CHECK(cwUnits::unitName(cwUnits::Yards) == "yd");
    CHECK(cwUnits::unitName(cwUnits::Meters) == "m");
    CHECK(cwUnits::unitName(cwUnits::Millimeters) == "mm");
    CHECK(cwUnits::unitName(cwUnits::Centimeters) == "cm");
    CHECK(cwUnits::unitName(cwUnits::Kilometers) == "km");
    CHECK(cwUnits::unitName(cwUnits::Miles) == "mi");

    CHECK(cwUnits::toLengthUnit("ft") == cwUnits::Feet);
    CHECK(cwUnits::toLengthUnit("feet") == cwUnits::Feet);
    CHECK(cwUnits::toLengthUnit("metric") == cwUnits::Meters);
    CHECK(cwUnits::toLengthUnit("meters") == cwUnits::Meters);
    CHECK(cwUnits::toLengthUnit("metres") == cwUnits::Meters);
    CHECK(cwUnits::toLengthUnit("m") == cwUnits::Meters);
    CHECK(cwUnits::toLengthUnit("yards") == cwUnits::Yards);
    CHECK(cwUnits::toLengthUnit("yd") == cwUnits::Yards);
    CHECK(cwUnits::toLengthUnit("in") == cwUnits::Inches);
    CHECK(cwUnits::toLengthUnit("inches") == cwUnits::Inches);
    CHECK(cwUnits::toLengthUnit("inch") == cwUnits::Inches);
    CHECK(cwUnits::toLengthUnit("mm") == cwUnits::Millimeters);
    CHECK(cwUnits::toLengthUnit("cm") == cwUnits::Centimeters);
    CHECK(cwUnits::toLengthUnit("km") == cwUnits::Kilometers);
    CHECK(cwUnits::toLengthUnit("mi") == cwUnits::Miles);
    CHECK(cwUnits::toLengthUnit("unknown") == cwUnits::LengthUnitless);

    const QStringList names = cwUnits::lengthUnitNames();
    CHECK(names.contains("in"));
    CHECK(names.contains("ft"));
    CHECK(names.contains("yd"));
    CHECK(names.contains("m"));
    CHECK(names.contains("mm"));
    CHECK(names.contains("cm"));
    CHECK(names.contains("km"));
    CHECK(names.contains("mi"));
}

TEST_CASE("cwUnits image resolution conversions are consistent", "[cwUnits]") {
    const double value = 300.0;
    const cwUnits::ImageResolutionUnit units[] = {
        cwUnits::DotsPerInch,
        cwUnits::DotsPerCentimeter,
        cwUnits::DotsPerMeter
    };

    for (const auto from : units) {
        const double toDpm = cwUnits::convert(value, from, cwUnits::DotsPerMeter);
        const double back = cwUnits::convert(toDpm, cwUnits::DotsPerMeter, from);
        CHECK(back == Approx(value));
    }
}

TEST_CASE("cwUnits image resolution names and parsing", "[cwUnits]") {
    CHECK(cwUnits::unitName(cwUnits::DotsPerInch) == "Dots per inch");
    CHECK(cwUnits::unitName(cwUnits::DotsPerCentimeter) == "Dots per centimeter");
    CHECK(cwUnits::unitName(cwUnits::DotsPerMeter) == "Dots per meter");

    CHECK(cwUnits::toImageResolutionUnit("Dots per inch") == cwUnits::DotsPerInch);
    CHECK(cwUnits::toImageResolutionUnit("Dots per centimeter") == cwUnits::DotsPerCentimeter);
    CHECK(cwUnits::toImageResolutionUnit("Dots per meter") == cwUnits::DotsPerMeter);
    CHECK(cwUnits::toImageResolutionUnit("Unknown") == cwUnits::DotsPerInch);

    const QStringList names = cwUnits::imageResolutionUnitNames();
    CHECK(names.contains("Dots per inch"));
    CHECK(names.contains("Dots per centimeter"));
    CHECK(names.contains("Dots per meter"));
}
