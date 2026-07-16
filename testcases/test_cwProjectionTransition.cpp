//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwProjectionTransition.h"
#include "cwCamera.h"
#include "cwScene.h"
#include "cwOrthogonalProjection.h"
#include "cwPerspectiveProjection.h"
#include "cwBaseTurnTableInteraction.h"
#include "cwTurnTableViewState.h"
#include "cwProjection.h"
#include "ProjectionScaleTestHelpers.h"

//Qt includes
#include <QMatrix4x4>
#include <QPlane3D>

//Std
#include <cmath>

using namespace Catch;

namespace {

constexpr int kViewportWidth = 800;
constexpr int kViewportHeight = 600;
constexpr double kFovDegrees = cwCamera::defaultFieldOfView();
constexpr float kMatrixEps = 1e-3f;

cwTurnTableViewState planViewState(double distance, double zoomScale)
{
    cwTurnTableViewState s;
    s.center = QVector3D(0.0f, 0.0f, 0.0f);
    s.azimuth = 0.0;
    s.pitch = 90.0;
    s.distance = distance;
    s.zoomScale = zoomScale;
    return s;
}

// The transition orchestrates real collaborators (camera + interaction +
// projection providers). The providers are constructed without a viewer here —
// they still hold/report enabled() and fieldOfView(), which is all the
// orchestration depends on. The zoom-channel math itself is unit-tested in
// test_cwBaseTurnTableInteraction.cpp (reconcileZoomForProjection).
struct Fixture
{
    cwCamera camera;
    cwScene scene;
    cwOrthogonalProjection ortho;
    cwPerspectiveProjection perspective;
    cwBaseTurnTableInteraction interaction;
    cwProjectionTransition transition;

    Fixture()
    {
        camera.setViewport(QRect(0, 0, kViewportWidth, kViewportHeight));
        camera.setProjection(camera.orthoProjectionDefault());
        perspective.setFieldOfView(kFovDegrees);

        interaction.setGridPlane(QPlane3D(QVector3D(0, 0, 0), QVector3D(0, 0, 1)));
        interaction.setScene(&scene);
        interaction.setCamera(&camera);

        transition.setCamera(&camera);
        transition.setOrthoProjection(&ortho);
        transition.setPerspectiveProjection(&perspective);
        transition.setInteraction(&interaction);
    }
};

} // namespace

TEST_CASE("cwProjectionTransition leaving ortho reconciles the eye distance",
          "[cwProjectionTransition]")
{
    // Scrubbing off the ortho endpoint must reconcile the inactive (eye
    // distance) channel for the perspective target, so the morph glides at
    // constant on-screen scale.
    Fixture f;
    const cwTurnTableViewState s = planViewState(50.0, 0.8);
    f.interaction.setViewState(s);

    f.transition.setProgress(0.5);

    const double expectedDist = matchedPerspectiveDistance(s.zoomScale, kFovDegrees, kViewportHeight);
    const QVector3D centerView = f.camera.viewMatrix().map(s.center);
    CHECK(centerView.z() == Approx(-expectedDist).margin(kMatrixEps));
    // zoomScale is the source channel here — untouched.
    CHECK(f.camera.zoomScale() == Approx(s.zoomScale));
}

TEST_CASE("cwProjectionTransition leaving perspective reconciles the zoom scale",
          "[cwProjectionTransition]")
{
    Fixture f;
    // Settle at the perspective endpoint without triggering a reconcile, then
    // set up a perspective view to scrub back from.
    f.transition.setProgress(1.0);
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());
    const cwTurnTableViewState s = planViewState(300.0, f.camera.defaultZoomScale());
    f.interaction.setViewState(s);

    f.transition.setProgress(0.5);

    const double expectedZoom = matchedOrthoZoom(s.distance, kFovDegrees, kViewportHeight);
    CHECK(f.camera.zoomScale() == Approx(expectedZoom).margin(kMatrixEps));
}

TEST_CASE("cwProjectionTransition reconcile reads the perspective projection's fov",
          "[cwProjectionTransition]")
{
    // A wider fov needs a larger eye distance to hold the same on-screen scale;
    // the transition must read it from the wired perspective provider.
    Fixture f;
    constexpr double kWideFovDegrees = 80.0;
    f.perspective.setFieldOfView(kWideFovDegrees);

    const cwTurnTableViewState s = planViewState(50.0, 0.5);
    f.interaction.setViewState(s);

    f.transition.setProgress(0.5);

    const double expectedDist = matchedPerspectiveDistance(s.zoomScale, kWideFovDegrees, kViewportHeight);
    const QVector3D centerView = f.camera.viewMatrix().map(s.center);
    CHECK(centerView.z() == Approx(-expectedDist).margin(kMatrixEps));
}

TEST_CASE("cwProjectionTransition settles onto a typed projection at each endpoint",
          "[cwProjectionTransition]")
{
    Fixture f;

    f.transition.setProgress(1.0);
    CHECK(f.perspective.enabled());
    CHECK_FALSE(f.ortho.enabled());

    f.transition.setProgress(0.0);
    CHECK(f.ortho.enabled());
    CHECK_FALSE(f.perspective.enabled());
}

TEST_CASE("cwProjectionTransition pushes a custom projection while scrubbing",
          "[cwProjectionTransition]")
{
    // Between the endpoints the camera carries an interpolated (untyped) custom
    // projection, not a typed ortho/perspective one.
    Fixture f;
    f.interaction.setViewState(planViewState(50.0, 0.8));

    f.transition.setProgress(0.5);

    CHECK(f.camera.projection().type() == cwProjection::Unknown);
}
