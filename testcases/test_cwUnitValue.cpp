//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwImageResolution.h"
#include "cwUnitValue.h"
#include "cwUnits.h"

//Qt includes
#include <QObject>
#include <QSignalSpy>

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

TEST_CASE("cwUnitValue setData applies unit and value atomically before emitting", "[cwUnitValue]") {
    //Regression: setData() used to call setUnit() then setValue() in sequence,
    //so a slot connected to unitChanged() observed a (new unit, old value)
    //pair. cwNote recomputes every scrap's note transform on unitChanged(), and
    //during that window cwNote::dotPerMeter() was wrong by the
    //DotsPerInch<->DotsPerMeter factor (39.37), poisoning the scrap scale and
    //blowing up cwTriangulateTask::createPointGrid into a multi-million-cell
    //grid. setData() must expose only the final, consistent state.
    cwImageResolution resolution;
    resolution.setUnit(cwUnits::DotsPerInch);
    resolution.setValue(72.0);
    resolution.setUpdateValue(false);

    //Record the unit/value visible to a handler at the instant each signal fires.
    int unitSeenOnUnitChanged = -1;
    double valueSeenOnUnitChanged = -1.0;
    QObject::connect(&resolution, &cwUnitValue::unitChanged, &resolution, [&]() {
        unitSeenOnUnitChanged = resolution.unit();
        valueSeenOnUnitChanged = resolution.value();
    });

    int unitSeenOnValueChanged = -1;
    double valueSeenOnValueChanged = -1.0;
    QObject::connect(&resolution, &cwUnitValue::valueChanged, &resolution, [&]() {
        unitSeenOnValueChanged = resolution.unit();
        valueSeenOnValueChanged = resolution.value();
    });

    QSignalSpy unitSpy(&resolution, &cwUnitValue::unitChanged);
    QSignalSpy valueSpy(&resolution, &cwUnitValue::valueChanged);

    cwImageResolution::Data data;
    data.unit = cwUnits::DotsPerMeter;
    data.value = 5000.0;
    data.updateValueWhenUnitChanged = false;

    resolution.setData(data);

    //Both fields changed, so each signal fires exactly once.
    CHECK(unitSpy.count() == 1);
    CHECK(valueSpy.count() == 1);

    //The core guarantee: when unitChanged() fired, the value was ALREADY the new
    //value (5000.0), not the stale 72.0. Likewise when valueChanged() fired the
    //unit was already DotsPerMeter. No observer ever sees a mismatched pair.
    CHECK(unitSeenOnUnitChanged == cwUnits::DotsPerMeter);
    CHECK(valueSeenOnUnitChanged == Catch::Approx(5000.0).epsilon(1e-6));
    CHECK(unitSeenOnValueChanged == cwUnits::DotsPerMeter);
    CHECK(valueSeenOnValueChanged == Catch::Approx(5000.0).epsilon(1e-6));

    //Final state is consistent.
    CHECK(resolution.unit() == cwUnits::DotsPerMeter);
    CHECK(resolution.value() == Catch::Approx(5000.0).epsilon(1e-6));
}

TEST_CASE("cwUnitValue setData emits valueChanged but not unitChanged when only value changes", "[cwUnitValue]") {
    cwImageResolution resolution;
    resolution.setUnit(cwUnits::DotsPerMeter);
    resolution.setValue(1000.0);
    resolution.setUpdateValue(false);

    QSignalSpy unitSpy(&resolution, &cwUnitValue::unitChanged);
    QSignalSpy valueSpy(&resolution, &cwUnitValue::valueChanged);

    cwImageResolution::Data data;
    data.unit = cwUnits::DotsPerMeter; //same unit
    data.value = 2000.0;               //changed value
    data.updateValueWhenUnitChanged = false;

    resolution.setData(data);

    CHECK(unitSpy.count() == 0);
    CHECK(valueSpy.count() == 1);
    CHECK(resolution.value() == Catch::Approx(2000.0).epsilon(1e-6));
}

TEST_CASE("cwUnitValue setData emits no signals when nothing changes", "[cwUnitValue]") {
    cwImageResolution resolution;
    resolution.setUnit(cwUnits::DotsPerMeter);
    resolution.setValue(1000.0);
    resolution.setUpdateValue(false);

    QSignalSpy unitSpy(&resolution, &cwUnitValue::unitChanged);
    QSignalSpy valueSpy(&resolution, &cwUnitValue::valueChanged);

    cwImageResolution::Data data;
    data.unit = cwUnits::DotsPerMeter;
    data.value = 1000.0;
    data.updateValueWhenUnitChanged = false;

    resolution.setData(data);

    CHECK(unitSpy.count() == 0);
    CHECK(valueSpy.count() == 0);
}
