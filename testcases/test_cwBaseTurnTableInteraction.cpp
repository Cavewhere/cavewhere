//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwBaseTurnTableInteraction.h"
#include "cwCamera.h"
#include "cwScene.h"
#include "cwGeometryItersecter.h"
#include "cwGeometry.h"
#include "cwRenderObject.h"
#include "cwRayHit.h"
#include "cwTurnTableViewState.h"
#include "ProjectionScaleTestHelpers.h"

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
#include <numeric>

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

// Match cwCamera::perspectiveProjectionDefault, which uses the camera default.
constexpr double kPerspectiveFovYDegrees = cwCamera::defaultFieldOfView();
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

// Adds a small point cluster spanning [-extent, extent] in XY at z=0 to the
// scene's intersecter and blocks until the BVH is built, so unProject() has
// real geometry to hit and to bound the grid-plane fallback against (#527).
void addSmallGeometry(cwScene& scene, float extent, uint64_t id = 1)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, QVector<QVector3D>{
        QVector3D(0.0f, 0.0f, 0.0f),   // center point so a screen-center ray hits
        QVector3D(-extent, -extent, 0.0f),
        QVector3D( extent,  extent, 0.0f),
        QVector3D(-extent,  extent, 0.0f),
        QVector3D( extent, -extent, 0.0f),
    });
    geometry.setType(cwGeometry::Type::Points);

    scene.geometryItersecter()->addObject(
        cwGeometryItersecter::Object({nullptr, id}, geometry, QMatrix4x4(), 0.5f));
    scene.geometryItersecter()->waitForFinish();
}

// Elevation of the test centerline above the z=0 grid plane. Lifting it lets a
// test tell a line pivot (z≈kCenterlineZ) apart from the grid-plane fallback
// (z≈0), and is well clear of kPixelTolerance so the two never blur together.
constexpr float kCenterlineZ = 10.0f;

// Adds a centerline (Type::Lines with iota indices, the way cwRenderLinePlot
// registers it) at z = kCenterlineZ and blocks until the BVH is built. No solid
// geometry is added, so the line is the only thing a pick can land on — the
// "all the solid is hidden, still orbit the line plot" case.
void addCenterline(cwScene& scene, uint64_t id = 2)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, QVector<QVector3D>{
        QVector3D(-20.0f, 0.0f, kCenterlineZ),
        QVector3D( 20.0f, 0.0f, kCenterlineZ),
    });
    QVector<uint32_t> indices(2);
    std::iota(indices.begin(), indices.end(), 0u);
    geometry.setIndices(std::move(indices));
    geometry.setType(cwGeometry::Type::Lines);

    scene.geometryItersecter()->addObject(
        cwGeometryItersecter::Object({nullptr, id}, geometry));
    scene.geometryItersecter()->waitForFinish();
}

// Adds a small point cluster of half-size `extent` centered at `center`
// (world space) to the scene's intersecter and blocks until the BVH is built.
// Places the whole scene far from the origin so resetView()'s scene framing
// can be tested against a non-origin datum (issue #549).
void addGeometryAt(cwScene& scene, const QVector3D& center, float extent, uint64_t id = 3)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, QVector<QVector3D>{
        center,
        center + QVector3D(-extent, -extent, 0.0f),
        center + QVector3D( extent,  extent, 0.0f),
        center + QVector3D(-extent,  extent, 0.0f),
        center + QVector3D( extent, -extent, 0.0f),
    });
    geometry.setType(cwGeometry::Type::Points);

    scene.geometryItersecter()->addObject(
        cwGeometryItersecter::Object({nullptr, id}, geometry, QMatrix4x4(), 0.5f));
    scene.geometryItersecter()->waitForFinish();
}

// Adds one cloud point at `position` and blocks until the BVH is built. Placed
// beyond the exact pick's reach but inside the anchor radius, it exercises the
// geometry-anchored pivot fallback (#562) rather than an exact hit.
void addSinglePoint(cwScene& scene, const QVector3D& position,
                    cwRenderObject* parent = nullptr, uint64_t id = 3)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, QVector<QVector3D>{position});
    geometry.setType(cwGeometry::Type::Points);

    scene.geometryItersecter()->addObject(
        cwGeometryItersecter::Object({parent, id}, geometry, QMatrix4x4(), 0.5f));
    scene.geometryItersecter()->waitForFinish();
}

// World-space radius of the rotation pivot's nearest-geometry anchor under
// this fixture's ortho camera, derived through the same pixelsForMillimeters
// + pickQuery pipeline unProject uses. Tests bound this against the offsets
// they hardcode, so a retuned PivotAnchorRadiusMillimeters fails loudly
// instead of silently making a test vacuous. Ortho only: pickQuery fills
// tolerance.constant for ortho and tolerance.slope for perspective, so this
// returns 0.0 under a perspective camera.
double anchorRadiusWorld(const Fixture& f)
{
    return f.camera.pickQuery(
        f.interaction.pixelsForMillimeters(
            cwBaseTurnTableInteraction::PivotAnchorRadiusMillimeters))
        .tolerance.constant;
}

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

TEST_CASE("cwBaseTurnTableInteraction framingViewState centers box at viewport center (ortho)",
          "[cwBaseTurnTableInteraction]")
{
    // After applying the framing state, box.center() must project to the
    // viewport center — that's the contract animateToViewState relies on
    // for the sink-preview camera layer.
    Fixture f;
    const cwTurnTableViewState current = f.interaction.viewState();

    const QBox3D box(QVector3D(40.0f, 60.0f, -1.0f),
                     QVector3D(60.0f, 80.0f,  1.0f));
    cwTurnTableViewState target = f.interaction.framingViewState(
                box, current.azimuth, current.pitch);
    f.interaction.setViewState(target);

    const QPointF projected = f.camera.project(box.center());
    const QPointF expected = screenCenter();
    CHECK(projected.x() == Approx(expected.x()).margin(kPixelTolerance));
    CHECK(projected.y() == Approx(expected.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction framingViewState centers box at viewport center (perspective)",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());
    const cwTurnTableViewState current = f.interaction.viewState();

    const QBox3D box(QVector3D(40.0f, 60.0f, -1.0f),
                     QVector3D(60.0f, 80.0f,  1.0f));
    cwTurnTableViewState target = f.interaction.framingViewState(
                box, current.azimuth, current.pitch);
    f.interaction.setViewState(target);

    const QPointF projected = f.camera.project(box.center());
    const QPointF expected = screenCenter();
    CHECK(projected.x() == Approx(expected.x()).margin(kPixelTolerance));
    CHECK(projected.y() == Approx(expected.y()).margin(kPixelTolerance));

    // Eye-to-center depth in view space matches the production fit math.
    // At default orientation (pitch=90, azimuth=0), the view-space half
    // extents equal the world half extents and the expected distance
    // matches the zoomTo helper.
    target = f.interaction.framingViewState(box, 0.0, 90.0);
    f.interaction.setViewState(target);
    const QVector3D centerView = f.camera.viewMatrix().map(box.center());
    const double expectedDist = expectedZoomToDistPerspective(
                box, kPerspectiveFovYDegrees, kPerspectiveAspect);
    CHECK(centerView.z() == Approx(-expectedDist).margin(kMatrixEps));
}

TEST_CASE("cwBaseTurnTableInteraction framingViewState fits the box within the viewport (ortho)",
          "[cwBaseTurnTableInteraction]")
{
    // At default orientation, an asymmetric box should fill the dominant
    // viewport axis to within 1/FramingPad. Use the orientation we know
    // aligns view axes with world axes (pitch=90, azimuth=0) so the test
    // can predict which axis dominates without modeling the rotated AABB.
    Fixture f;

    // Wider in X than Y → X is the limiting axis.
    const QBox3D wide(QVector3D(-30.0f, -5.0f, -1.0e-4f),
                      QVector3D( 30.0f,  5.0f,  1.0e-4f));
    cwTurnTableViewState target = f.interaction.framingViewState(wide, 0.0, 90.0);
    f.interaction.setViewState(target);

    const QPointF left  = f.camera.project(QVector3D(-30.0f, 0.0f, 0.0f));
    const QPointF right = f.camera.project(QVector3D( 30.0f, 0.0f, 0.0f));
    const QPointF top    = f.camera.project(QVector3D(0.0f,  5.0f, 0.0f));
    const QPointF bottom = f.camera.project(QVector3D(0.0f, -5.0f, 0.0f));

    const double halfW = double(kViewportWidth) / 2.0;
    const double halfH = double(kViewportHeight) / 2.0;
    const double centerX = screenCenter().x();
    const double centerY = screenCenter().y();
    const double pad = double(cwBaseTurnTableInteraction::FramingPad);

    // X fills the viewport at the 1/pad band.
    const double leftFromCenter  = centerX - left.x();
    const double rightFromCenter = right.x() - centerX;
    CHECK(leftFromCenter  == Approx(halfW / pad).margin(2.0));
    CHECK(rightFromCenter == Approx(halfW / pad).margin(2.0));

    // Y stays inside the viewport (this box is wider than tall).
    const double topFromCenter    = centerY - top.y();
    const double bottomFromCenter = bottom.y() - centerY;
    CHECK(topFromCenter    < halfH);
    CHECK(bottomFromCenter < halfH);
    CHECK(topFromCenter    > 0.0);
    CHECK(bottomFromCenter > 0.0);
}

TEST_CASE("cwBaseTurnTableInteraction framingViewState fits box in tilted orientation",
          "[cwBaseTurnTableInteraction]")
{
    // The view-space AABB code path: at non-default orientation, the
    // world-aligned box's projected extents don't equal its world extents.
    // After framing, the projected extents on screen still have to land
    // inside the viewport — otherwise the rotated-AABB math is wrong.
    Fixture f;
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());

    // Cube box — its rotated projection is a hexagon, and the projected
    // extents grow vs. the world half-widths. The fit must compensate.
    const QBox3D cube(QVector3D(-10.0f, -10.0f, -10.0f),
                      QVector3D( 10.0f,  10.0f,  10.0f));
    cwTurnTableViewState target = f.interaction.framingViewState(cube, 45.0, 60.0);
    f.interaction.setViewState(target);

    // All 8 corners must project inside the viewport. The unrotated
    // pre-fix math under-fits by ~sqrt(3) in the worst case — this test
    // catches that regression.
    const QVector3D corners[8] = {
        QVector3D(-10.0f, -10.0f, -10.0f), QVector3D( 10.0f, -10.0f, -10.0f),
        QVector3D(-10.0f,  10.0f, -10.0f), QVector3D( 10.0f,  10.0f, -10.0f),
        QVector3D(-10.0f, -10.0f,  10.0f), QVector3D( 10.0f, -10.0f,  10.0f),
        QVector3D(-10.0f,  10.0f,  10.0f), QVector3D( 10.0f,  10.0f,  10.0f),
    };
    for (const QVector3D& c : corners) {
        const QPointF p = f.camera.project(c);
        CHECK(p.x() >= 0.0);
        CHECK(p.x() <= double(kViewportWidth));
        CHECK(p.y() >= 0.0);
        CHECK(p.y() <= double(kViewportHeight));
    }
}

TEST_CASE("cwBaseTurnTableInteraction framingViewState defends against NaN/inf box",
          "[cwBaseTurnTableInteraction]")
{
    Fixture f;
    const cwTurnTableViewState before = f.interaction.viewState();

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    QBox3D bad(QVector3D(nan, 0.0f, 0.0f), QVector3D(inf, 10.0f, 1.0f));
    REQUIRE(!bad.isNull());

    cwTurnTableViewState got = f.interaction.framingViewState(
                bad, before.azimuth, before.pitch);

    // Identity on poisoned input — no NaN/inf propagation into the
    // returned view state.
    CHECK(got.distance == Approx(before.distance).margin(kMatrixEps));
    CHECK(got.zoomScale == Approx(before.zoomScale));
    CHECK(std::isfinite(got.center.x()));
    CHECK(std::isfinite(got.center.y()));
    CHECK(std::isfinite(got.center.z()));
}

TEST_CASE("cwBaseTurnTableInteraction framingViewState(box, az, pitch) overrides orientation",
          "[cwBaseTurnTableInteraction]")
{
    // The 3-arg overload writes the supplied az/pitch into the returned
    // target — and the fit math uses them, not the current orientation.
    // The sink-clip preview uses this to snap to a pitch=0 profile view.
    Fixture f;
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());
    f.interaction.setAzimuth(45.0);
    f.interaction.setPitch(70.0);

    const QBox3D box(QVector3D(-5.0f, -5.0f, -5.0f),
                     QVector3D( 5.0f,  5.0f,  5.0f));
    const cwTurnTableViewState target =
        f.interaction.framingViewState(box, 30.0, 0.0);

    CHECK(target.azimuth == Approx(30.0));
    CHECK(target.pitch == Approx(0.0));
    CHECK(target.center.x() == Approx(box.center().x()).margin(kMatrixEps));
    CHECK(target.center.y() == Approx(box.center().y()).margin(kMatrixEps));
    CHECK(target.center.z() == Approx(box.center().z()).margin(kMatrixEps));

    // And the framed view actually fits the box in the supplied
    // orientation — apply, then project all corners and verify each
    // lands inside the viewport.
    f.interaction.setViewState(target);
    const QVector3D corners[8] = {
        QVector3D(-5.0f, -5.0f, -5.0f), QVector3D( 5.0f, -5.0f, -5.0f),
        QVector3D(-5.0f,  5.0f, -5.0f), QVector3D( 5.0f,  5.0f, -5.0f),
        QVector3D(-5.0f, -5.0f,  5.0f), QVector3D( 5.0f, -5.0f,  5.0f),
        QVector3D(-5.0f,  5.0f,  5.0f), QVector3D( 5.0f,  5.0f,  5.0f),
    };
    for (const QVector3D& c : corners) {
        const QPointF p = f.camera.project(c);
        CHECK(p.x() >= 0.0);
        CHECK(p.x() <= double(kViewportWidth));
        CHECK(p.y() >= 0.0);
        CHECK(p.y() <= double(kViewportHeight));
    }
}

TEST_CASE("cwBaseTurnTableInteraction framingViewState(box, az, pitch) uses supplied pitch for fit",
          "[cwBaseTurnTableInteraction]")
{
    // The whole point of the 3-arg overload is that the fit math runs
    // against the post-rotation orientation. A tall-thin box (large Z,
    // small XY) needs MORE camera distance at pitch=0 (profile, Z maps
    // to view-Y) than at pitch=90 (top-down, Z maps to view-depth).
    // If we used the current orientation for the fit, both calls would
    // return the same distance.
    Fixture f;
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());
    f.interaction.setAzimuth(0.0);
    f.interaction.setPitch(45.0);

    const QBox3D tall(QVector3D(-1.0f, -1.0f, -50.0f),
                      QVector3D( 1.0f,  1.0f,  50.0f));

    const cwTurnTableViewState profile =
        f.interaction.framingViewState(tall, 0.0, 0.0);   // horizontal
    const cwTurnTableViewState topDown =
        f.interaction.framingViewState(tall, 0.0, 90.0);  // straight down

    CHECK(profile.distance > topDown.distance);
}

TEST_CASE("cwBaseTurnTableInteraction framingViewState(box, az, pitch) null box returns current viewState",
          "[cwBaseTurnTableInteraction]")
{
    // Matching the 1-arg overload's contract: when the box is invalid,
    // return the current viewState unchanged — the orientation override
    // does not apply because there's nothing to fit.
    Fixture f;
    f.interaction.setAzimuth(10.0);
    f.interaction.setPitch(80.0);
    const cwTurnTableViewState before = f.interaction.viewState();

    const cwTurnTableViewState got =
        f.interaction.framingViewState(QBox3D(), 30.0, 0.0);

    CHECK(got.azimuth == Approx(before.azimuth));
    CHECK(got.pitch == Approx(before.pitch));
    CHECK(got.distance == Approx(before.distance).margin(kMatrixEps));
}

TEST_CASE("cwBaseTurnTableInteraction resetView frames the whole scene at the default orientation",
          "[cwBaseTurnTableInteraction]")
{
    // Issue #549: a model whose datum places it far from the origin must be
    // brought back into view by reset, not left off-screen. resetView() frames
    // the geometry intersecter's root bounding box, animating there.
    Fixture f;
    const QVector3D sceneCenter(500.0f, -300.0f, 0.0f);
    addGeometryAt(f.scene, sceneCenter, 20.0f);

    // Rotate away from default so we can verify the orientation is restored.
    f.interaction.setAzimuth(75.0);
    f.interaction.setPitch(45.0);

    // Drive the animation to completion the same way the centerOn(true) test
    // does: drain viewMatrixChanged ticks until the spy goes quiet.
    QSignalSpy spy(&f.camera, &cwCamera::viewMatrixChanged);
    f.interaction.resetView();
    REQUIRE(spy.wait(kAnimationFirstTickTimeoutMs));
    while (spy.wait(kAnimationQuietWindowMs)) {
        // drain remaining animation ticks
    }

    CHECK(f.interaction.azimuth() == Approx(0.0));
    CHECK(f.interaction.pitch() == Approx(90.0));

    const QBox3D sceneBox = f.scene.geometryItersecter()->boundingBox();
    QPointF projected = f.camera.project(sceneBox.center());
    QPointF expected = screenCenter();
    CHECK(projected.x() == Approx(expected.x()).margin(kPixelTolerance));
    CHECK(projected.y() == Approx(expected.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction resetView on an empty scene falls back to the default pose",
          "[cwBaseTurnTableInteraction]")
{
    // With no geometry there is nothing to frame, so resetView() falls back to
    // the fixed default pose (a plain T(0,0,kDefaultViewZ)) synchronously.
    Fixture f;
    f.interaction.setAzimuth(75.0);
    f.interaction.setPitch(45.0);

    f.interaction.resetView();

    CHECK(f.interaction.azimuth() == Approx(0.0));
    CHECK(f.interaction.pitch() == Approx(90.0));

    QMatrix4x4 expected;
    expected.translate(QVector3D(0.0f, 0.0f, kDefaultViewZ));
    CHECK(matricesNearlyEqual(f.camera.viewMatrix(), expected));
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

// --- Issue #527: missed intersection must not teleport the view ---

TEST_CASE("cwBaseTurnTableInteraction startRotating on empty space far from geometry keeps the pivot",
          "[cwBaseTurnTableInteraction]")
{
    // Geometry sits in a tiny box near the origin. The grid plane is infinite,
    // so an off-center click misses the geometry but still hits the grid plane
    // far away. The old code teleported the pivot there; now it must keep the
    // current center. Every point is far outside the #562 anchor radius, so
    // the nearest-geometry fallback finds nothing, and once geometry exists
    // the grid is not a pivot source: the missed click leaves the pivot where
    // it was (#527). (Contrast the near case below, where a point inside the
    // anchor radius re-anchors the pivot.)
    Fixture f;
    addSmallGeometry(f.scene, 1.0f);

    // Move the pivot off the origin first: a default-constructed m_center is
    // (0,0,0), so "kept" and "reset to the origin" would be the same assertion.
    f.interaction.setCenter(QVector3D(5.0f, 6.0f, 7.0f));
    const QVector3D centerBefore = f.interaction.center();

    // Precondition: the grid hit this click produces is far from the geometry,
    // so it is the teleport the fix must suppress.
    const QVector3D farGridHit = unprojectClickOntoGrid(f.camera, kOffCenterClick);
    REQUIRE(farGridHit.length() > 10.0f);

    f.interaction.startRotating(kOffCenterClick);

    CHECK(f.interaction.center().x() == Approx(centerBefore.x()));
    CHECK(f.interaction.center().y() == Approx(centerBefore.y()));
    CHECK(f.interaction.center().z() == Approx(centerBefore.z()));
}

TEST_CASE("cwBaseTurnTableInteraction startRotating near geometry anchors the pivot to the nearest geometry point",
          "[cwBaseTurnTableInteraction]")
{
    // #562: once survey geometry exists, a near-miss click no longer holds the
    // old pivot — it re-anchors to the closest point ON geometry within the
    // anchor radius, so the rotate keeps its context. It must still never fall
    // to the grid.
    //
    // One cloud point sits 10 units off the ray — far beyond the exact pick's
    // reach (its 0.5 pick radius) but inside the anchor radius. The
    // screen-center click misses it exactly, yet the pivot snaps to the point
    // itself — the z proves it landed on the geometry, not the z = 0 grid
    // plane and not the z = 0 default pivot it would have held before.
    Fixture f;

    const double anchorRadius = anchorRadiusWorld(f);
    REQUIRE(anchorRadius > 12.0);  // the 10-unit offset must be inside reach

    const QVector3D cloudPoint(10.0f, 0.0f, kCenterlineZ);
    addSinglePoint(f.scene, cloudPoint);

    f.interaction.startRotating(screenCenter().toPoint());

    CHECK(f.interaction.center().x() == Approx(cloudPoint.x()).margin(kPixelTolerance));
    CHECK(f.interaction.center().y() == Approx(cloudPoint.y()).margin(kPixelTolerance));
    CHECK(f.interaction.center().z() == Approx(cloudPoint.z()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction startRotating in a point-cloud gap keeps the pivot",
          "[cwBaseTurnTableInteraction]")
{
    // The point-cloud regression on #562's original node-center anchor: a
    // LiDAR cloud is ONE intersecter Object, so a click down the gap between
    // two far-apart clusters used to anchor the whole-cloud centroid — a point
    // in empty space at the wrong depth — teleporting the view ("can't rotate
    // around a point cloud point"). The nearest-point anchor has no aggregate
    // candidates: with every real point outside the anchor radius, the gap
    // click keeps the pivot.
    Fixture f;
    REQUIRE(anchorRadiusWorld(f) < 60.0);  // clusters sit ~70 units off the ray

    QVector<QVector3D> points;
    const auto addCluster = [&points](const QVector3D& center) {
        for (int zi = 0; zi < 2; ++zi) {
            for (int yi = -1; yi <= 1; ++yi) {
                for (int xi = -1; xi <= 1; ++xi) {
                    points.append(center
                                  + QVector3D(2.0f * xi, 2.0f * yi, 2.0f * zi));
                }
            }
        }
    };
    addCluster(QVector3D(-50.0f, -50.0f, 40.0f));
    addCluster(QVector3D(50.0f, 50.0f, -40.0f));

    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    geometry.setType(cwGeometry::Type::Points);
    f.scene.geometryItersecter()->addObject(
        cwGeometryItersecter::Object({nullptr, 4}, geometry, QMatrix4x4(), 0.5f));
    f.scene.geometryItersecter()->waitForFinish();

    const QVector3D pivotBefore(5.0f, 6.0f, 7.0f);
    f.interaction.setCenter(pivotBefore);

    f.interaction.startRotating(screenCenter().toPoint());

    CHECK(f.interaction.center().x() == Approx(pivotBefore.x()));
    CHECK(f.interaction.center().y() == Approx(pivotBefore.y()));
    CHECK(f.interaction.center().z() == Approx(pivotBefore.z()));
}

TEST_CASE("cwBaseTurnTableInteraction startRotating does not anchor the pivot to a hidden object",
          "[cwBaseTurnTableInteraction]")
{
    // The nearest-point fallback skips hidden objects (isPickable guard): a
    // hidden cloud must never anchor the pivot. Same click and point as the
    // anchor test above, but the point's parent is invisible.
    //
    // Every object here is hidden, so nothing is pickable and the scene reads
    // as empty — the grid comes back as the last-resort pivot (isPickableEmpty),
    // which is the only way out: with the grid gated off the pivot could never
    // be moved again while the geometry stays hidden. What must NOT happen is
    // the pivot landing on the hidden point.
    //
    // `hidden` outlives the fixture so the intersecter never holds a key to a
    // destroyed object.
    cwRenderObject hidden;
    Fixture f;
    hidden.setVisible(false);
    addSinglePoint(f.scene, QVector3D(10.0f, 0.0f, kCenterlineZ), &hidden);

    f.interaction.setCenter(QVector3D(5.0f, 6.0f, 7.0f));
    f.interaction.startRotating(screenCenter().toPoint());

    // The grid plane is z = 0 through the origin and the click is dead centre,
    // so the rescue lands on the origin — distinct from both the prior pivot
    // and the hidden point at (10, 0, 10).
    CHECK(f.interaction.center().x() == Approx(0.0f).margin(1e-3));
    CHECK(f.interaction.center().y() == Approx(0.0f).margin(1e-3));
    CHECK(f.interaction.center().z() == Approx(0.0f).margin(1e-3));
}

TEST_CASE("cwBaseTurnTableInteraction startRotating keeps the grid gated off while any object stays visible",
          "[cwBaseTurnTableInteraction]")
{
    // The counterpart to the all-hidden rescue above: hiding SOME geometry
    // must not re-open the grid. A visible object exists, so a click that
    // reaches nothing keeps the pivot rather than teleporting to the grid
    // (#562) — the grid only returns when nothing at all is pickable.
    cwRenderObject hidden;
    Fixture f;
    hidden.setVisible(false);
    addSinglePoint(f.scene, QVector3D(10.0f, 0.0f, kCenterlineZ), &hidden);

    // A visible object far outside the anchor's reach of the centre click.
    addSinglePoint(f.scene, QVector3D(400.0f, 400.0f, kCenterlineZ), nullptr, 4);

    const QVector3D centerBefore(5.0f, 6.0f, 7.0f);
    f.interaction.setCenter(centerBefore);
    f.interaction.startRotating(screenCenter().toPoint());

    CHECK(f.interaction.center().x() == Approx(centerBefore.x()));
    CHECK(f.interaction.center().y() == Approx(centerBefore.y()));
    CHECK(f.interaction.center().z() == Approx(centerBefore.z()));
}

TEST_CASE("cwBaseTurnTableInteraction startRotating on a long shot's far end pivots on the exact hit, not the node center",
          "[cwBaseTurnTableInteraction]")
{
    // Exact-hit stays primary over the #562 nearest-geometry fallback:
    // clicking the far end of a long survey shot must pivot on the point under
    // the cursor, not any aggregate of the segment (a bbox-center-first policy
    // would swing the pivot ~half the shot length to the midpoint).
    Fixture f;
    addCenterline(f.scene);   // (-20,0,z) .. (20,0,z), one long segment

    // Click the projected far end of the segment.
    const QVector3D farEnd(18.0f, 0.0f, kCenterlineZ);
    const QPoint click = f.camera.project(farEnd).toPoint();

    f.interaction.startRotating(click);

    // Lands on the exact closest point (~farEnd), not the midpoint (0, 0, z).
    CHECK(f.interaction.center().x() == Approx(18.0f).margin(kPixelTolerance));
    CHECK(f.interaction.center().y() == Approx(0.0f).margin(kPixelTolerance));
    CHECK(f.interaction.center().z() == Approx(kCenterlineZ).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction startPanning on a near-miss does not jump the view",
          "[cwBaseTurnTableInteraction]")
{
    // #562 regression: the nearest-geometry pivot anchor is OFF the cursor ray,
    // so it must not become the pan anchor — startPanning would build the pan
    // plane through an off-ray point and the initial pan tick would lurch the
    // view. Pan uses the on-ray unProject (exact hit / gated grid / nullopt);
    // on a near-miss with geometry present the anchor resolves to the cursor
    // ray at the current center depth, so the view does not move when panning
    // begins.
    //
    // The cloud spans the off-center click's ray but has no point directly under
    // it, so the click misses geometry while the rotation-only anchor path
    // would have produced an off-ray anchor. startPanning runs the first pan
    // tick synchronously (translateLastPosition), so any jump shows up here.
    const float extent = 105.0f;

    SECTION("ortho") {
        Fixture f;
        addSmallGeometry(f.scene, extent);
        const QMatrix4x4 before = f.camera.viewMatrix();
        f.interaction.startPanning(kOffCenterClick);
        CHECK(matricesNearlyEqual(f.camera.viewMatrix(), before));
    }

    SECTION("perspective") {
        Fixture f;
        f.camera.setProjection(f.camera.perspectiveProjectionDefault());
        addSmallGeometry(f.scene, extent);
        const QMatrix4x4 before = f.camera.viewMatrix();
        f.interaction.startPanning(kOffCenterClick);
        CHECK(matricesNearlyEqual(f.camera.viewMatrix(), before));
    }
}

TEST_CASE("cwBaseTurnTableInteraction startRotating that hits geometry re-centers on the hit",
          "[cwBaseTurnTableInteraction]")
{
    // A click at screen center looks straight down the default ortho view onto
    // the geometry at the origin: a real pick, so the pivot moves to it.
    Fixture f;
    addSmallGeometry(f.scene, 5.0f);

    f.interaction.startRotating(screenCenter().toPoint());

    CHECK(f.interaction.center().x() == Approx(0.0f).margin(kPixelTolerance));
    CHECK(f.interaction.center().y() == Approx(0.0f).margin(kPixelTolerance));
    CHECK(f.interaction.center().z() == Approx(0.0f).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction startRotating with no geometry still uses the grid plane",
          "[cwBaseTurnTableInteraction]")
{
    // With nothing loaded there is nothing to teleport away from, so the grid
    // plane remains the pivot source (original behavior preserved).
    Fixture f;

    const QVector3D expected = unprojectClickOntoGrid(f.camera, kOffCenterClick);
    f.interaction.startRotating(kOffCenterClick);

    CHECK(f.interaction.center().x() == Approx(expected.x()));
    CHECK(f.interaction.center().y() == Approx(expected.y()));
    CHECK(f.interaction.center().z() == Approx(expected.z()));
}

// --- Pivoting on the centerline when the solid geometry is hidden ---

TEST_CASE("cwBaseTurnTableInteraction pick snaps to the centerline with no solid geometry",
          "[cwBaseTurnTableInteraction]")
{
    // The pivot pick carries a screen-space tolerance so the otherwise
    // infinitely-thin centerline is hittable. Without it (a bare default query)
    // the line would never be picked and pick() would miss.
    Fixture f;
    addCenterline(f.scene);

    const cwRayHit hit = f.interaction.pick(screenCenter());
    REQUIRE(hit.hit());
    // The screen-center ray runs down -Z through the world origin and crosses
    // the line at (0, 0, kCenterlineZ); the closest point on the segment is
    // exactly that crossing — z≈kCenterlineZ proves it's the line, not the grid.
    CHECK(hit.pointWorld().x() == Approx(0.0f).margin(kPixelTolerance));
    CHECK(hit.pointWorld().y() == Approx(0.0f).margin(kPixelTolerance));
    CHECK(hit.pointWorld().z() == Approx(kCenterlineZ).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction startRotating orbits the centerline with no solid geometry",
          "[cwBaseTurnTableInteraction]")
{
    // startRotating() routes through unProject(). With only the centerline in
    // the scene, the pivot must land on the line (z≈kCenterlineZ) rather than
    // fall through to the z=0 grid plane — that's "orbit the line plot when the
    // solid is hidden".
    Fixture f;
    addCenterline(f.scene);

    f.interaction.startRotating(screenCenter().toPoint());

    CHECK(f.interaction.center().x() == Approx(0.0f).margin(kPixelTolerance));
    CHECK(f.interaction.center().y() == Approx(0.0f).margin(kPixelTolerance));
    CHECK(f.interaction.center().z() == Approx(kCenterlineZ).margin(kPixelTolerance));
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

TEST_CASE("cwBaseTurnTableInteraction viewState captures eyeOffset after pan",
          "[cwBaseTurnTableInteraction]")
{
    // A user pan applies viewMatrix.translate() — m_center stays put, but
    // the canonical recipe alone can no longer reproduce the resulting
    // view. The eyeOffset channel exists exactly to carry that delta so a
    // viewState() snapshot remains round-trippable.
    Fixture f;

    // Establish a known canonical state — setViewState rebuilds the view
    // matrix so eyeOffset starts at zero.
    cwTurnTableViewState canon;
    canon.center = QVector3D(5.0f, 7.0f, 1.0f);
    canon.azimuth = 0.0;
    canon.pitch = 90.0;
    canon.distance = 50.0;
    canon.zoomScale = f.camera.defaultZoomScale();
    f.interaction.setViewState(canon);

    const cwTurnTableViewState before = f.interaction.viewState();
    REQUIRE(before.eyeOffset.x() == Approx(0.0).margin(kMatrixEps));
    REQUIRE(before.eyeOffset.y() == Approx(0.0).margin(kMatrixEps));

    // Synthesize a pan: shift the view matrix sideways by a known
    // world-space delta (no mouse-event plumbing in this fixture).
    QMatrix4x4 view = f.camera.viewMatrix();
    view.translate(QVector3D(2.0f, -1.0f, 0.0f));
    f.camera.setViewMatrix(view);

    const cwTurnTableViewState after = f.interaction.viewState();
    // Pan moves m_center off the canonical screen-centre — eyeOffset
    // captures the eye-space XY shift. The Z component stays at zero by
    // construction (distance = -centerView.z() makes it cancel).
    const QVector3D centerView = f.camera.viewMatrix().map(f.interaction.center());
    CHECK(after.eyeOffset.x() == Approx(centerView.x()).margin(kMatrixEps));
    CHECK(after.eyeOffset.y() == Approx(centerView.y()).margin(kMatrixEps));
    CHECK(after.eyeOffset.z() == Approx(0.0).margin(kMatrixEps));
}

TEST_CASE("cwBaseTurnTableInteraction setViewState reproduces a panned view",
          "[cwBaseTurnTableInteraction]")
{
    // Round-trip: read the panned state, snap it back, and the view
    // matrix must agree to float epsilon. Without the eyeOffset channel
    // setViewState would silently snap m_center back to screen-centre
    // and the matrix would differ.
    Fixture f;
    f.interaction.setCenter(QVector3D(3.0f, -2.0f, 0.0f));

    QMatrix4x4 view = f.camera.viewMatrix();
    view.translate(QVector3D(1.5f, 0.75f, 0.0f));
    f.camera.setViewMatrix(view);

    const QMatrix4x4 expected = f.camera.viewMatrix();
    const cwTurnTableViewState snap = f.interaction.viewState();

    // Snap to a different state, then back, to prove the eyeOffset channel
    // is what makes the second snap reproduce the first.
    cwTurnTableViewState detour;
    detour.center = QVector3D(0, 0, 0);
    detour.azimuth = 0.0;
    detour.pitch = 90.0;
    detour.distance = 50.0;
    detour.zoomScale = f.camera.defaultZoomScale();
    f.interaction.setViewState(detour);

    f.interaction.setViewState(snap);

    CHECK(matricesNearlyEqual(f.camera.viewMatrix(), expected));
}

TEST_CASE("cwBaseTurnTableInteraction eyeOffset preserves orbit around m_center",
          "[cwBaseTurnTableInteraction]")
{
    // The whole point of eyeOffset (vs. re-anchoring m_center) is that the
    // rotation pivot stays put. Set up a state with non-zero eyeOffset,
    // rotate, and check m_center's screen position is invariant under
    // rotation.
    Fixture f;

    cwTurnTableViewState s;
    s.center = QVector3D(4.0f, 4.0f, 0.0f);
    s.azimuth = 30.0;
    s.pitch = 70.0;
    s.distance = 60.0;
    s.zoomScale = f.camera.defaultZoomScale();
    s.eyeOffset = QVector3D(8.0f, -5.0f, 0.0f); // user has panned
    f.interaction.setViewState(s);

    const QPointF before = f.camera.project(s.center);

    // Rotate via the public setter; orbit pivot must stay on m_center.
    f.interaction.setAzimuth(75.0);

    const QPointF after = f.camera.project(s.center);
    CHECK(after.x() == Approx(before.x()).margin(kPixelTolerance));
    CHECK(after.y() == Approx(before.y()).margin(kPixelTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction animateToViewState interpolates eyeOffset",
          "[cwBaseTurnTableInteraction]")
{
    // The bug this channel fixes: when an enter animation starts after a
    // user pan, the animator must interpolate FROM the panned view, not
    // snap back to canonical at t=0. The eyeOffset channel carries the
    // pan into m_stateAnimStart so the t=0 frame lands on the current
    // view matrix.
    Fixture f;

    cwTurnTableViewState start;
    start.center = QVector3D(0, 0, 0);
    start.azimuth = 0.0;
    start.pitch = 90.0;
    start.distance = 50.0;
    start.zoomScale = 1.0;
    start.eyeOffset = QVector3D(10.0f, -4.0f, 0.0f);
    f.interaction.setViewState(start);

    cwTurnTableViewState target = start;
    target.eyeOffset = QVector3D(0.0f, 0.0f, 0.0f);
    f.interaction.animateToViewState(target, 1000);

    auto* anim = f.interaction.findChild<QVariantAnimation*>(
                cwBaseTurnTableInteraction::stateAnimationObjectName);
    REQUIRE(anim != nullptr);
    anim->setCurrentTime(anim->duration() / 2);

    cwTurnTableViewState mid = f.interaction.viewState();
    // Midpoint = (start + target) / 2 = (5, -2, 0).
    CHECK(mid.eyeOffset.x() == Approx(5.0).margin(kMatrixEps));
    CHECK(mid.eyeOffset.y() == Approx(-2.0).margin(kMatrixEps));
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

// --- Projection-swap zoom reconcile (issue #513) ---
//
// reconcileZoomForProjection is the pure service cwProjectionTransition calls
// at the start of a transition. The on-screen-scale-preservation behaviour
// (driving the whole morph through the controller) lives in
// test_cwProjectionTransition.cpp; here we unit-test the channel conversion.

namespace {
constexpr double kPerspectiveFovYRadians = kPerspectiveFovYDegrees * M_PI / 180.0;
}

TEST_CASE("cwBaseTurnTableInteraction reconcileZoomForProjection(true) sets the matched eye distance",
          "[cwBaseTurnTableInteraction]")
{
    // ortho->perspective: the eye distance is set from the ortho zoom so the
    // visible half-height (distance * tan(fov/2)) matches the ortho box
    // (viewportHeight/2 * zoomScale). zoomScale, the source channel, is left
    // untouched.
    Fixture f;

    cwTurnTableViewState s;
    s.center = QVector3D(0.0f, 0.0f, 0.0f);
    s.azimuth = 0.0;
    s.pitch = 90.0;
    s.distance = 50.0;
    s.zoomScale = 0.8;
    f.interaction.setViewState(s);

    f.interaction.reconcileZoomForProjection(true, kPerspectiveFovYRadians);

    const double expectedDist =
            matchedPerspectiveDistance(s.zoomScale, kPerspectiveFovYDegrees, kViewportHeight);
    const QVector3D centerView = f.camera.viewMatrix().map(s.center);
    CHECK(centerView.z() == Approx(-expectedDist).margin(kMatrixEps));
    CHECK(f.camera.zoomScale() == Approx(s.zoomScale));
}

TEST_CASE("cwBaseTurnTableInteraction reconcileZoomForProjection(false) sets the matched zoom scale",
          "[cwBaseTurnTableInteraction]")
{
    // perspective->ortho: the ortho zoom is set from the eye distance. The
    // distance (view-matrix depth), the source channel, is left untouched.
    Fixture f;
    f.camera.setProjection(f.camera.perspectiveProjectionDefault());

    cwTurnTableViewState s;
    s.center = QVector3D(0.0f, 0.0f, 0.0f);
    s.azimuth = 0.0;
    s.pitch = 90.0;
    s.distance = 300.0;
    s.zoomScale = f.camera.defaultZoomScale();
    f.interaction.setViewState(s);

    f.interaction.reconcileZoomForProjection(false, kPerspectiveFovYRadians);

    const double expectedZoom =
            matchedOrthoZoom(s.distance, kPerspectiveFovYDegrees, kViewportHeight);
    CHECK(f.camera.zoomScale() == Approx(expectedZoom).margin(kMatrixEps));
    const QVector3D centerView = f.camera.viewMatrix().map(s.center);
    CHECK(centerView.z() == Approx(-s.distance).margin(kMatrixEps));
}

TEST_CASE("cwBaseTurnTableInteraction reconcileZoomForProjection honors a non-default field of view",
          "[cwBaseTurnTableInteraction]")
{
    // A wider fov needs a larger eye distance to hold the same on-screen scale.
    Fixture f;

    cwTurnTableViewState s;
    s.center = QVector3D(0.0f, 0.0f, 0.0f);
    s.azimuth = 0.0;
    s.pitch = 90.0;
    s.distance = 50.0;
    s.zoomScale = 0.5;
    f.interaction.setViewState(s);

    constexpr double kWideFovDegrees = 80.0;
    f.interaction.reconcileZoomForProjection(true, kWideFovDegrees * M_PI / 180.0);

    const double expectedDist =
            matchedPerspectiveDistance(s.zoomScale, kWideFovDegrees, kViewportHeight);
    const QVector3D centerView = f.camera.viewMatrix().map(s.center);
    CHECK(centerView.z() == Approx(-expectedDist).margin(kMatrixEps));
}

TEST_CASE("cwBaseTurnTableInteraction reconcileZoomForProjection round-trips zoomScale",
          "[cwBaseTurnTableInteraction]")
{
    // The two directions are exact inverses, so converting to perspective and
    // back returns the ortho zoom to its starting value.
    Fixture f;

    cwTurnTableViewState s;
    s.center = QVector3D(2.0f, -3.0f, 0.0f);
    s.azimuth = 0.0;
    s.pitch = 90.0;
    s.distance = 50.0;
    s.zoomScale = 1.35;
    f.interaction.setViewState(s);

    const double zoomBefore = f.camera.zoomScale();

    f.interaction.reconcileZoomForProjection(true, kPerspectiveFovYRadians);
    f.interaction.reconcileZoomForProjection(false, kPerspectiveFovYRadians);

    CHECK(f.camera.zoomScale() == Approx(zoomBefore).margin(kMatrixEps));
}
