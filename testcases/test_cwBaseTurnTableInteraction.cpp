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
#include <QVariantAnimation>
#include <QVector3D>

//QMath3d
#include <QBox3D>
#include <QPlane3D>
#include <qray3d.h>

//Std
#include <cmath>
#include <limits>

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

// Match cwCamera::perspectiveProjectionDefault (cwCamera.cpp:259-265).
constexpr double kPerspectiveFovYDegrees = 55.0;
constexpr double kPerspectiveAspect =
        double(kViewportWidth) / double(kViewportHeight);

// Replicates zoomTo's perspective fit math so tests can verify the
// resulting eye-to-box-center distance independently. Translated 1:1 from
// the production code so any divergence between this and the implementation
// trips the test. Reads the framing pad from the production constant so a
// future tune of FramingPad updates both sides together.
inline double expectedZoomToDistPerspective(const QBox3D& box,
                                            double fovYDegrees,
                                            double aspect)
{
    constexpr double kEpsilon = 1.0e-6;
    const QVector3D half = 0.5f * (box.maximum() - box.minimum());
    const double hx = (std::max)(double(half.x()), kEpsilon);
    const double hy = (std::max)(double(half.y()), kEpsilon);
    const double hz = (std::max)(double(half.z()), 0.0);
    const double tanHalfFovY = std::tan(0.5 * fovYDegrees * M_PI / 180.0);
    const double tanHalfFovX = tanHalfFovY * aspect;
    const double dY = hy / (std::max)(tanHalfFovY, kEpsilon);
    const double dX = hx / (std::max)(tanHalfFovX, kEpsilon);
    return ((std::max)(dX, dY) + hz)
            * double(cwBaseTurnTableInteraction::FramingPad);
}

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

TEST_CASE("cwBaseTurnTableInteraction zoomTo (perspective) resets orientation and centers a non-origin box",
          "[cwBaseTurnTableInteraction]")
{
    // Regression: the perspective branch of zoomTo had a sign error
    // (-forwardWorld * dist instead of forwardWorld * dist) AND an inverted
    // canInvert guard (bailed on success, ran on failure), so the camera
    // either stayed at the resetView default distance or moved toward the
    // box instead of away. This test exercises perspective with an
    // off-origin box to catch both the recenter step and the distance step.
    Fixture f;
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());

    f.interaction.setAzimuth(75.0);
    f.interaction.setPitch(45.0);

    const QVector3D boxMin(40.0f, 60.0f, -1.0f);
    const QVector3D boxMax(60.0f, 80.0f,  1.0f);
    QBox3D box(boxMin, boxMax);
    const QVector3D c = box.center();

    f.interaction.zoomTo(box);

    CHECK(f.interaction.azimuth() == Approx(0.0));
    CHECK(f.interaction.pitch() == Approx(90.0));

    // Box center projects to viewport center.
    QPointF projected = f.camera.project(c);
    QPointF expected = screenCenter();
    CHECK(projected.x() == Approx(expected.x()).margin(kPixelTolerance));
    CHECK(projected.y() == Approx(expected.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction zoomTo (perspective) places eye at the computed fit distance",
          "[cwBaseTurnTableInteraction]")
{
    // The view-space depth of box.center after zoomTo must equal the
    // perspective fit distance the math prescribes — that's what makes the
    // box fit the viewport without clipping or shrinking. The current
    // buggy code leaves the box at the default distance (50) regardless
    // of the computed dist, or — with the sign fixed — moves the camera
    // toward the box. Either failure shows up here.
    Fixture f;
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());

    QBox3D box(kZoomToBoxMin, kZoomToBoxMax);
    f.interaction.zoomTo(box);

    const QVector3D centerView = f.camera.viewMatrix().map(box.center());
    const double expectedDist = expectedZoomToDistPerspective(
                box, kPerspectiveFovYDegrees, kPerspectiveAspect);

    CHECK(centerView.x() == Approx(0.0).margin(kMatrixEps));
    CHECK(centerView.y() == Approx(0.0).margin(kMatrixEps));
    CHECK(centerView.z() == Approx(-expectedDist).margin(kMatrixEps));
}

TEST_CASE("cwBaseTurnTableInteraction zoomTo (perspective) fits the box within the viewport",
          "[cwBaseTurnTableInteraction]")
{
    // After zoomTo, the box's bounding extent on the dominant axis must
    // very nearly fill the viewport (1 / pad fraction). An asymmetric box
    // pins which axis is dominant, so we can tell whether the fit math
    // picked the right one.
    Fixture f;
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());

    // Tall box: hy > hx, so vertical FOV is the limiting axis. hz kept
    // near zero so that the +hz fudge in zoomTo's fit math doesn't push
    // the camera measurably further back; the projected extent should
    // sit at exactly (halfH / pad) within rounding.
    const QBox3D tall(QVector3D(-5.0f, -20.0f, -1.0e-4f),
                      QVector3D( 5.0f,  20.0f,  1.0e-4f));
    f.interaction.zoomTo(tall);

    // Project the four lateral box corners; the topmost/bottommost should
    // sit just inside the viewport (the 1/pad band around the centre).
    QPointF top    = f.camera.project(QVector3D(0.0f,  20.0f, 0.0f));
    QPointF bottom = f.camera.project(QVector3D(0.0f, -20.0f, 0.0f));
    QPointF left   = f.camera.project(QVector3D(-5.0f, 0.0f, 0.0f));
    QPointF right  = f.camera.project(QVector3D( 5.0f, 0.0f, 0.0f));

    const double centerY = screenCenter().y();
    const double centerX = screenCenter().x();
    const double halfH = double(kViewportHeight) / 2.0;
    const double halfW = double(kViewportWidth)  / 2.0;

    // Vertical extent fills 1/pad of the viewport height (within a few px).
    const double topOffsetFromCenter    = centerY - top.y();        // top is above center → y < centerY
    const double bottomOffsetFromCenter = bottom.y() - centerY;
    CHECK(topOffsetFromCenter    == Approx(halfH / double(cwBaseTurnTableInteraction::FramingPad)).margin(2.0));
    CHECK(bottomOffsetFromCenter == Approx(halfH / double(cwBaseTurnTableInteraction::FramingPad)).margin(2.0));

    // Horizontal extent is narrower than the viewport (this box is taller
    // than it is wide, so X should not be filling the screen).
    const double leftOffsetFromCenter  = centerX - left.x();
    const double rightOffsetFromCenter = right.x() - centerX;
    CHECK(leftOffsetFromCenter  < halfW);
    CHECK(rightOffsetFromCenter < halfW);
    CHECK(leftOffsetFromCenter  > 0.0);
    CHECK(rightOffsetFromCenter > 0.0);
}

TEST_CASE("cwBaseTurnTableInteraction zoomTo defends against NaN/inf box bounds and a degenerate FOV",
          "[cwBaseTurnTableInteraction]")
{
    // Two failure modes the rewrite has to defend against:
    //   1. A non-null box with NaN/inf min or max (std::max(NaN, x) returns
    //      NaN on libstdc++/libc++ — naive clamps don't help).
    //   2. A perspective projection with fov so small tan(fov/2) underflows
    //      to zero, which without the symmetric kFramingEpsilon guard
    //      would divide by zero and write inf into the view matrix.
    // In both cases the camera must end up with a finite view matrix
    // (early-return is fine; poisoning the camera is not).

    auto allFinite = [](const QMatrix4x4& m) {
        for (int i = 0; i < 16; ++i) {
            if (!std::isfinite(m.constData()[i])) return false;
        }
        return true;
    };

    SECTION("NaN box bounds (ortho)") {
        Fixture f;
        const QMatrix4x4 before = f.camera.viewMatrix();
        const float nan = std::numeric_limits<float>::quiet_NaN();
        QBox3D bad(QVector3D(nan, 0.0f, 0.0f), QVector3D(10.0f, 10.0f, 1.0f));
        REQUIRE(!bad.isNull());
        f.interaction.zoomTo(bad);
        CHECK(allFinite(f.camera.viewMatrix()));
        // Early-return contract: camera left untouched.
        CHECK(matricesNearlyEqual(f.camera.viewMatrix(), before));
    }

    SECTION("inf box bounds (perspective)") {
        Fixture f;
        f.camera.setProjection(f.camera.perspectiveProjectionDefault());
        const QMatrix4x4 before = f.camera.viewMatrix();
        const float inf = std::numeric_limits<float>::infinity();
        QBox3D bad(QVector3D(-10.0f, -10.0f, -1.0f), QVector3D(10.0f, inf, 1.0f));
        REQUIRE(!bad.isNull());
        f.interaction.zoomTo(bad);
        CHECK(allFinite(f.camera.viewMatrix()));
        CHECK(matricesNearlyEqual(f.camera.viewMatrix(), before));
    }

    SECTION("near-zero perspective FOV") {
        // The unguarded path would divide by tan(0) = 0 → inf. With the
        // symmetric kFramingEpsilon guard the view matrix stays finite
        // (dist is huge but bounded).
        Fixture f;
        cwProjection tiny;
        tiny.setPerspective(1.0e-3, kPerspectiveAspect, 1.0, 10000.0);
        f.camera.setProjection(tiny);
        QBox3D box(kZoomToBoxMin, kZoomToBoxMax);
        f.interaction.zoomTo(box);
        CHECK(allFinite(f.camera.viewMatrix()));
    }
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

    // In the turntable convention, (Pitch=90, Azimuth=0) is the "default"
    // and corresponds to identity rotation on the view matrix — matches
    // exactly what resetView() leaves behind: a pure translation.
    QMatrix4x4 expected;
    expected.translate(QVector3D(0.0f, 0.0f, -50.0f));
    CHECK(matricesNearlyEqual(f.camera.viewMatrix(), expected));
}

// --- Commit 3 cases: animateToViewState ---

TEST_CASE("cwBaseTurnTableInteraction animateToViewState end-state matches setViewState",
          "[cwBaseTurnTableInteraction]")
{
    cwTurnTableViewState target;
    target.center = QVector3D(4.0f, -2.0f, 1.5f);
    target.azimuth = 75.0;
    target.pitch = 30.0;
    target.distance = 80.0;
    target.zoomScale = 1.75;

    // Reference: snap path.
    QMatrix4x4 expectedView;
    double expectedZoom = 0.0;
    {
        Fixture snap;
        snap.interaction.setViewState(target);
        expectedView = snap.camera.viewMatrix();
        expectedZoom = snap.camera.zoomScale();
    }

    // Animated path: drive to completion via QSignalSpy quiet-window drain.
    Fixture f;
    QSignalSpy finishedSpy(&f.interaction,
                           &cwBaseTurnTableInteraction::animationFinished);
    QSignalSpy viewSpy(&f.camera, &cwCamera::viewMatrixChanged);
    f.interaction.animateToViewState(target, 100);
    REQUIRE(viewSpy.wait(kAnimationFirstTickTimeoutMs));
    while(viewSpy.wait(kAnimationQuietWindowMs)) {
        // drain remaining ticks
    }

    CHECK(matricesNearlyEqual(f.camera.viewMatrix(), expectedView));
    CHECK(f.camera.zoomScale() == Approx(expectedZoom));
    // animationFinished must have fired exactly once for the run.
    CHECK(finishedSpy.size() == 1);
}

TEST_CASE("cwBaseTurnTableInteraction animateToViewState at t=0.5 is at the midpoint",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    cwTurnTableViewState start;
    start.center = QVector3D(0.0f, 0.0f, 0.0f);
    start.azimuth = 0.0;
    start.pitch = 90.0;
    start.distance = 50.0;
    start.zoomScale = 1.0;
    f.interaction.setViewState(start);

    cwTurnTableViewState target;
    target.center = QVector3D(10.0f, 20.0f, 0.0f);
    target.azimuth = 60.0;
    target.pitch = 30.0;
    target.distance = 100.0;
    target.zoomScale = 2.0;

    f.interaction.animateToViewState(target, 1000);

    // Reach into the private animator by objectName, drive it to t=0.5
    // via setCurrentTime. The InOutCubic easing maps duration/2 to t=0.5
    // exactly (the curve is symmetric around the midpoint).
    auto* anim = f.interaction.findChild<QVariantAnimation*>(
                cwBaseTurnTableInteraction::stateAnimationObjectName);
    REQUIRE(anim != nullptr);
    anim->setCurrentTime(anim->duration() / 2);

    cwTurnTableViewState mid = f.interaction.viewState();
    CHECK(mid.center.x() == Approx(5.0).margin(kMatrixEps));
    CHECK(mid.center.y() == Approx(10.0).margin(kMatrixEps));
    CHECK(mid.center.z() == Approx(0.0).margin(kMatrixEps));
    CHECK(mid.azimuth == Approx(30.0));
    CHECK(mid.pitch == Approx(60.0));
    // Ortho path: distance pinned to target (not lerped).
    CHECK(mid.distance == Approx(target.distance).margin(kMatrixEps));
    CHECK(mid.zoomScale == Approx(1.5));
}

TEST_CASE("cwBaseTurnTableInteraction animateToViewState takes the short-arc on azimuth wrap",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    cwTurnTableViewState start = f.interaction.viewState();
    start.azimuth = 350.0;
    f.interaction.setViewState(start);

    cwTurnTableViewState target = start;
    target.azimuth = 10.0;

    f.interaction.animateToViewState(target, 1000);

    auto* anim = f.interaction.findChild<QVariantAnimation*>(
                cwBaseTurnTableInteraction::stateAnimationObjectName);
    REQUIRE(anim != nullptr);
    anim->setCurrentTime(anim->duration() / 2);

    cwTurnTableViewState mid = f.interaction.viewState();
    // The short arc from 350° to 10° passes through 0°. The long arc
    // would pass through 180° — detect that explicitly so a regression
    // to naive lerp fails loudly.
    const double midAzimuth = mid.azimuth;
    // Wrap-tolerant check: 0° is equivalent to 360°.
    const double distanceFromZero = (std::min)(midAzimuth,
                                               360.0 - midAzimuth);
    CHECK(distanceFromZero == Approx(0.0).margin(0.5));
    CHECK(std::abs(midAzimuth - 180.0) > 90.0); // not on the long arc
}

TEST_CASE("cwBaseTurnTableInteraction animateToViewState lerps distance in perspective",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());

    cwTurnTableViewState start;
    start.center = QVector3D(0.0f, 0.0f, 0.0f);
    start.azimuth = 0.0;
    start.pitch = 90.0;
    start.distance = 50.0;
    start.zoomScale = 1.0;
    f.interaction.setViewState(start);

    cwTurnTableViewState target = start;
    target.distance = 150.0;

    f.interaction.animateToViewState(target, 1000);

    auto* anim = f.interaction.findChild<QVariantAnimation*>(
                cwBaseTurnTableInteraction::stateAnimationObjectName);
    REQUIRE(anim != nullptr);
    anim->setCurrentTime(anim->duration() / 2);

    cwTurnTableViewState mid = f.interaction.viewState();
    // Under perspective the distance channel is lerped: midpoint of
    // (50, 150) is 100.
    CHECK(mid.distance == Approx(100.0).margin(kMatrixEps));
}

TEST_CASE("cwBaseTurnTableInteraction centerOn(point, false) preserves orientation and zoom",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    // Establish a non-default state so the test would notice if any
    // channel other than center accidentally changed.
    f.interaction.setAzimuth(35.0);
    f.interaction.setPitch(60.0);
    REQUIRE(f.interaction.azimuth() == Approx(35.0));
    REQUIRE(f.interaction.pitch() == Approx(60.0));
    const double zoomBefore = f.camera.zoomScale();

    const QVector3D target(3.0f, 4.0f, 0.0f);
    f.interaction.centerOn(target, false);

    // Orientation and zoom unchanged.
    CHECK(f.interaction.azimuth() == Approx(35.0));
    CHECK(f.interaction.pitch() == Approx(60.0));
    CHECK(f.camera.zoomScale() == Approx(zoomBefore));

    // Target lands at screen center, same observable contract as the
    // commit-1 baseline.
    QPointF projected = f.camera.project(target);
    QPointF expected = screenCenter();
    CHECK(projected.x() == Approx(expected.x()).margin(kPixelTolerance));
    CHECK(projected.y() == Approx(expected.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction centerOn from plan view preserves plan view",
          "[cwBaseTurnTableInteraction]")
{
    // Regression: the first call to centerOn() in plan view (Pitch=90,
    // Azimuth=0) used to rebuild the view matrix using an absolute
    // R_x(Pitch)*R_z(Azimuth) rotation. That is wrong for this class —
    // resetView() leaves the view matrix as a pure translation T(0,0,-50)
    // and caches Pitch=90 / CurrentRotation = R_x(90)*R_z(0), so the
    // "default" Pitch=90 must map to identity on the view matrix (not
    // R_x(90), which tips the camera to profile view). The visible
    // symptom was: after gotoLead from the lead page in plan view, the
    // camera flipped to a profile-style horizontal view (Pitch reported
    // 90 but the matrix said otherwise), and subsequent rotation drags
    // could spin the cave upside down because the cached rotation no
    // longer matched the actual view-matrix rotation.
    Fixture f;

    REQUIRE(f.interaction.pitch() == Approx(90.0));
    REQUIRE(f.interaction.azimuth() == Approx(0.0));

    // Snapshot what a true "plan view" should look like: a point above the
    // target in world +Z must project to the SAME screen pixel as the
    // target itself (ortho top-down view = zero parallax in Z).
    const QVector3D target(5.0f, 5.0f, 0.0f);
    const QVector3D above = target + QVector3D(0.0f, 0.0f, 10.0f);

    // Snap centerOn (no animation) to isolate the buildViewMatrix bug.
    f.interaction.centerOn(target, false);

    // The cached angles must not have moved.
    CHECK(f.interaction.pitch() == Approx(90.0));
    CHECK(f.interaction.azimuth() == Approx(0.0));

    // Basic centerOn contract: target projects to screen center.
    QPointF projected = f.camera.project(target);
    QPointF screenC = screenCenter();
    CHECK(projected.x() == Approx(screenC.x()).margin(kPixelTolerance));
    CHECK(projected.y() == Approx(screenC.y()).margin(kPixelTolerance));

    // Plan-view orientation invariant: under the top-down ortho view,
    // points stacked along world Z project to the SAME screen pixel. If
    // the camera was wrongly rotated to profile, the +Z offset would
    // shift the projection (almost certainly along screen Y).
    QPointF projectedAbove = f.camera.project(above);
    CHECK(projectedAbove.x() == Approx(screenC.x()).margin(kPixelTolerance));
    CHECK(projectedAbove.y() == Approx(screenC.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction setViewState defends against NaN/inf channels",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;

    cwTurnTableViewState bad;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const double dnan = std::numeric_limits<double>::quiet_NaN();
    const double dinf = std::numeric_limits<double>::infinity();
    bad.center = QVector3D(nan, nan, nan);
    bad.azimuth = dnan;
    bad.pitch = dinf;
    bad.distance = dnan;
    bad.zoomScale = -dinf;

    f.interaction.setViewState(bad);

    // The matrix must be entirely finite — a single NaN entry would
    // permanently poison the camera.
    const QMatrix4x4 view = f.camera.viewMatrix();
    for(int i = 0; i < 16; ++i) {
        CHECK(std::isfinite(view.constData()[i]));
    }

    // And the read-back state must be finite/clean.
    cwTurnTableViewState got = f.interaction.viewState();
    CHECK(std::isfinite(got.center.x()));
    CHECK(std::isfinite(got.center.y()));
    CHECK(std::isfinite(got.center.z()));
    CHECK(std::isfinite(got.azimuth));
    CHECK(std::isfinite(got.pitch));
    CHECK(got.distance > 0.0);
    CHECK(got.zoomScale > 0.0);
}

TEST_CASE("cwBaseTurnTableInteraction animationFinished is queued — slot re-entry is safe",
          "[cwBaseTurnTableInteraction]")
{
    // Regression: when animationFinished was a direct re-emit of
    // QVariantAnimation::finished, a slot calling animateToViewState
    // synchronously during stopAnimation()'s setCurrentTime(duration)
    // would corrupt m_stateAnimStart/m_stateAnimTarget. Queueing the
    // signal moves listener slots to the next event-loop turn, after
    // the outer call has finished its bookkeeping.
    Fixture f;

    cwTurnTableViewState first;
    first.center = QVector3D(1.0f, 0.0f, 0.0f);
    first.azimuth = 10.0;
    first.pitch = 80.0;
    first.distance = 50.0;
    first.zoomScale = 1.0;

    cwTurnTableViewState second;
    second.center = QVector3D(5.0f, 0.0f, 0.0f);
    second.azimuth = 50.0;
    second.pitch = 40.0;
    second.distance = 80.0;
    second.zoomScale = 1.5;

    int reentryCount = 0;
    QObject::connect(&f.interaction, &cwBaseTurnTableInteraction::animationFinished,
                     [&]() {
        if(reentryCount == 0) {
            ++reentryCount;
            // This is the case the fix is designed to handle: chaining
            // another animateToViewState from the finished slot. With a
            // direct connection this would have re-entered during
            // stopAnimation and clobbered state; with a queued connection
            // it runs cleanly on the next event-loop turn.
            f.interaction.animateToViewState(second, 50);
        }
    });

    QSignalSpy viewSpy(&f.camera, &cwCamera::viewMatrixChanged);
    f.interaction.animateToViewState(first, 50);

    // Drain both animations (first + chained second).
    REQUIRE(viewSpy.wait(kAnimationFirstTickTimeoutMs));
    while(viewSpy.wait(kAnimationQuietWindowMs)) {
        // drain
    }

    // The chained animation must have run and landed at `second`'s
    // end-state. If state was corrupted by re-entry during stopAnimation,
    // the chained animation would have run with a stale target and the
    // viewMatrix would not match.
    CHECK(reentryCount == 1);

    cwTurnTableViewState got = f.interaction.viewState();
    CHECK(got.center.x() == Approx(second.center.x()).margin(kMatrixEps));
    CHECK(got.azimuth == Approx(second.azimuth));
    CHECK(got.pitch == Approx(second.pitch));
}
