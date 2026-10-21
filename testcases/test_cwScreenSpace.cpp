//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Std includes
#include <cmath>

//Qt includes
#include <QMatrix4x4>
#include <QVector3D>

//QMath3d includes
#include <QBox3D>

//Our includes
#include "cwScreenSpace.h"

using namespace cw::sse;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

    constexpr double kTolerance = 1e-9;
    constexpr double kFloatTolerance = 1e-6;

    constexpr float kFovY = 60.0f;
    constexpr float kAspect = 1.0f;
    constexpr float kNearPlane = 0.1f;
    constexpr float kFarPlane = 1000.0f;

    /**
     * A camera looking down -Z from the origin, so a point at world z = -depth
     * has clip w = depth.
     */
    QMatrix4x4 perspectiveViewProjection()
    {
        QMatrix4x4 projection;
        projection.perspective(kFovY, kAspect, kNearPlane, kFarPlane);
        return projection;
    }
}

TEST_CASE("nearestClipW returns 1 for an orthographic camera", "[ScreenSpace]")
{
    //An orthographic projection keeps row 3 as (0, 0, 0, 1)
    QMatrix4x4 ortho;
    ortho.ortho(-10.0f, 10.0f, -10.0f, 10.0f, kNearPlane, kFarPlane);

    const QBox3D box({-3.0f, -4.0f, -50.0f}, {2.0f, 1.0f, -5.0f});

    CHECK_THAT(nearestClipW(box, ortho), WithinAbs(1.0, kTolerance));
}

TEST_CASE("projectedPixels with an identity w gives length · absP11 · H / 2", "[ScreenSpace]")
{
    const QMatrix4x4 identity;
    const QBox3D box({0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});

    const double absP11 = 2.5;
    const int viewportHeightPx = 800;
    const double worldLength = 3.0;

    const double expected = worldLength * absP11 * viewportHeightPx / 2.0;

    CHECK_THAT(projectedPixels(worldLength, box, identity, absP11, viewportHeightPx),
               WithinRel(expected, kTolerance));
    CHECK_THAT(pixelsPerMeter(absP11, viewportHeightPx, 1.0),
               WithinRel(absP11 * viewportHeightPx / 2.0, kTolerance));
}

TEST_CASE("A box behind the camera floors at kMinimumClipW", "[ScreenSpace]")
{
    STATIC_REQUIRE(kMinimumClipW == 1e-4);

    const QMatrix4x4 viewProjection = perspectiveViewProjection();

    //Entirely behind the eye: every corner has w = -z <= 0
    const QBox3D behind({-1.0f, -1.0f, 5.0f}, {1.0f, 1.0f, 20.0f});
    CHECK_THAT(nearestClipW(behind, viewProjection), WithinAbs(kMinimumClipW, kTolerance));

    //Straddling the eye plane: the positive corners are all below the floor
    const QBox3D straddling({-1.0f, -1.0f, -1e-9f}, {1.0f, 1.0f, 20.0f});
    CHECK_THAT(nearestClipW(straddling, viewProjection), WithinAbs(kMinimumClipW, kTolerance));
}

TEST_CASE("A straddling box uses its smallest positive corner, not the floor", "[ScreenSpace]")
{
    const QMatrix4x4 viewProjection = perspectiveViewProjection();

    //Corners at z = -5 give w = 5; corners at z = 20 are behind the eye and ignored
    const QBox3D straddling({-1.0f, -1.0f, -5.0f}, {1.0f, 1.0f, 20.0f});
    CHECK_THAT(nearestClipW(straddling, viewProjection), WithinRel(5.0, kFloatTolerance));
}

TEST_CASE("The nearest corner of the box wins", "[ScreenSpace]")
{
    const QMatrix4x4 viewProjection = perspectiveViewProjection();

    //w = -z for this projection, so the nearest corner is the one at z = -4
    const QBox3D box({-1.0f, -1.0f, -100.0f}, {1.0f, 1.0f, -4.0f});
    CHECK_THAT(nearestClipW(box, viewProjection), WithinRel(4.0, kFloatTolerance));

    //Pushing the whole box further away moves the answer to the new near face
    const QBox3D farther({-1.0f, -1.0f, -100.0f}, {1.0f, 1.0f, -25.0f});
    CHECK_THAT(nearestClipW(farther, viewProjection), WithinRel(25.0, kFloatTolerance));
}

TEST_CASE("A 1 m length at a known depth projects to the hand-computed pixels", "[ScreenSpace]")
{
    const QMatrix4x4 viewProjection = perspectiveViewProjection();

    //P(1, 1) = cot(kFovY / 2)
    const double absP11 = std::abs(double(viewProjection(1, 1)));
    const int viewportHeightPx = 1080;
    const double depth = 10.0;

    const QBox3D box({-0.5f, -0.5f, float(-depth)}, {0.5f, 0.5f, float(-depth)});

    const double expected = absP11 * viewportHeightPx / (2.0 * depth);

    CHECK_THAT(projectedPixels(1.0, box, viewProjection, absP11, viewportHeightPx),
               WithinRel(expected, kFloatTolerance));
}
