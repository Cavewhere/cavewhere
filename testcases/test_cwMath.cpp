// Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace Catch;

// Our includes
#include "cwMath.h"

// Qt
#include <QtGlobal>

// Std
#include <cmath>
#include <limits>

TEST_CASE("cwWrapDegrees360 keeps angles within [0, 360)", "[cwMath]")
{
    SECTION("Already normalized values stay the same")
    {
        CHECK(cwWrapDegrees360(0.0) == 0.0);
        CHECK(cwWrapDegrees360(45.5) == Approx(45.5));
        CHECK(cwWrapDegrees360(359.999) == Approx(359.999));
    }

    SECTION("Positive overflow wraps around")
    {
        CHECK(cwWrapDegrees360(360.0) == 0.0);
        CHECK(cwWrapDegrees360(361.0) == Approx(1.0));
        CHECK(cwWrapDegrees360(725.25) == Approx(5.25));
    }

    SECTION("Negative angles wrap into positive range")
    {
        CHECK(cwWrapDegrees360(-1.0) == Approx(359.0));
        CHECK(cwWrapDegrees360(-90.0) == Approx(270.0));
        CHECK(cwWrapDegrees360(-721.0) == Approx(359.0));
    }

    SECTION("Non-finite numbers return zero")
    {
        CHECK(cwWrapDegrees360(std::numeric_limits<double>::infinity()) == 0.0);
        CHECK(cwWrapDegrees360(-std::numeric_limits<double>::infinity()) == 0.0);
        CHECK(cwWrapDegrees360(std::numeric_limits<double>::quiet_NaN()) == 0.0);
    }
}

TEST_CASE("cwMedian finds the middle of a list", "[cwMath]")
{
    SECTION("An odd count takes the middle value")
    {
        CHECK(cwMedian({3.0, 1.0, 2.0}) == Approx(2.0));
        CHECK(cwMedian({5.0}) == Approx(5.0));
    }

    SECTION("An even count averages the two middle values")
    {
        CHECK(cwMedian({1.0, 2.0, 3.0, 4.0}) == Approx(2.5));
        CHECK(cwMedian({-10.0, 10.0}) == Approx(0.0));
    }

    SECTION("One value far from the rest moves the middle by one place, not by its distance")
    {
        CHECK(cwMedian({1.0, 2.0, 3.0, 1000000.0}) == Approx(2.5));
    }

    SECTION("An empty list is zero")
    {
        CHECK(cwMedian({}) == 0.0);
    }

    SECTION("The caller's order is left alone")
    {
        const QList<double> values{3.0, 1.0, 2.0};
        CHECK(cwMedian(values) == Approx(2.0));
        CHECK(values == QList<double>{3.0, 1.0, 2.0});
    }
}
