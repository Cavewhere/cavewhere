//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwScale.h"
#include "cwUnits.h"

TEST_CASE("cwScale static scale matches instance scale", "[cwScale]") {
    auto checkScale = [](double numeratorValue,
                         cwUnits::LengthUnit numeratorUnit,
                         double denominatorValue,
                         cwUnits::LengthUnit denominatorUnit,
                         double expectedScale) {
        cwScale scale;
        scale.scaleNumerator()->setValue(numeratorValue);
        scale.scaleNumerator()->setUnit(numeratorUnit);
        scale.scaleDenominator()->setValue(denominatorValue);
        scale.scaleDenominator()->setUnit(denominatorUnit);

        const cwScale::Data data = scale.data();
        CHECK(scale.scale() == Catch::Approx(expectedScale).epsilon(1e-9));
        CHECK(cwScale::scale(data) == Catch::Approx(expectedScale).epsilon(1e-9));
    };

    checkScale(1.0, cwUnits::LengthUnitless, 1.0, cwUnits::LengthUnitless, 1.0);
    checkScale(1.0, cwUnits::Meters, 500.0, cwUnits::Meters, 0.002);
    checkScale(2.0, cwUnits::Centimeters, 80.0, cwUnits::Meters, 0.00025);
    checkScale(3.0, cwUnits::Feet, 12.0, cwUnits::Meters, 0.0762);

    cwScale scale;
    scale.scaleNumerator()->setUnit(cwUnits::Meters);
    scale.scaleDenominator()->setUnit(cwUnits::Meters);
    scale.setScale(500.0);
    CHECK(scale.scale() == Catch::Approx(cwScale::scale(scale.data())).epsilon(1e-9));
}
