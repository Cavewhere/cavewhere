//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QMatrix4x4>

//Our includes
#include "cwInfiniteGridModel.h"
#include "cwFixedGridModel.h"
#include "cwLength.h"

using Catch::Approx;

// zoomLevel = log10(viewScale/mapScale * minorGridMinPixelInterval/10) + 2
// and is clamped into [0, maxZoomLevel]. A viewScale of 0.01 with identity
// mapMatrix and the default minorGridMinPixelInterval=10 puts us at level 0.
// Each 10× increase in viewScale bumps the zoom level by one decade, and
// each grid's displayed interval is the base interval multiplied by
// 10^clampedZoomLevel.

TEST_CASE("cwInfiniteGridModel defaults", "[cwInfiniteGridModel]") {
    cwInfiniteGridModel model;

    CHECK(model.majorGridInterval() == Approx(5.0));
    CHECK(model.minorGridInterval() == Approx(1.0));
    CHECK(model.maxZoomLevel() == 5);
    CHECK(model.minorGridMinPixelInterval() == Approx(10.0));
    CHECK(model.viewScale() == Approx(1.0));
    CHECK(model.majorGridModel() != nullptr);
    CHECK(model.minorGridModel() != nullptr);
    CHECK(model.majorTextModel() != nullptr);
    CHECK(model.minorTextModel() != nullptr);
}

TEST_CASE("cwInfiniteGridModel zoom level 0 matches base intervals",
          "[cwInfiniteGridModel]") {
    cwInfiniteGridModel model;
    QMatrix4x4 identity;
    model.setMapMatrix(identity);
    model.setViewScale(0.01); // Drives clampedZoomLevel to 0.

    CHECK(model.majorGridModel()->gridInterval()->value() == Approx(5.0));
    CHECK(model.minorGridModel()->gridInterval()->value() == Approx(1.0));
}

TEST_CASE("cwInfiniteGridModel scales intervals by powers of ten per zoom level",
          "[cwInfiniteGridModel]") {
    cwInfiniteGridModel model;
    QMatrix4x4 identity;
    model.setMapMatrix(identity);

    // Level 0.
    model.setViewScale(0.01);
    CHECK(model.majorGridModel()->gridInterval()->value() == Approx(5.0));
    CHECK(model.minorGridModel()->gridInterval()->value() == Approx(1.0));

    // Level 1.
    model.setViewScale(0.1);
    CHECK(model.majorGridModel()->gridInterval()->value() == Approx(50.0));
    CHECK(model.minorGridModel()->gridInterval()->value() == Approx(10.0));

    // Level 2.
    model.setViewScale(1.0);
    CHECK(model.majorGridModel()->gridInterval()->value() == Approx(500.0));
    CHECK(model.minorGridModel()->gridInterval()->value() == Approx(100.0));
}

TEST_CASE("cwInfiniteGridModel clamps zoom level to maxZoomLevel",
          "[cwInfiniteGridModel]") {
    cwInfiniteGridModel model;
    QMatrix4x4 identity;
    model.setMapMatrix(identity);
    model.setMaxZoomLevel(2);

    // Well past the cap — intervals should freeze at level 2.
    model.setViewScale(1e6);

    CHECK(model.majorGridModel()->gridInterval()->value() == Approx(500.0));
    CHECK(model.minorGridModel()->gridInterval()->value() == Approx(100.0));
}

TEST_CASE("cwInfiniteGridModel hides the major interval in the minor grid",
          "[cwInfiniteGridModel]") {
    // Minor grid must not redraw the lines that the major grid already covers.
    cwInfiniteGridModel model;
    QMatrix4x4 identity;
    model.setMapMatrix(identity);
    model.setViewScale(0.01);

    CHECK(model.minorGridModel()->hiddenInterval()
          == Approx(model.majorGridModel()->gridInterval()->value()));
}
