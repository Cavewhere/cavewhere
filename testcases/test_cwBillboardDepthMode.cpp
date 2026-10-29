// Pins the depth modes billboard layers draw with: the marker knobs map to a
// mode with the opacity as a 0-1 factor, and each mode lists its passes in draw
// order. The default mode is the one depth-tested pass every layer drew before
// the x-ray knobs; x-ray adds a hidden pass first that passes only where
// geometry is nearer (Greater), writes no depth and draws at the x-ray opacity.
// Pure, so no QRhi is needed.

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwBillboardDepthMode.h"
#include "cwBillboardDepthPass.h"

namespace {

constexpr int kDefaultOpacityPercent = 30;
constexpr float kDefaultOpacity = 0.3f;
constexpr int kHalfOpacityPercent = 50;
constexpr float kHalfOpacity = 0.5f;
constexpr int kFullOpacityPercent = 100;

using cw::billboard::DepthPass;

DepthPass visiblePass()
{
    return DepthPass{true, true, QRhiGraphicsPipeline::Less, 1.0f};
}

DepthPass overEverythingPass()
{
    return DepthPass{false, false, QRhiGraphicsPipeline::Less, 1.0f};
}

} // namespace

TEST_CASE("The marker knobs map to a depth mode with the opacity as a factor", "[cwBillboardDepthMode]")
{
    const cwBillboardDepthMode xRay = cw::billboard::markerDepthMode(true, true, kDefaultOpacityPercent);
    CHECK(xRay.depthTest);
    CHECK(xRay.xRay);
    CHECK(xRay.xRayOpacity == Catch::Approx(kDefaultOpacity));

    const cwBillboardDepthMode overEverything = cw::billboard::markerDepthMode(false, true, kFullOpacityPercent);
    CHECK_FALSE(overEverything.depthTest);
    CHECK(overEverything.xRay);
    CHECK(overEverything.xRayOpacity == Catch::Approx(1.0f));

    const cwBillboardDepthMode depthTestedOnly = cw::billboard::markerDepthMode(true, false, kHalfOpacityPercent);
    CHECK(depthTestedOnly.depthTest);
    CHECK_FALSE(depthTestedOnly.xRay);
    CHECK(depthTestedOnly.xRayOpacity == Catch::Approx(kHalfOpacity));
}

TEST_CASE("The default marker depth mode is the knobs' defaults: x-ray at 30 percent", "[cwBillboardDepthMode]")
{
    CHECK(cw::billboard::kDefaultMarkerDepthTest);
    CHECK(cw::billboard::kDefaultMarkerXRay);
    CHECK(cw::billboard::kDefaultXRayOpacityPercent == kDefaultOpacityPercent);

    const cwBillboardDepthMode mode = cw::billboard::defaultMarkerDepthMode();
    CHECK(mode.depthTest);
    CHECK(mode.xRay);
    CHECK(mode.xRayOpacity == Catch::Approx(kDefaultOpacity));

    // The layer default stays the single depth-tested pass station labels draw.
    CHECK_FALSE(mode == cwBillboardDepthMode());
}

TEST_CASE("Each depth mode lists its passes, the hidden pass first", "[cwBillboardDepthMode]")
{
    SECTION("the layer default is today's one depth-tested, depth-writing pass") {
        const QVector<DepthPass> passes = cw::billboard::depthPasses(cwBillboardDepthMode());
        REQUIRE(passes.size() == 1);
        CHECK(passes.at(0) == visiblePass());
    }

    SECTION("the depth test on without x-ray is the same one pass") {
        const QVector<DepthPass> passes =
            cw::billboard::depthPasses(cw::billboard::markerDepthMode(true, false, kDefaultOpacityPercent));
        REQUIRE(passes.size() == 1);
        CHECK(passes.at(0) == visiblePass());
    }

    SECTION("the depth test off is one pass over everything, with or without x-ray") {
        for (const bool xRay : {false, true}) {
            INFO("x-ray " << xRay);
            const QVector<DepthPass> passes =
                cw::billboard::depthPasses(cw::billboard::markerDepthMode(false, xRay, kDefaultOpacityPercent));
            REQUIRE(passes.size() == 1);
            CHECK(passes.at(0) == overEverythingPass());
        }
    }

    SECTION("x-ray draws the hidden part first: Greater, no depth write, at the x-ray opacity") {
        const QVector<DepthPass> passes =
            cw::billboard::depthPasses(cw::billboard::markerDepthMode(true, true, kDefaultOpacityPercent));
        REQUIRE(passes.size() == 2);

        const DepthPass& hidden = passes.at(0);
        CHECK(hidden.depthTest);
        CHECK_FALSE(hidden.depthWrite);
        CHECK(hidden.depthOp == QRhiGraphicsPipeline::Greater);
        CHECK(hidden.opacity == Catch::Approx(kDefaultOpacity));

        CHECK(passes.at(1) == visiblePass());
    }
}
