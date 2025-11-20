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
