//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QSignalSpy>

//Our includes
#include "cwWorldToScreenMatrix.h"
#include "cwScale.h"

using Catch::Approx;

// Matrix diag element = scale * meterToMM * pixelDensity.
// Derivation: cwScale::scale() is numerator/denominator (e.g. 1:250 = 0.004
// paper-m-per-cave-m). Multiply by 1000 mm/m → mm-of-paper per cave-meter;
// multiply by pixelDensity (pixels/mm of paper) → pixels per cave-meter.
static constexpr double meterToMM = 1000.0;

TEST_CASE("cwWorldToScreenMatrix initial matrix", "[cwWorldToScreenMatrix]") {
    cwWorldToScreenMatrix w2s;
    const auto mtx = w2s.matrix();
    const double expected = w2s.scale()->scale() * meterToMM * w2s.pixelDensity();

    CHECK(mtx(0, 0) == Approx(expected));
    CHECK(mtx(1, 1) == Approx(-expected));
    CHECK(mtx(2, 2) == Approx(expected));
    CHECK(mtx(3, 3) == Approx(1.0));
}

TEST_CASE("cwWorldToScreenMatrix emits matrixChanged on DPI change", "[cwWorldToScreenMatrix]") {
    cwWorldToScreenMatrix w2s;
    QSignalSpy spy(&w2s, &cwWorldToScreenMatrix::matrixChanged);

    const double newDensity = 192.0;
    w2s.setPixelDensity(newDensity);

    REQUIRE(spy.count() == 1);

    const auto mtx = w2s.matrix();
    const double expected = w2s.scale()->scale() * meterToMM * newDensity;
    CHECK(mtx(0, 0) == Approx(expected));
    CHECK(mtx(1, 1) == Approx(-expected));
    CHECK(mtx(2, 2) == Approx(expected));
}

TEST_CASE("cwWorldToScreenMatrix does not emit matrixChanged on no-op DPI", "[cwWorldToScreenMatrix]") {
    cwWorldToScreenMatrix w2s;
    QSignalSpy spy(&w2s, &cwWorldToScreenMatrix::matrixChanged);

    w2s.setPixelDensity(w2s.pixelDensity());
    REQUIRE(spy.isEmpty());
}

TEST_CASE("cwWorldToScreenMatrix emits matrixChanged on scale change", "[cwWorldToScreenMatrix]") {
    cwWorldToScreenMatrix w2s;
    QSignalSpy spy(&w2s, &cwWorldToScreenMatrix::matrixChanged);

    const auto mtx = w2s.matrix();
    double expected = w2s.scale()->scale() * meterToMM * w2s.pixelDensity();
    CHECK(mtx(0, 0) == Approx(expected));
    CHECK(mtx(1, 1) == Approx(-expected));
    CHECK(mtx(2, 2) == Approx(expected));

    const double newScale = 2.0;
    w2s.scale()->setScale(newScale);

    REQUIRE(spy.count() == 1);

    const auto mtx1 = w2s.matrix();
    expected = newScale * meterToMM * w2s.pixelDensity();
    CHECK(mtx1(0, 0) == Approx(expected));
    CHECK(mtx1(1, 1) == Approx(-expected));
    CHECK(mtx1(2, 2) == Approx(expected));
    // Doubling the scale doubles the matrix diag (not halves).
    CHECK(mtx1(0, 0) == Approx(mtx(0, 0) * 2.0));
    CHECK(mtx1(1, 1) == Approx(mtx(1, 1) * 2.0));
    CHECK(mtx1(2, 2) == Approx(mtx(2, 2) * 2.0));
}
