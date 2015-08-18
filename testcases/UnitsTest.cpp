/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "catch.hpp"

//Cavewhere inculdes
#include "cwUnits.h"

//std includes
#include <limits>

TEST_CASE("Distance units can be convert between each other", "[units]") {
    CHECK(cwUnits::convert(500.0, cwUnits::Inches, cwUnits::Feet) == Approx(41.6666666667));
    CHECK(cwUnits::convert(500.0, cwUnits::Inches, cwUnits::Inches) == Approx(500.0));
    CHECK(cwUnits::convert(500.0, cwUnits::Inches, cwUnits::Yards) == Approx(13.8888888889));
    CHECK(cwUnits::convert(500.0, cwUnits::Inches, cwUnits::Meters) == Approx(12.7));
    CHECK(cwUnits::convert(500.0, cwUnits::Inches, cwUnits::Millimeters) == Approx(12700.0));
    CHECK(cwUnits::convert(500.0, cwUnits::Inches, cwUnits::Centimeters) == Approx(1270.0));
    CHECK(cwUnits::convert(500.0, cwUnits::Inches, cwUnits::Miles) == Approx(0.00789141));
    CHECK(cwUnits::convert(500.0, cwUnits::Inches, cwUnits::Kilometers) == Approx(0.0127));
}

TEST_CASE("Distance units string convertion", "[units]") {
    CHECK(cwUnits::Inches == cwUnits::toLengthUnit(cwUnits::unitName(cwUnits::Inches)));
    CHECK(cwUnits::Feet == cwUnits::toLengthUnit(cwUnits::unitName(cwUnits::Feet)));
    CHECK(cwUnits::Yards == cwUnits::toLengthUnit(cwUnits::unitName(cwUnits::Yards)));
    CHECK(cwUnits::Meters == cwUnits::toLengthUnit(cwUnits::unitName(cwUnits::Meters)));
    CHECK(cwUnits::Centimeters == cwUnits::toLengthUnit(cwUnits::unitName(cwUnits::Centimeters)));
    CHECK(cwUnits::Millimeters == cwUnits::toLengthUnit(cwUnits::unitName(cwUnits::Millimeters)));
    CHECK(cwUnits::Miles == cwUnits::toLengthUnit(cwUnits::unitName(cwUnits::Miles)));
    CHECK(cwUnits::Kilometers == cwUnits::toLengthUnit(cwUnits::unitName(cwUnits::Kilometers)));

    CHECK(cwUnits::unitName(cwUnits::Inches) == "in");
    CHECK(cwUnits::unitName(cwUnits::Feet) == "ft");
    CHECK(cwUnits::unitName(cwUnits::Yards) == "yd");
    CHECK(cwUnits::unitName(cwUnits::Meters) == "m");
    CHECK(cwUnits::unitName(cwUnits::Centimeters) == "cm");
    CHECK(cwUnits::unitName(cwUnits::Millimeters) == "mm");
    CHECK(cwUnits::unitName(cwUnits::Miles) == "mi");
    CHECK(cwUnits::unitName(cwUnits::Kilometers) == "km");

    foreach(QString unitName, cwUnits::lengthUnitNames()) {
        CHECK(unitName == cwUnits::unitName(cwUnits::toLengthUnit(unitName)));
    }
}

TEST_CASE("Angle units can be convert between each other", "[units]") {
    CHECK(cwUnits::convert(43.0, cwUnits::Degrees, cwUnits::Degrees) == Approx(43.0));
    CHECK(cwUnits::convert(43.0, cwUnits::Degrees, cwUnits::Minutes) == Approx(2580.0));
    CHECK(cwUnits::convert(43.0, cwUnits::Degrees, cwUnits::Seconds) == Approx(154800.0));
    CHECK(cwUnits::convert(43.0, cwUnits::Degrees, cwUnits::Gradians) == Approx(47.777777778));
    CHECK(cwUnits::convert(43.0, cwUnits::Degrees, cwUnits::Mils) == Approx(764.444444444));
}

TEST_CASE("Angle units string convertion", "[units]") {
    CHECK(cwUnits::Degrees == cwUnits::toAngleUnit(cwUnits::unitName(cwUnits::Degrees)));
    CHECK(cwUnits::Minutes == cwUnits::toAngleUnit(cwUnits::unitName(cwUnits::Minutes)));
    CHECK(cwUnits::Seconds == cwUnits::toAngleUnit(cwUnits::unitName(cwUnits::Seconds)));
    CHECK(cwUnits::Gradians == cwUnits::toAngleUnit(cwUnits::unitName(cwUnits::Gradians)));
    CHECK(cwUnits::Mils == cwUnits::toAngleUnit(cwUnits::unitName(cwUnits::Mils)));

    CHECK(cwUnits::unitName(cwUnits::Degrees) == "deg");
    CHECK(cwUnits::unitName(cwUnits::Minutes) == "min");
    CHECK(cwUnits::unitName(cwUnits::Seconds) == "sec");
    CHECK(cwUnits::unitName(cwUnits::Gradians) == "grad");
    CHECK(cwUnits::unitName(cwUnits::Mils) == "mils");

    foreach(QString unitName, cwUnits::angleUnitNames()) {
        CHECK(unitName == cwUnits::unitName(cwUnits::toAngleUnit(unitName)));
    }
}

TEST_CASE("Vertical Angle units can be convert between each other", "[units]") {
    CHECK(cwUnits::convert(90.0, cwUnits::Degrees, cwUnits::PercentGrade) == Approx(1633123935319536896.0));
    CHECK(cwUnits::convert(-90.0, cwUnits::Degrees, cwUnits::PercentGrade) == Approx(-1633123935319536896.0));
    CHECK(cwUnits::convert(45.0, cwUnits::Degrees, cwUnits::PercentGrade) == Approx(100.0));
    CHECK(cwUnits::convert(0.0, cwUnits::Degrees, cwUnits::PercentGrade) == Approx(0.0));
    CHECK(cwUnits::convert(-63.2, cwUnits::Degrees, cwUnits::PercentGrade) == Approx(-197.9663518141));
    CHECK(cwUnits::convert(63.0, cwUnits::PercentGrade, cwUnits::PercentGrade) == Approx(63.0));
    CHECK(cwUnits::convert(0.0, cwUnits::PercentGrade, cwUnits::Degrees) == Approx(0.0));
    CHECK(cwUnits::convert(300.0, cwUnits::PercentGrade, cwUnits::Degrees) == Approx(71.5650511771));
    CHECK(cwUnits::convert(100.0, cwUnits::PercentGrade, cwUnits::Degrees) == Approx(45.0));
    CHECK(cwUnits::convert(std::numeric_limits<double>::infinity(), cwUnits::PercentGrade, cwUnits::Degrees) == Approx(90.0));
    CHECK(cwUnits::convert(-std::numeric_limits<double>::infinity(), cwUnits::PercentGrade, cwUnits::Degrees) == Approx(-90.0));

    CHECK(cwUnits::convert(43.0, cwUnits::Minutes, cwUnits::PercentGrade) == Approx(1.2508845336));
    CHECK(cwUnits::convert(43.0, cwUnits::Seconds, cwUnits::PercentGrade) == Approx(0.0208469886));
    CHECK(cwUnits::convert(43.0, cwUnits::Gradians, cwUnits::PercentGrade) == Approx(80.1151070559));
    CHECK(cwUnits::convert(43.0, cwUnits::Mils, cwUnits::PercentGrade) == Approx(4.2240246646));
}

TEST_CASE("Image Resolution units can be convert between each other", "[units]") {
    CHECK(cwUnits::convert(200.0, cwUnits::DotsPerInch, cwUnits::DotsPerInch) == Approx(200.0));
    CHECK(cwUnits::convert(200.0, cwUnits::DotsPerInch, cwUnits::DotsPerCentimeter) == Approx(78.7401574803));
    CHECK(cwUnits::convert(200.0, cwUnits::DotsPerInch, cwUnits::DotsPerMeter) == Approx(7874.01574803));
}

TEST_CASE("Image Resolution units string convertion", "[units]") {
    CHECK(cwUnits::DotsPerInch == cwUnits::toImageResolutionUnit(cwUnits::unitName(cwUnits::DotsPerInch)));
    CHECK(cwUnits::DotsPerCentimeter == cwUnits::toImageResolutionUnit(cwUnits::unitName(cwUnits::DotsPerCentimeter)));
    CHECK(cwUnits::DotsPerMeter == cwUnits::toImageResolutionUnit(cwUnits::unitName(cwUnits::DotsPerMeter)));

    CHECK(cwUnits::unitName(cwUnits::DotsPerInch) == "Dots per inch");
    CHECK(cwUnits::unitName(cwUnits::DotsPerCentimeter) == "Dots per centimeter");
    CHECK(cwUnits::unitName(cwUnits::DotsPerMeter) == "Dots per meter");

    foreach(QString unitName, cwUnits::imageResolutionUnitNames()) {
        CHECK(unitName == cwUnits::unitName(cwUnits::toImageResolutionUnit(unitName)));
    }
}

