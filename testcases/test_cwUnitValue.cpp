//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwImageResolution.h"
#include "cwUnits.h"

TEST_CASE("cwUnitValue setData preserves value and unit when auto-update is enabled", "[cwUnitValue]") {
    cwImageResolution resolution;
    resolution.setUnit(cwUnits::DotsPerInch);
    resolution.setValue(72.0);
    resolution.setUpdateValue(true);

    cwImageResolution::Data data;
    data.unit = cwUnits::DotsPerMeter;
    data.value = 1234.0;
    data.updateValueWhenUnitChanged = true;

    resolution.setData(data);

    CHECK(resolution.unit() == data.unit);
    CHECK(resolution.value() == Catch::Approx(data.value).epsilon(1e-6));
    CHECK(resolution.isUpdatingValue() == true);
}
