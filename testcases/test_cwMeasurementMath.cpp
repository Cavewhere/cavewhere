//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Qt
#include <QVector3D>

// SUT
#include "cwMeasurementMath.h"

using namespace Catch;

TEST_CASE("cwMeasurementMath::between measures distance and deltas", "[cwMeasurementMath]")
{
    const QVector3D a(10.0f, 20.0f, 30.0f);
    const QVector3D b(13.0f, 24.0f, 42.0f);

    const auto m = cwMeasurementMath::between(a, b);

    CHECK(m.deltaEast == Approx(3.0));
    CHECK(m.deltaNorth == Approx(4.0));
    CHECK(m.vertical == Approx(12.0));
    CHECK(m.horizontal == Approx(5.0));   // hypot(3, 4)
    CHECK(m.distance == Approx(13.0));    // hypot(5, 12)
}

TEST_CASE("cwMeasurementMath::between signs deltas in the third quadrant", "[cwMeasurementMath]")
{
    // South-west and downward: every delta is negative and the azimuth lands in
    // the wrapped (180,360) range, catching a swapped-argument or sign error.
    const auto m = cwMeasurementMath::between(QVector3D(0.0f, 0.0f, 0.0f),
                                              QVector3D(-10.0f, -10.0f, -5.0f));
    CHECK(m.deltaEast == Approx(-10.0));
    CHECK(m.deltaNorth == Approx(-10.0));
    CHECK(m.vertical == Approx(-5.0));
    CHECK(m.azimuth == Approx(225.0));    // SW grid bearing
    CHECK(m.inclination < 0.0);           // pointing down
}

TEST_CASE("cwMeasurementMath::between computes grid azimuth", "[cwMeasurementMath]")
{
    const QVector3D origin(0.0f, 0.0f, 0.0f);

    SECTION("due north (+Y) is azimuth 0") {
        const auto m = cwMeasurementMath::between(origin, QVector3D(0.0f, 10.0f, 0.0f));
        CHECK(m.azimuth == Approx(0.0).margin(1e-9));
    }

    SECTION("due east (+X) is azimuth 90") {
        const auto m = cwMeasurementMath::between(origin, QVector3D(10.0f, 0.0f, 0.0f));
        CHECK(m.azimuth == Approx(90.0));
    }

    SECTION("due south (-Y) is azimuth 180") {
        const auto m = cwMeasurementMath::between(origin, QVector3D(0.0f, -10.0f, 0.0f));
        CHECK(m.azimuth == Approx(180.0));
    }

    SECTION("due west (-X) wraps to azimuth 270, not -90") {
        const auto m = cwMeasurementMath::between(origin, QVector3D(-10.0f, 0.0f, 0.0f));
        CHECK(m.azimuth == Approx(270.0));
    }
}

TEST_CASE("cwMeasurementMath::between computes inclination", "[cwMeasurementMath]")
{
    const QVector3D origin(0.0f, 0.0f, 0.0f);

    SECTION("45 degrees up") {
        const auto m = cwMeasurementMath::between(origin, QVector3D(0.0f, 10.0f, 10.0f));
        CHECK(m.inclination == Approx(45.0));
    }

    SECTION("level shot is 0") {
        const auto m = cwMeasurementMath::between(origin, QVector3D(10.0f, 10.0f, 0.0f));
        CHECK(m.inclination == Approx(0.0).margin(1e-9));
    }

    SECTION("straight up is +90 with azimuth 0") {
        const auto m = cwMeasurementMath::between(origin, QVector3D(0.0f, 0.0f, 5.0f));
        CHECK(m.inclination == Approx(90.0));
        CHECK(m.azimuth == Approx(0.0).margin(1e-9));
        CHECK(m.horizontal == Approx(0.0).margin(1e-9));
    }

    SECTION("straight down is -90 with azimuth 0") {
        const auto m = cwMeasurementMath::between(origin, QVector3D(0.0f, 0.0f, -5.0f));
        CHECK(m.inclination == Approx(-90.0));
        CHECK(m.azimuth == Approx(0.0).margin(1e-9));
    }
}

TEST_CASE("cwMeasurementMath::between handles coincident points", "[cwMeasurementMath]")
{
    const QVector3D p(7.0f, -3.0f, 2.0f);
    const auto m = cwMeasurementMath::between(p, p);

    CHECK(m.distance == Approx(0.0).margin(1e-9));
    CHECK(m.horizontal == Approx(0.0).margin(1e-9));
    CHECK(m.vertical == Approx(0.0).margin(1e-9));
    CHECK(m.azimuth == Approx(0.0).margin(1e-9));
    CHECK(m.inclination == Approx(0.0).margin(1e-9));
    CHECK(m.deltaEast == Approx(0.0).margin(1e-9));
    CHECK(m.deltaNorth == Approx(0.0).margin(1e-9));
}
