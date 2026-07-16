//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// SUT
#include "cwInteraction.h"

using namespace Catch;

namespace {
// 96 DPI fallback expressed as pixels per millimeter, matching cwInteraction.cpp.
constexpr double kFallbackPixelsPerMm = 96.0 / 25.4;
}

TEST_CASE("cwInteraction::pixelsPerMillimeter divides logical width by physical width",
          "[cwInteraction]")
{
    // 1000 logical px across 250 mm of glass = 4 px/mm.
    CHECK(cwInteraction::pixelsPerMillimeter(1000.0, 250.0) == Approx(4.0));

    // A typical retina laptop in its default scaled mode (1512 logical px across
    // a ~302 mm panel) gives ~5 px/mm, so the 4 mm pivot tolerance lands near the
    // 20 px it replaced — the feel is preserved where the radius was tuned.
    const double pxPerMm = cwInteraction::pixelsPerMillimeter(1512.0, 302.0);
    CHECK(pxPerMm == Approx(5.007).margin(0.01));
    CHECK(4.0 * pxPerMm == Approx(20.0).margin(0.1));
}

TEST_CASE("cwInteraction::pixelsPerMillimeter falls back to 96 DPI for an unknown physical size",
          "[cwInteraction]")
{
    // A zero / missing physical size (bad EDID, headless) must not divide by zero
    // or collapse the tolerance to nothing — fall back to a fixed 96 DPI.
    CHECK(cwInteraction::pixelsPerMillimeter(1920.0, 0.0) == Approx(kFallbackPixelsPerMm));
    CHECK(cwInteraction::pixelsPerMillimeter(0.0, 250.0) == Approx(kFallbackPixelsPerMm));
    CHECK(cwInteraction::pixelsPerMillimeter(-5.0, 250.0) == Approx(kFallbackPixelsPerMm));
    CHECK(cwInteraction::pixelsPerMillimeter(1000.0, -1.0) == Approx(kFallbackPixelsPerMm));
}
