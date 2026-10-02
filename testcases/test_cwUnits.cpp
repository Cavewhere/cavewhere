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

TEST_CASE("cwUnits::formatLength renders a length at its unit's decimals",
          "[cwUnits]") {
    // Every unit reads to about a millimeter, so the same length resolves the
    // same whichever unit it is shown in.
    CHECK(cwUnits::formatLength(60.0, cwUnits::Meters) == QStringLiteral("60.000 m"));
    CHECK(cwUnits::formatLength(60.0, cwUnits::Feet) == QStringLiteral("196.85 ft"));
    CHECK(cwUnits::formatLength(6000.0, cwUnits::Kilometers) == QStringLiteral("6.000 km"));
    CHECK(cwUnits::formatLength(6000.0, cwUnits::Miles) == QStringLiteral("3.728 mi"));

    // The sign decision acts on the rounded value, so a length below half the
    // last digit prints a clean zero.
    CHECK(cwUnits::formatLength(-0.0004, cwUnits::Meters, true) == QStringLiteral("0.000 m"));
    CHECK(cwUnits::formatLength(0.001, cwUnits::Meters, true) == QStringLiteral("+0.001 m"));
}

TEST_CASE("cwUnits::formatAngle renders degrees at the asked precision", "[cwUnits]") {
    CHECK(cwUnits::formatAngle(0.376) == QStringLiteral("0.4°"));
    CHECK(cwUnits::formatAngle(0.376, 3) == QStringLiteral("0.376°"));
    CHECK(cwUnits::formatAngle(-1.25, 1) == QStringLiteral("-1.3°"));
    CHECK(cwUnits::formatAngle(0.0, 0) == QStringLiteral("0°"));
}

TEST_CASE("cwUnits::formatAngle drops the sign on a value that prints as zero",
          "[cwUnits]") {
    // The reason this exists: a reading a hair below its reference direction —
    // a cave just west of the projection's central meridian, a level shot
    // reading fractionally down — would otherwise render a minus sign with no
    // digits to back it up, which reads as a broken value rather than a zero.
    CHECK(cwUnits::formatAngle(-0.0) == QStringLiteral("0.0°"));
    CHECK(cwUnits::formatAngle(-0.004) == QStringLiteral("0.0°"));
    CHECK(cwUnits::formatAngle(-0.0000004, 6) == QStringLiteral("0.000000°"));

    // The sign survives wherever the printed digits still carry it.
    CHECK(cwUnits::formatAngle(-0.06) == QStringLiteral("-0.1°"));
    CHECK(cwUnits::formatAngle(-0.004, 3) == QStringLiteral("-0.004°"));
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
