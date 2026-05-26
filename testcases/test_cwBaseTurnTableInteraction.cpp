//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwBaseTurnTableInteraction.h"
#include "cwCamera.h"
#include "cwScene.h"
#include "cwTurnTableViewState.h"

//Qt includes
#include <QMatrix4x4>
#include <QPoint>
#include <QSignalSpy>
#include <QTest>
#include <QVector3D>

//QMath3d
#include <QBox3D>
#include <QPlane3D>
#include <qray3d.h>

//Std
#include <cmath>

using namespace Catch;

namespace {

constexpr int kViewportWidth = 800;
constexpr int kViewportHeight = 600;

// Default-view translation set by resetView() (cwBaseTurnTableInteraction.cpp:248-250).
constexpr float kDefaultViewZ = -50.0f;

// Tolerance for comparing two view matrices element-wise.
constexpr float kMatrixEps = 1e-3f;

// Tolerance for project()/screen-position comparisons in ortho. The existing
// centerOn() math goes through QRect::center() (integer truncation) and the
// +1 fixup in cwCamera::mapToQtViewport, so an exact pixel match isn't
// possible. 3 px is loose enough to absorb that rounding and tight enough
// to detect a real regression (e.g. landing on the wrong axis or quadrant).
constexpr float kPixelTolerance = 3.0f;

// Animation completion is detected by spying on cwCamera::viewMatrixChanged
// and draining ticks until no signal fires within a quiet window. This
// adapts to CI load instead of pinning a fixed wait.
constexpr int kAnimationFirstTickTimeoutMs = 500;
constexpr int kAnimationQuietWindowMs = 100;

// Box used by the zoomTo test — a 20 × 20 × 2 axis-aligned box centered at
// the origin. Z half-thickness is small so the box "lies flat" on the XY
// plane the grid plane uses.
const QVector3D kZoomToBoxMin(-10.0f, -10.0f, -1.0f);
const QVector3D kZoomToBoxMax(10.0f, 10.0f, 1.0f);

// Click positions used by orbit-around-pivot tests. Chosen off-center so
// the pivot lands at a non-origin world point, exercising the
// translate-rotate-translate path in updateRotationMatrix().
constexpr QPoint kOffCenterClick(200, 150);
constexpr QPoint kFirstClick(300, 200);
constexpr QPoint kSecondClick(500, 400);

struct Fixture
{
    cwCamera camera;
    cwScene scene;
    cwBaseTurnTableInteraction interaction;

    Fixture()
    {
        camera.setViewport(QRect(0, 0, kViewportWidth, kViewportHeight));
        camera.setProjection(camera.orthoProjectionDefault());
        interaction.setCamera(&camera);
        // Default grid plane: XY plane through the origin. Used by
        // unProject() when no scene geometry is hit (cwBaseTurnTableInteraction.cpp:103-104).
        interaction.setGridPlane(QPlane3D(QVector3D(0, 0, 0), QVector3D(0, 0, 1)));
        interaction.setScene(&scene);
    }
};

bool matricesNearlyEqual(const QMatrix4x4& a, const QMatrix4x4& b, float eps = kMatrixEps)
{
    for (int i = 0; i < 16; ++i) {
        if (std::abs(a.constData()[i] - b.constData()[i]) > eps) {
            return false;
        }
    }
    return true;
}

QPointF screenCenter()
{
    return QPointF(kViewportWidth / 2.0, kViewportHeight / 2.0);
}

// Reproduce the same unproject-to-grid-plane that startRotating / startPanning use,
// so we can compute the world pivot a test would expect without poking the private
// m_center / LastMouseGlobalPosition member.
QVector3D unprojectClickOntoGrid(const cwCamera& cam, QPoint qtViewportPoint)
{
    QPoint mapped = cam.mapToGLViewport(qtViewportPoint);
    QVector3D front = cam.unProject(mapped, 0.0);
    QVector3D back = cam.unProject(mapped, 1.0);
    QVector3D dir = (back - front).normalized();
    QRay3D ray(front, dir);
    QPlane3D plane(QVector3D(0, 0, 0), QVector3D(0, 0, 1));
    double t = plane.intersection(ray);
    return ray.point(t);
}

} // namespace

TEST_CASE("cwBaseTurnTableInteraction default state after setCamera",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    CHECK(f.interaction.azimuth() == Approx(0.0));
    CHECK(f.interaction.pitch() == Approx(90.0));

    QMatrix4x4 expectedView;
    expectedView.translate(QVector3D(0.0f, 0.0f, kDefaultViewZ));
    CHECK(matricesNearlyEqual(f.camera.viewMatrix(), expectedView));

    CHECK(f.camera.zoomScale() == Approx(f.camera.defaultZoomScale()));
}

TEST_CASE("cwBaseTurnTableInteraction setAzimuth/setPitch update the view matrix",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    QSignalSpy azimuthSpy(&f.interaction, &cwBaseTurnTableInteraction::azimuthChanged);
    QSignalSpy pitchSpy(&f.interaction, &cwBaseTurnTableInteraction::pitchChanged);

    QMatrix4x4 beforeAzimuth = f.camera.viewMatrix();
    f.interaction.setAzimuth(45.0);
    CHECK(f.interaction.azimuth() == Approx(45.0));
    CHECK(azimuthSpy.count() == 1);
    CHECK_FALSE(matricesNearlyEqual(beforeAzimuth, f.camera.viewMatrix()));

    QMatrix4x4 beforePitch = f.camera.viewMatrix();
    f.interaction.setPitch(45.0);
    CHECK(f.interaction.pitch() == Approx(45.0));
    CHECK(pitchSpy.count() == 1);
    CHECK_FALSE(matricesNearlyEqual(beforePitch, f.camera.viewMatrix()));
}

TEST_CASE("cwBaseTurnTableInteraction azimuthLocked blocks setAzimuth",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    f.interaction.setAzimuthLocked(true);
    CHECK(f.interaction.isAzimuthLocked());

    const double initial = f.interaction.azimuth();
    QMatrix4x4 beforeView = f.camera.viewMatrix();
    f.interaction.setAzimuth(45.0);

    CHECK(f.interaction.azimuth() == Approx(initial));
    CHECK(matricesNearlyEqual(f.camera.viewMatrix(), beforeView));
}

TEST_CASE("cwBaseTurnTableInteraction pitchLocked blocks setPitch",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    f.interaction.setPitchLocked(true);
    CHECK(f.interaction.isPitchLocked());

    const double initial = f.interaction.pitch();
    QMatrix4x4 beforeView = f.camera.viewMatrix();
    f.interaction.setPitch(45.0);

    CHECK(f.interaction.pitch() == Approx(initial));
    CHECK(matricesNearlyEqual(f.camera.viewMatrix(), beforeView));
}

TEST_CASE("cwBaseTurnTableInteraction gridPlane round-trips through setter/getter",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    QPlane3D plane(QVector3D(1.0f, 2.0f, 3.0f), QVector3D(0.0f, 1.0f, 0.0f));
    f.interaction.setGridPlane(plane);
    QPlane3D got = f.interaction.gridPlane();

    CHECK(got.origin().x() == Approx(plane.origin().x()));
    CHECK(got.origin().y() == Approx(plane.origin().y()));
    CHECK(got.origin().z() == Approx(plane.origin().z()));
    CHECK(got.normal().x() == Approx(plane.normal().x()));
    CHECK(got.normal().y() == Approx(plane.normal().y()));
    CHECK(got.normal().z() == Approx(plane.normal().z()));
}

TEST_CASE("cwBaseTurnTableInteraction centerOn(point, false) puts point at screen center",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    QVector3D point(10.0f, 5.0f, 0.0f);
    f.interaction.centerOn(point, false);

    QPointF projected = f.camera.project(point);
    QPointF expected = screenCenter();

    CHECK(projected.x() == Approx(expected.x()).margin(kPixelTolerance));
    CHECK(projected.y() == Approx(expected.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction centerOn(point, true) end-state matches the snap path",
          "[cwBaseTurnTableInteraction]")
{
    QVector3D point(7.5f, -3.25f, 0.0f);

    // Snap path: build a reference final view matrix using the no-animate branch.
    QMatrix4x4 expectedView;
    {
        Fixture snap;
        snap.interaction.centerOn(point, false);
        expectedView = snap.camera.viewMatrix();
    }

    // Animated path: drive the animation to completion by draining
    // viewMatrixChanged ticks until the spy goes quiet for one window.
    // This is event-driven and adapts to CI load instead of relying on a
    // fixed wait coupled to the hardcoded 1000 ms animation duration.
    Fixture f;
    QSignalSpy spy(&f.camera, &cwCamera::viewMatrixChanged);
    f.interaction.centerOn(point, true);
    REQUIRE(spy.wait(kAnimationFirstTickTimeoutMs));
    while (spy.wait(kAnimationQuietWindowMs)) {
        // drain remaining animation ticks
    }

    CHECK(matricesNearlyEqual(f.camera.viewMatrix(), expectedView));
}

TEST_CASE("cwBaseTurnTableInteraction zoomTo resets orientation and frames the box center",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    // Rotate first so we can verify zoomTo resets orientation.
    f.interaction.setAzimuth(75.0);
    f.interaction.setPitch(45.0);
    REQUIRE(f.interaction.azimuth() == Approx(75.0));
    REQUIRE(f.interaction.pitch() == Approx(45.0));

    QBox3D box(kZoomToBoxMin, kZoomToBoxMax);
    f.interaction.zoomTo(box);

    CHECK(f.interaction.azimuth() == Approx(0.0));
    CHECK(f.interaction.pitch() == Approx(90.0));

    QPointF projected = f.camera.project(box.center());
    QPointF expected = screenCenter();
    CHECK(projected.x() == Approx(expected.x()).margin(kPixelTolerance));
    CHECK(projected.y() == Approx(expected.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction startRotating + setAzimuth orbits around the click point",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    // Click off-center so the pivot ends up at a non-origin world point.
    QVector3D expectedPivot = unprojectClickOntoGrid(f.camera, kOffCenterClick);

    f.interaction.startRotating(kOffCenterClick);

    // The pivot's screen position before any rotation, sanity check.
    QPointF screenBefore = f.camera.project(expectedPivot);

    // Rotate via the azimuth setter. updateRotationMatrix() should orbit
    // around expectedPivot, keeping its screen position invariant.
    f.interaction.setAzimuth(30.0);

    QPointF screenAfter = f.camera.project(expectedPivot);
    CHECK(screenAfter.x() == Approx(screenBefore.x()).margin(kPixelTolerance));
    CHECK(screenAfter.y() == Approx(screenBefore.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction successive startRotating updates the orbit pivot",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    f.interaction.startRotating(kFirstClick);
    f.interaction.setAzimuth(30.0);

    // Second click somewhere else; the pivot must move with it.
    QVector3D expectedSecondPivot = unprojectClickOntoGrid(f.camera, kSecondClick);

    QPointF screenBefore = f.camera.project(expectedSecondPivot);
    f.interaction.startRotating(kSecondClick);
    f.interaction.setAzimuth(60.0);
    QPointF screenAfter = f.camera.project(expectedSecondPivot);

    CHECK(screenAfter.x() == Approx(screenBefore.x()).margin(kPixelTolerance));
    CHECK(screenAfter.y() == Approx(screenBefore.y()).margin(kPixelTolerance));
}

// --- Commit 2 cases ---

TEST_CASE("cwBaseTurnTableInteraction setCenter updates the orbit pivot and fires centerChanged",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    QSignalSpy spy(&f.interaction, &cwBaseTurnTableInteraction::centerChanged);

    const QVector3D pivot(5.0f, -3.0f, 0.0f);
    f.interaction.setCenter(pivot);
    CHECK(f.interaction.center() == pivot);
    CHECK(spy.size() == 1);

    // Idempotent: same value should not re-emit.
    f.interaction.setCenter(pivot);
    CHECK(spy.size() == 1);

    // setAzimuth should now orbit around `pivot`, keeping its screen position fixed.
    QPointF screenBefore = f.camera.project(pivot);
    f.interaction.setAzimuth(45.0);
    QPointF screenAfter = f.camera.project(pivot);
    CHECK(screenAfter.x() == Approx(screenBefore.x()).margin(kPixelTolerance));
    CHECK(screenAfter.y() == Approx(screenBefore.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction setCenterLocked blocks startRotating from moving the pivot",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    QSignalSpy spy(&f.interaction, &cwBaseTurnTableInteraction::centerLockedChanged);

    const QVector3D pinned(2.0f, 7.0f, 0.0f);
    f.interaction.setCenter(pinned);
    f.interaction.setCenterLocked(true);
    CHECK(f.interaction.isCenterLocked());
    CHECK(spy.size() == 1);

    // A startRotating elsewhere must not change m_center.
    f.interaction.startRotating(kOffCenterClick);
    CHECK(f.interaction.center() == pinned);

    // And the orbit must still be around the pinned point.
    QPointF screenBefore = f.camera.project(pinned);
    f.interaction.setAzimuth(30.0);
    QPointF screenAfter = f.camera.project(pinned);
    CHECK(screenAfter.x() == Approx(screenBefore.x()).margin(kPixelTolerance));
    CHECK(screenAfter.y() == Approx(screenBefore.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction setCenterLocked blocks startPanning from moving the pivot",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    const QVector3D pinned(0.0f, 0.0f, 0.0f);
    f.interaction.setCenter(pinned);
    f.interaction.setCenterLocked(true);

    f.interaction.startPanning(kOffCenterClick);
    CHECK(f.interaction.center() == pinned);
}

TEST_CASE("cwBaseTurnTableInteraction setCenterLocked(false) restores the default startRotating behavior",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    f.interaction.setCenterLocked(true);
    f.interaction.setCenterLocked(false);

    // After unlocking, startRotating should reset the pivot to the unprojected click.
    f.interaction.startRotating(kOffCenterClick);
    QVector3D expected = unprojectClickOntoGrid(f.camera, kOffCenterClick);
    CHECK(f.interaction.center().x() == Approx(expected.x()));
    CHECK(f.interaction.center().y() == Approx(expected.y()));
    CHECK(f.interaction.center().z() == Approx(expected.z()));
}

TEST_CASE("cwBaseTurnTableInteraction azimuthLocked emits NOTIFY signal",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    QSignalSpy spy(&f.interaction, &cwBaseTurnTableInteraction::azimuthLockedChanged);

    f.interaction.setAzimuthLocked(true);
    CHECK(spy.size() == 1);
    f.interaction.setAzimuthLocked(true);    // idempotent
    CHECK(spy.size() == 1);
    f.interaction.setAzimuthLocked(false);
    CHECK(spy.size() == 2);
}

TEST_CASE("cwBaseTurnTableInteraction pitchLocked emits NOTIFY signal",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    QSignalSpy spy(&f.interaction, &cwBaseTurnTableInteraction::pitchLockedChanged);

    f.interaction.setPitchLocked(true);
    CHECK(spy.size() == 1);
    f.interaction.setPitchLocked(true);
    CHECK(spy.size() == 1);
    f.interaction.setPitchLocked(false);
    CHECK(spy.size() == 2);
}

TEST_CASE("cwBaseTurnTableInteraction gridPlane setter emits gridPlaneChanged",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    QSignalSpy spy(&f.interaction, &cwBaseTurnTableInteraction::gridPlaneChanged);

    QPlane3D p1(QVector3D(1, 0, 0), QVector3D(0, 1, 0));
    f.interaction.setGridPlane(p1);
    CHECK(spy.size() == 1);

    // Idempotent on same plane.
    f.interaction.setGridPlane(p1);
    CHECK(spy.size() == 1);
}

TEST_CASE("cwBaseTurnTableInteraction viewState round-trips through setViewState (ortho)",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    cwTurnTableViewState target;
    target.center = QVector3D(3.0f, -2.0f, 1.0f);
    target.azimuth = 37.0;
    target.pitch = 42.0;
    target.distance = 75.0;
    target.zoomScale = 2.5;

    f.interaction.setViewState(target);

    cwTurnTableViewState got = f.interaction.viewState();
    CHECK(got.center.x() == Approx(target.center.x()).margin(kMatrixEps));
    CHECK(got.center.y() == Approx(target.center.y()).margin(kMatrixEps));
    CHECK(got.center.z() == Approx(target.center.z()).margin(kMatrixEps));
    CHECK(got.azimuth == Approx(target.azimuth));
    CHECK(got.pitch == Approx(target.pitch));
    CHECK(got.distance == Approx(target.distance).margin(kMatrixEps));
    CHECK(got.zoomScale == Approx(target.zoomScale));

    // After setViewState, setAzimuth orbits around the new center.
    QPointF screenBefore = f.camera.project(target.center);
    f.interaction.setAzimuth(target.azimuth + 15.0);
    QPointF screenAfter = f.camera.project(target.center);
    CHECK(screenAfter.x() == Approx(screenBefore.x()).margin(kPixelTolerance));
    CHECK(screenAfter.y() == Approx(screenBefore.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction viewState round-trips through setViewState (perspective)",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());

    cwTurnTableViewState target;
    target.center = QVector3D(1.0f, 1.0f, 0.0f);
    target.azimuth = 60.0;
    target.pitch = 30.0;
    target.distance = 100.0;
    target.zoomScale = 1.5; // zoomScale is stored even under perspective so the round-trip is faithful.

    f.interaction.setViewState(target);

    cwTurnTableViewState got = f.interaction.viewState();
    CHECK(got.center.x() == Approx(target.center.x()).margin(kMatrixEps));
    CHECK(got.center.y() == Approx(target.center.y()).margin(kMatrixEps));
    CHECK(got.center.z() == Approx(target.center.z()).margin(kMatrixEps));
    CHECK(got.azimuth == Approx(target.azimuth));
    CHECK(got.pitch == Approx(target.pitch));
    CHECK(got.distance == Approx(target.distance).margin(kMatrixEps));
    CHECK(got.zoomScale == Approx(target.zoomScale));

    // The center must project to view-space depth = -distance (the canonical recipe).
    QVector3D centerView = f.camera.viewMatrix().map(target.center);
    CHECK(centerView.z() == Approx(-target.distance).margin(kMatrixEps));
}

TEST_CASE("cwBaseTurnTableInteraction setViewState clamps non-positive distance and zoomScale",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    cwTurnTableViewState bad;
    bad.center = QVector3D(0, 0, 0);
    bad.azimuth = 0.0;
    bad.pitch = 90.0;
    bad.distance = 0.0;     // would put eye at center; degenerate matrix
    bad.zoomScale = -1.0;   // would invert the ortho frustum

    f.interaction.setViewState(bad);

    cwTurnTableViewState got = f.interaction.viewState();
    // Clamped to small positive minima — exact value is not load-bearing,
    // just must be strictly positive.
    CHECK(got.distance > 0.0);
    CHECK(got.zoomScale > 0.0);

    // The resulting view matrix must be non-degenerate (the center should
    // still project to a finite point well behind the eye in view space).
    QVector3D centerView = f.camera.viewMatrix().map(bad.center);
    CHECK(std::isfinite(centerView.z()));
    CHECK(centerView.z() < 0.0);
}

TEST_CASE("cwBaseTurnTableInteraction setViewState produces canonical view matrix",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    cwTurnTableViewState s;
    s.center = QVector3D(0.0f, 0.0f, 0.0f);
    s.azimuth = 0.0;
    s.pitch = 90.0;
    s.distance = 50.0;
    s.zoomScale = f.camera.defaultZoomScale();

    f.interaction.setViewState(s);

    // Canonical default view: identity rotation (pitch 90, az 0) translated -50 along Z.
    QMatrix4x4 expected;
    expected.translate(QVector3D(0.0f, 0.0f, -50.0f));
    expected.rotate(QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, 90.0f));
    expected.rotate(QQuaternion::fromAxisAndAngle(0.0f, 0.0f, 1.0f, 0.0f));
    CHECK(matricesNearlyEqual(f.camera.viewMatrix(), expected));
}
