//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Std
#include <algorithm>
#include <limits>

// Qt
#include <QMatrix4x4>
#include <QRay3D>
#include <QRect>
#include <QVector3D>

// SUT
#include "cwBaseTurnTableInteraction.h"
#include "cwCamera.h"
#include "cwGeometry.h"
#include "cwGeometryItersecter.h"
#include "cwPickQuery.h"
#include "cwRayHit.h"
#include "cwRenderPointCloud.h"
#include "cwScene.h"

using namespace Catch;

namespace {

// Matches the fixture in test_cwBaseTurnTableInteraction.cpp. Ortho at the
// default ZoomScale of 0.5 (cwCamera::defaultZoomScale) spans [-200,200] x
// [-150,150] with near=-10000 / far=10000 — the same projection shape a real
// ortho session reports. Nothing here hardcodes that span: the distances below
// are bounded against anchorRadiusWorld() instead, so a retuned zoom fails
// loudly rather than going vacuous.
constexpr int kViewportWidth = 800;
constexpr int kViewportHeight = 600;

// Slack for comparing a pivot against the world point it should have landed
// on. Generous enough to absorb the unproject round-trip, far tighter than
// the distances any of these tests need to tell apart.
constexpr double kPivotTolerance = 0.25;

// Depths along the screen-center ray. The camera looks down -Z, so a LARGER z
// is NEARER the eye; each test REQUIREs that so the naming can't silently
// invert under a camera change.
constexpr float kNearZ = 10.0f;
constexpr float kFarZ = -200.0f;

// Mirrors the anonymous-namespace kPivotPickRadiusMillimeters in
// cwBaseTurnTableInteraction.cpp, which is unreachable from here. If that one
// is retuned this stops matching the exact rung under test — but no assertion
// below depends on the value, only on it being far under the anchor radius.
constexpr double kPivotPickRadiusMillimeters = 4.0;

// Lateral offset of the scrap from the screen-center ray. Must sit outside any
// exact reach but inside the 20mm anchor radius (asserted per-test). Triangles
// are picked by an exact rayTriangleMT test that never consults the tolerance,
// so ANY nonzero offset means no triangle candidate exists for the exact rung
// — this models "clicked next to the scrap and missed it".
constexpr float kScrapOffsetX = 5.0f;

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
        interaction.setGridPlane(QPlane3D(QVector3D(0, 0, 0), QVector3D(0, 0, 1)));
        interaction.setScene(&scene);
    }
};

QPoint screenCenter()
{
    return QPoint(kViewportWidth / 2, kViewportHeight / 2);
}

// World radius of the rotation pivot's 20mm anchor under this ortho camera,
// derived through the same pixelsForMillimeters + pickQuery pipeline unProject
// uses. Tests bound their hardcoded offsets against it so a retuned
// PivotAnchorRadiusMillimeters fails loudly instead of going vacuous.
double anchorRadiusWorld(const Fixture& f)
{
    return f.camera.pickQuery(
        f.interaction.pixelsForMillimeters(
            cwBaseTurnTableInteraction::PivotAnchorRadiusMillimeters))
        .tolerance.constant;
}

// A scrap, registered the way cwRenderTexturedItems does: Type::Triangles with
// an index list and no pick radius. LiDAR notes take this same path.
void addScrapTriangle(cwScene& scene, float offsetX, float z, uint64_t id = 1)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    // The edge nearest the ray runs along x = offsetX, so the closest point on
    // the scrap to a screen-center ray is (offsetX, 0, z).
    geometry.set(cwGeometry::Semantic::Position, QVector<QVector3D>{
        QVector3D(offsetX, -20.0f, z),
        QVector3D(offsetX,  20.0f, z),
        QVector3D(offsetX + 40.0f, 0.0f, z),
    });
    geometry.setIndices(QVector<uint32_t>{0u, 1u, 2u});
    geometry.setType(cwGeometry::Type::Triangles);

    scene.geometryItersecter()->addObject(
        cwGeometryItersecter::Object(nullptr, id, geometry));
    scene.geometryItersecter()->waitForFinish();
}

// One cloud point sitting exactly on the screen-center ray. Dead-centre, so it
// is always an EXACT sphere hit whatever the radius — which is the rung that
// short-circuited the whole ladder before this fix. The 0.5f radius is picked
// only to make that hit unambiguous; it does NOT model a production cloud
// (those derive it from meanSpacingXY * PointPickRadiusScale), and no test
// here depends on it doing so.
void addCloudPointOnRay(cwScene& scene, float z, uint64_t id = 2)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position,
                 QVector<QVector3D>{QVector3D(0.0f, 0.0f, z)});
    geometry.setType(cwGeometry::Type::Points);

    scene.geometryItersecter()->addObject(
        cwGeometryItersecter::Object(nullptr, id, geometry, QMatrix4x4(), 0.5f));
    scene.geometryItersecter()->waitForFinish();
}

// Guards the sign convention every depth claim in this file rests on.
void requireLookingDownNegativeZ(const Fixture& f)
{
    const QRay3D ray = f.camera.frustrumRay(f.camera.mapToGLViewport(screenCenter()));
    REQUIRE(ray.direction().z() < 0.0f);
}

// Spacing of the synthetic wall below. Any value works — every distance in the
// slip test is expressed as a multiple of it — so this is just a round number.
constexpr float kWallSpacing = 1.0f;

// sqrt(2)/2: a grid cell's centre-to-corner distance, in spacings. Spelled out
// rather than M_SQRT1_2, which is not standard C++ (MSVC gates it behind
// _USE_MATH_DEFINES, which this project never sets).
constexpr double kCellCentreDistanceScale = 0.70710678;

// Perpendicular distance from `ray` to the nearest wall point, rebuilt from the
// same rule addPointWall uses. Lets a test assert the cell-centre premise
// rather than trusting the grid arithmetic to stay true.
double nearestWallPointDistance(const QRay3D& ray, float z)
{
    double best = std::numeric_limits<double>::max();
    constexpr int kHalfExtent = 6;
    for (int yi = -kHalfExtent; yi < kHalfExtent; ++yi) {
        for (int xi = -kHalfExtent; xi < kHalfExtent; ++xi) {
            const QVector3D point((float(xi) + 0.5f) * kWallSpacing,
                                  (float(yi) + 0.5f) * kWallSpacing,
                                  z);
            best = std::min(best, double(ray.distance(point)));
        }
    }
    return best;
}

// A flat wall of points on a regular grid at z, deliberately straddling the
// screen-center ray: with an even count either side of zero, no point sits at
// x=y=0, so the ray passes exactly through a CELL CENTRE — the worst case, at
// kWallSpacing*sqrt(2)/2 ~= 0.707 spacings from the four nearest points. This
// is the geometry the drawn splats cover but the pick spheres may not.
void addPointWall(cwScene& scene, float z, float pickRadius, uint64_t id = 3)
{
    QVector<QVector3D> points;
    constexpr int kHalfExtent = 6;
    for (int yi = -kHalfExtent; yi < kHalfExtent; ++yi) {
        for (int xi = -kHalfExtent; xi < kHalfExtent; ++xi) {
            points.append(QVector3D((float(xi) + 0.5f) * kWallSpacing,
                                    (float(yi) + 0.5f) * kWallSpacing,
                                    z));
        }
    }

    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    geometry.setType(cwGeometry::Type::Points);

    scene.geometryItersecter()->addObject(
        cwGeometryItersecter::Object(nullptr, id, geometry, QMatrix4x4(), pickRadius));
    scene.geometryItersecter()->waitForFinish();
}

} // namespace

// The threshold invariant itself is pinned by a static_assert beside the
// constant in cwRenderPointCloud.h — a compile-time check no tag filter can
// skip. What follows is that threshold made observable: why crossing it
// actually loses a pick.
TEST_CASE("cwGeometryItersecter does not pick through a point wall to a far point",
          "[cwGeometryItersecter][cwRenderPointCloud][pointCloudPivot]")
{
    // The slip-through. A wall of points the splats draw as solid sits near
    // the eye; one lone point sits far down the ray. The ray threads the wall's
    // cell centre, so whether the wall is hit at all comes down to the pick
    // radius. Under a radius below the watertight threshold every wall sphere
    // is missed and the far point — invisible on screen, buried behind the
    // wall — becomes the only hit and takes the pivot.
    Fixture f;
    requireLookingDownNegativeZ(f);

    const float pickRadius = kWallSpacing * cwRenderPointCloud::PointPickRadiusScale;
    addPointWall(f.scene, kNearZ, pickRadius);
    addCloudPointOnRay(f.scene, kFarZ);

    const QRay3D ray = f.camera.frustrumRay(f.camera.mapToGLViewport(screenCenter()));

    // Pin the premise, not just the sign of the ray. Everything here rests on
    // the ray threading a CELL CENTRE — the worst case, sqrt(2)/2 spacings from
    // the four nearest points. A view matrix that ever gains an x/y offset
    // would slide the ray onto a point, making it an exact hit at ANY radius
    // and turning this into a test that passes while guarding nothing.
    REQUIRE(nearestWallPointDistance(ray, kNearZ)
            == Approx(kWallSpacing * kCellCentreDistanceScale).margin(1e-4));

    const cwRayHit hit = f.scene.geometryItersecter()->intersectsDetailed(
        ray, f.camera.pickQuery(
            f.interaction.pixelsForMillimeters(kPivotPickRadiusMillimeters)));

    REQUIRE(hit.hit());
    CHECK(hit.pointWorld().z() == Approx(kNearZ).margin(kPivotTolerance));
}


TEST_CASE("cwBaseTurnTableInteraction startRotating prefers a near-missed scrap "
          "over a far point-cloud hit",
          "[cwBaseTurnTableInteraction][pointCloudPivot]")
{
    // The reported bug. Clicking just beside a scrap with the cloud turned on
    // pivots onto a far cloud point instead of the scrap the user aimed at.
    //
    // Why it happens: triangles are exact-only, so a near-miss yields no
    // triangle candidate at all. The cloud, being everywhere, still produces an
    // exact sphere hit somewhere far down the ray. unProject returns that at
    // rung 1 and short-circuits, so the 20mm anchor that would have found the
    // scrap never runs.
    Fixture f;
    requireLookingDownNegativeZ(f);

    REQUIRE(anchorRadiusWorld(f) > double(kScrapOffsetX));  // scrap inside anchor reach

    addScrapTriangle(f.scene, kScrapOffsetX, kNearZ);
    addCloudPointOnRay(f.scene, kFarZ);

    f.interaction.startRotating(screenCenter());

    // The scrap is nearer than the cloud and only barely missed, so the pivot
    // belongs on its closest point.
    const QVector3D expected(kScrapOffsetX, 0.0f, kNearZ);
    CHECK(f.interaction.center().x() == Approx(expected.x()).margin(kPivotTolerance));
    CHECK(f.interaction.center().y() == Approx(expected.y()).margin(kPivotTolerance));
    CHECK(f.interaction.center().z() == Approx(expected.z()).margin(kPivotTolerance));
}

TEST_CASE("cwBaseTurnTableInteraction startRotating pivots on the point cloud "
          "when it occludes the scrap",
          "[cwBaseTurnTableInteraction][pointCloudPivot]")
{
    // The other half of the rule, and the reason this can't be a blanket
    // "scraps always beat the cloud" priority: when the cloud is what you are
    // actually looking AT, with the scrap behind it, the pivot must stay on the
    // cloud. Preferring the occluded scrap here would orbit geometry the user
    // cannot see — a new teleport traded for the old one.
    //
    // This passes today (the exact cloud hit wins at rung 1) and must keep
    // passing after the near-missed-scrap case above is fixed.
    Fixture f;
    requireLookingDownNegativeZ(f);

    REQUIRE(anchorRadiusWorld(f) > double(kScrapOffsetX));  // scrap is in anchor reach...

    addScrapTriangle(f.scene, kScrapOffsetX, kFarZ);  // ...but behind the cloud
    addCloudPointOnRay(f.scene, kNearZ);

    f.interaction.startRotating(screenCenter());

    const QVector3D expected(0.0f, 0.0f, kNearZ);
    CHECK(f.interaction.center().x() == Approx(expected.x()).margin(kPivotTolerance));
    CHECK(f.interaction.center().y() == Approx(expected.y()).margin(kPivotTolerance));
    CHECK(f.interaction.center().z() == Approx(expected.z()).margin(kPivotTolerance));
}
