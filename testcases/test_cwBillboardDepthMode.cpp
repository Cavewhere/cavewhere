// Pins the depth modes billboard layers draw with: the marker knobs map to a
// mode with the opacity as a 0-1 factor, and each mode lists its passes in draw
// order. The default mode is the one depth-tested pass every layer drew before
// the x-ray knobs; x-ray adds a hidden pass first that passes only where
// geometry is nearer (Greater), writes no depth and draws at the x-ray opacity.
// That opacity ramps with the camera tilt read from the view matrix, from the
// base knob to the overhead knob between the two ramp tilts.
// Pure, so no QRhi is needed.

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwBillboardDepthMode.h"
#include "cwBillboardDepthPass.h"

namespace {

constexpr int kDefaultOpacityPercent = 35;
constexpr float kDefaultOpacity = 0.35f;
constexpr int kHalfOpacityPercent = 50;
constexpr float kHalfOpacity = 0.5f;
constexpr int kFullOpacityPercent = 100;

// The ramp knobs' defaults: ghosted at 35 percent up to a 60 degree tilt, then
// rising to full opacity at an 85 degree tilt.
constexpr int kRampStartTilt = 60;
constexpr int kRampEndTilt = 85;
constexpr double kStraightDownTilt = 90.0;
constexpr double kLevelTilt = 0.0;
constexpr double kStraightUpTilt = -90.0;
constexpr double kMidRampTilt = 72.5;
constexpr float kMidRampOpacity = 0.675f;
constexpr double kHalfwayDownTilt = 45.0;
constexpr float kOverheadOpacity = 1.0f;

// How far below a ramp edge the step tests look.
constexpr double kJustBelow = 0.1;

constexpr float kOpacityTolerance = 1e-5f;
constexpr double kTiltToleranceDegrees = 1e-3;

float defaultRamp(double tiltDegrees)
{
    return cw::billboard::rampedXRayOpacity(tiltDegrees, kDefaultOpacityPercent, kFullOpacityPercent,
                                            kRampStartTilt, kRampEndTilt);
}

// A view matrix whose rotation is the turntable's at @a pitchDegrees, which is
// also its tilt: the identity is pitch 90, the plan view.
QMatrix4x4 pitchedView(double pitchDegrees)
{
    QMatrix4x4 view;
    view.rotate(float(pitchDegrees - kStraightDownTilt), 1.0f, 0.0f, 0.0f);
    return view;
}

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

TEST_CASE("The default marker depth mode is the knobs' defaults: x-ray at 35 percent", "[cwBillboardDepthMode]")
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


TEST_CASE("The x-ray ghost ramps from the base opacity to the overhead opacity as the camera tilts down",
          "[cwBillboardDepthMode]")
{
    CHECK(cw::billboard::kDefaultXRayRampStartTiltDegrees == kRampStartTilt);
    CHECK(cw::billboard::kDefaultXRayRampEndTiltDegrees == kRampEndTilt);
    CHECK(cw::billboard::kDefaultXRayOverheadOpacityPercent == kFullOpacityPercent);

    CHECK(defaultRamp(kStraightDownTilt) == Catch::Approx(kOverheadOpacity).margin(kOpacityTolerance));
    CHECK(defaultRamp(kMidRampTilt) == Catch::Approx(kMidRampOpacity).margin(kOpacityTolerance));
    CHECK(defaultRamp(kRampStartTilt) == Catch::Approx(kDefaultOpacity).margin(kOpacityTolerance));
    CHECK(defaultRamp(kHalfwayDownTilt) == Catch::Approx(kDefaultOpacity).margin(kOpacityTolerance));
    CHECK(defaultRamp(kLevelTilt) == Catch::Approx(kDefaultOpacity).margin(kOpacityTolerance));
    CHECK(defaultRamp(kStraightUpTilt) == Catch::Approx(kDefaultOpacity).margin(kOpacityTolerance));

    SECTION("an overhead opacity below the base ramps down") {
        constexpr int kLowOverheadPercent = 10;
        constexpr float kLowOverhead = 0.1f;
        constexpr float kMidwayToLow = 0.225f;
        CHECK(cw::billboard::rampedXRayOpacity(kStraightDownTilt, kDefaultOpacityPercent, kLowOverheadPercent,
                                               kRampStartTilt, kRampEndTilt)
              == Catch::Approx(kLowOverhead).margin(kOpacityTolerance));
        CHECK(cw::billboard::rampedXRayOpacity(kMidRampTilt, kDefaultOpacityPercent, kLowOverheadPercent,
                                               kRampStartTilt, kRampEndTilt)
              == Catch::Approx(kMidwayToLow).margin(kOpacityTolerance));
    }
}

TEST_CASE("The x-ray ramp steps from base to overhead at its end when the start is at or past the end",
          "[cwBillboardDepthMode]")
{
    SECTION("the start past the end") {
        constexpr int kLateStart = 80;
        constexpr int kEarlyEnd = 40;
        constexpr double kBetweenEndAndStart = 60.0;
        const auto ramp = [](double tilt) {
            return cw::billboard::rampedXRayOpacity(tilt, kDefaultOpacityPercent, kHalfOpacityPercent,
                                                    kLateStart, kEarlyEnd);
        };
        CHECK(ramp(kEarlyEnd - kJustBelow) == Catch::Approx(kDefaultOpacity).margin(kOpacityTolerance));
        CHECK(ramp(kEarlyEnd) == Catch::Approx(kHalfOpacity).margin(kOpacityTolerance));
        CHECK(ramp(kBetweenEndAndStart) == Catch::Approx(kHalfOpacity).margin(kOpacityTolerance));
        CHECK(ramp(kStraightDownTilt) == Catch::Approx(kHalfOpacity).margin(kOpacityTolerance));
    }

    SECTION("the start at the end") {
        constexpr int kStep = 50;
        const auto ramp = [](double tilt) {
            return cw::billboard::rampedXRayOpacity(tilt, kDefaultOpacityPercent, kHalfOpacityPercent,
                                                    kStep, kStep);
        };
        CHECK(ramp(kStep - kJustBelow) == Catch::Approx(kDefaultOpacity).margin(kOpacityTolerance));
        CHECK(ramp(kStep) == Catch::Approx(kHalfOpacity).margin(kOpacityTolerance));
    }
}

TEST_CASE("The camera tilt comes from the view matrix: 90 straight down, 0 level, -90 straight up",
          "[cwBillboardDepthMode]")
{
    CHECK(cw::billboard::cameraTiltDegrees(QMatrix4x4()) == Catch::Approx(kStraightDownTilt).margin(kTiltToleranceDegrees));
    CHECK(cw::billboard::cameraTiltDegrees(pitchedView(kStraightDownTilt)) == Catch::Approx(kStraightDownTilt).margin(kTiltToleranceDegrees));
    CHECK(cw::billboard::cameraTiltDegrees(pitchedView(kLevelTilt)) == Catch::Approx(kLevelTilt).margin(kTiltToleranceDegrees));
    CHECK(cw::billboard::cameraTiltDegrees(pitchedView(kStraightUpTilt)) == Catch::Approx(kStraightUpTilt).margin(kTiltToleranceDegrees));
    CHECK(cw::billboard::cameraTiltDegrees(pitchedView(kHalfwayDownTilt)) == Catch::Approx(kHalfwayDownTilt).margin(kTiltToleranceDegrees));
    CHECK(cw::billboard::cameraTiltDegrees(pitchedView(kMidRampTilt)) == Catch::Approx(kMidRampTilt).margin(kTiltToleranceDegrees));

    SECTION("azimuth and pan leave the tilt alone") {
        constexpr float kAzimuth = 30.0f;
        const QVector3D kPan(12.0f, -7.0f, -50.0f);

        QMatrix4x4 turned = pitchedView(kHalfwayDownTilt);
        turned.rotate(kAzimuth, 0.0f, 0.0f, 1.0f);
        CHECK(cw::billboard::cameraTiltDegrees(turned) == Catch::Approx(kHalfwayDownTilt).margin(kTiltToleranceDegrees));

        QMatrix4x4 panned;
        panned.translate(kPan);
        CHECK(cw::billboard::cameraTiltDegrees(panned) == Catch::Approx(kStraightDownTilt).margin(kTiltToleranceDegrees));

        QMatrix4x4 pannedLevel = pitchedView(kLevelTilt);
        pannedLevel.translate(kPan);
        CHECK(cw::billboard::cameraTiltDegrees(pannedLevel) == Catch::Approx(kLevelTilt).margin(kTiltToleranceDegrees));
    }
}
