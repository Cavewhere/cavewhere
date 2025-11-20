//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace Catch;

//Qt includes
#include <QSignalSpy>

//Our includes
#include "WorldToScreenMatrix.h"

// Constants from WorldToScreenMatrix implementation
static constexpr double meterToMM = 1000;

TEST_CASE("Initial matrix is correct", "[WorldToScreenMatrix]") {
    WorldToScreenMatrix w2s;
    auto mtx = w2s.matrix();
    // expected uniform scale factor = meterToMM / mapScale * pixelDensity
    double expected = meterToMM / w2s.scale()->scale() * w2s.pixelDensity();

    CHECK( mtx(0,0) == Approx(expected) );
    CHECK( mtx(1,1) == Approx(-expected) );
    CHECK( mtx(2,2) == Approx(expected) );
    CHECK( mtx(3,3) == Approx(1.0) );
}

TEST_CASE("Matrix updates on DPI change and emits a single matrixChanged",
          "[WorldToScreenMatrix]")
{
    WorldToScreenMatrix w2s;
    QSignalSpy spy(&w2s, &WorldToScreenMatrix::matrixChanged);

    double newDensity = 192.0;
    w2s.setPixelDensity(newDensity);

    REQUIRE( spy.count() == 1 );

    auto mtx = w2s.matrix();
    double expected = meterToMM / w2s.scale()->scale() * newDensity;
    CHECK( mtx(0,0) == Approx(expected) );
    CHECK( mtx(1,1) == Approx(-expected) );
    CHECK( mtx(2,2) == Approx(expected) );
}

TEST_CASE("No matrixChanged when setting DPI to the same value",
          "[WorldToScreenMatrix]")
{
    WorldToScreenMatrix w2s;
    QSignalSpy spy(&w2s, &WorldToScreenMatrix::matrixChanged);

    // setting to the current pixelDensity should not re-emit
    w2s.setPixelDensity(w2s.pixelDensity());
    REQUIRE( spy.isEmpty() );
}

TEST_CASE("Matrix updates on scale change and emits a single matrixChanged",
          "[WorldToScreenMatrix]")
{
    WorldToScreenMatrix w2s;
    QSignalSpy spy(&w2s, &WorldToScreenMatrix::matrixChanged);

    auto mtx = w2s.matrix();
    double expected = meterToMM / w2s.scale()->scale() * w2s.pixelDensity();
    CHECK( mtx(0,0) == Approx(expected) );
    CHECK( mtx(1,1) == Approx(-expected) );
    CHECK( mtx(2,2) == Approx(expected) );

    double newScale = 2.0;
    w2s.scale()->setScale(newScale);

    REQUIRE( spy.count() == 1 );

    auto mtx1 = w2s.matrix();
    expected = meterToMM / newScale * w2s.pixelDensity();
    CHECK( mtx1(0,0) == Approx(expected) );
    CHECK( mtx1(1,1) == Approx(-expected) );
    CHECK( mtx1(2,2) == Approx(expected) );
    CHECK( mtx1(0,0) * 2.0 == Approx(mtx(0,0)));
    CHECK( mtx1(1,1) * 2.0 == Approx(mtx(1,1)));
    CHECK( mtx1(2,2) * 2.0 == Approx(mtx(2,2)));
}
