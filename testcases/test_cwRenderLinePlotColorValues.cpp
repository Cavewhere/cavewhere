//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRenderLinePlot.h"

//Qt includes
#include <QVector3D>

namespace {

constexpr float kTripValue = 3.0f;

// Two shots, four vertices
QVector<QVector3D> twoShots()
{
    return {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(10.0f, 0.0f, -1.0f),
        QVector3D(10.0f, 0.0f, -1.0f),
        QVector3D(20.0f, 0.0f, -2.0f)
    };
}

} // namespace

TEST_CASE("cwRenderLinePlot stores per-vertex color values by range", "[ColorBy]")
{
    cwRenderLinePlot linePlot;
    linePlot.setGeometry(twoShots());
    CHECK(linePlot.colorValues() == QVector<float>({0.0f, 0.0f, 0.0f, 0.0f}));

    linePlot.setRangeColorValue(2, 2, kTripValue);
    CHECK(linePlot.colorValues() == QVector<float>({0.0f, 0.0f, kTripValue, kTripValue}));

    SECTION("Out-of-range spans are ignored") {
        linePlot.setRangeColorValue(-1, 2, 1.0f);
        linePlot.setRangeColorValue(2, 4, 1.0f);
        linePlot.setRangeColorValue(4, 1, 1.0f);
        linePlot.setRangeColorValue(0, 0, 1.0f);
        CHECK(linePlot.colorValues() == QVector<float>({0.0f, 0.0f, kTripValue, kTripValue}));
    }

    SECTION("New geometry resets every value to zero") {
        linePlot.setGeometry(twoShots());
        CHECK(linePlot.colorValues() == QVector<float>({0.0f, 0.0f, 0.0f, 0.0f}));
    }
}
