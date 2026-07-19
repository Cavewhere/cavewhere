// Pins cwLeadView::isOccluded — the gate LeadPoint.qml uses to decide whether a
// tap on a lead marker counts. A lead renders as an overlay billboard regardless
// of depth, so without this gate you could click a lead through a cave wall.
//
// isOccluded needs only a camera and a scene, so it runs here on the GUI thread
// with no window, QRhi, or QML engine: the BVH is CPU-side and the billboard
// plane is solved analytically.

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Qt
#include <QMatrix4x4>
#include <QPointF>
#include <QRay3D>
#include <QRect>
#include <QVector3D>

// SUT
#include "cwCamera.h"
#include "cwGeometry.h"
#include "cwGeometryItersecter.h"
#include "cwLeadView.h"
#include "cwScene.h"

using namespace Catch;

namespace {

constexpr int kViewportWidth = 800;
constexpr int kViewportHeight = 600;

// The camera looks down -Z, so a LARGER z is NEARER the eye. Every depth claim
// below rests on that; requireLookingStraightDownNegativeZ asserts it rather
// than trusting the default view matrix to stay identity.
constexpr float kLeadZ = 0.0f;

// How far in front of the lead the cloud point sits. Must clear the eye-ward
// slack isOccluded allows (kLeadDepthBias, 1.0 world units, file-local to
// cwLeadView.cpp) by enough that no test here is deciding a boundary case —
// 50 units is comfortably past it.
constexpr float kCloudInFrontOfLead = 50.0f;
constexpr float kCloudZ = kLeadZ + kCloudInFrontOfLead;

// The pick sphere the cloud is registered with. Any value works — the offset
// below is expressed as a multiple of it — so this is just a round number.
constexpr float kPickRadius = 1.0f;

// Perpendicular distance from the ray to the off-ray cloud point. Outside the
// pick sphere, so the ray genuinely misses the point and nothing the user can
// see covers the lead. 1.5x rather than a comfortable 10x because 1.5x is the
// distance that used to fail: the deleted near-miss fallback promoted any point
// within 2.5 radii, so this is the band a reintroduced fallback would break
// first.
constexpr float kOffRayDistance = kPickRadius * 1.5f;

static_assert(kOffRayDistance > kPickRadius,
              "The off-ray point must fall outside its own pick sphere, or the "
              "ray really does hit it and the lead is legitimately occluded — "
              "which would make this test pass while guarding nothing.");


struct Fixture
{
    cwCamera camera;
    cwScene scene;
    cwLeadView leadView;

    Fixture()
    {
        camera.setViewport(QRect(0, 0, kViewportWidth, kViewportHeight));
        camera.setProjection(camera.orthoProjectionDefault());
        leadView.setCamera(&camera);
        leadView.setScene(&scene);
    }
};

// One cloud point, registered the way a LAZ layer registers its cloud:
// Type::Points with a pick radius (production derives that radius from
// meanSpacingXY * cwRenderPointCloud::PointPickRadiusScale; nothing here
// depends on it doing so).
void addCloudPoint(cwScene& scene, const QVector3D& position, uint64_t id = 1)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, QVector<QVector3D>{position});
    geometry.setType(cwGeometry::Type::Points);

    scene.geometryItersecter()->addObject(
        cwGeometryItersecter::Object(nullptr, id, geometry, QMatrix4x4(), kPickRadius));
    scene.geometryItersecter()->waitForFinish();
}

// Guards the sign convention every depth claim in this file rests on, AND
// rayPointAtZ's precondition. Pinning the exact direction rather than just
// `z() < 0`: a 45-degree camera would satisfy the sign yet make rayPointAtZ
// return the wrong depth, and the hit case below cannot notice (rayPointAtZ
// always returns a point ON the ray, so its distance check passes regardless).
void requireLookingStraightDownNegativeZ(const QRay3D& ray)
{
    REQUIRE(ray.direction().x() == Approx(0.0f).margin(1e-6));
    REQUIRE(ray.direction().y() == Approx(0.0f).margin(1e-6));
    REQUIRE(ray.direction().z() == Approx(-1.0f).margin(1e-6));
}

// The point on `ray` at world depth z. Only valid while the camera looks
// straight down -Z (assert that first) — which keeps the cloud points below
// placed relative to the ray the camera ACTUALLY casts rather than to an
// assumed x=0. A tap pixel is rounded to an integer, and one pixel is half a
// world unit at this zoom, so an assumed-centre ray would shift the offsets by
// a third of the band they have to land in.
QVector3D rayPointAtZ(const QRay3D& ray, float z)
{
    return ray.point(ray.origin().z() - z);
}

} // namespace

TEST_CASE("cwLeadView does not occlude a lead with a cloud point the ray misses",
          "[cwLeadView][occlusion]")
{
    // A lead sits in open space with one cloud point off to the side, near the
    // eye. The ray through the tapped pixel passes 1.5 pick radii from that
    // point — it misses the sphere, and on screen the splat does not cover the
    // marker. Nothing occludes the lead, so the tap must select it.
    //
    // This failed before cwGeometryItersecter's near-miss fallback was deleted:
    // that fallback fired whenever no exact hit existed anywhere, snapping to
    // the nearest point within 2.5 pick radii and reporting its CENTRE depth.
    // isOccluded is unusually exposed to it — it picks cwPickQuery::Solid with
    // no tolerance, and neither triangles nor points consult the tolerance, so
    // the fallback was the only near-miss path reachable here and had nothing
    // to lose to. The promoted point landed 50 units in front of the lead, the
    // depth compare said "covered", and the lead silently stopped responding to
    // clicks while looking perfectly visible.
    Fixture f;

    const QVector3D leadPosition(0.0f, 0.0f, kLeadZ);
    const QPointF tap = f.camera.project(leadPosition);
    const QRay3D ray = f.camera.rayFromQtViewport(tap);
    requireLookingStraightDownNegativeZ(ray);

    const QVector3D cloudPoint =
        rayPointAtZ(ray, kCloudZ) + QVector3D(kOffRayDistance, 0.0f, 0.0f);
    addCloudPoint(f.scene, cloudPoint);

    // Pin the premise rather than trusting the arithmetic that built it: the
    // ray must pass kOffRayDistance from the point, which the static_assert
    // above has already established is outside the sphere.
    REQUIRE(ray.distance(cloudPoint) == Approx(kOffRayDistance).margin(1e-4));

    CHECK_FALSE(f.leadView.isOccluded(leadPosition, tap));
}

TEST_CASE("cwLeadView occludes a lead behind a cloud point the ray hits",
          "[cwLeadView][occlusion]")
{
    // The other half, and the reason the case above can't be fixed by dropping
    // point clouds from the occlusion query: a lead genuinely behind the cloud
    // must stay unclickable. Same geometry as above with the point moved onto
    // the ray, so this is an exact sphere hit.
    Fixture f;

    const QVector3D leadPosition(0.0f, 0.0f, kLeadZ);
    const QPointF tap = f.camera.project(leadPosition);
    const QRay3D ray = f.camera.rayFromQtViewport(tap);
    requireLookingStraightDownNegativeZ(ray);

    const QVector3D cloudPoint = rayPointAtZ(ray, kCloudZ);
    addCloudPoint(f.scene, cloudPoint);

    REQUIRE(ray.distance(cloudPoint) == Approx(0.0).margin(1e-4));

    CHECK(f.leadView.isOccluded(leadPosition, tap));
}

TEST_CASE("cwLeadView does not occlude a lead in an empty scene",
          "[cwLeadView][occlusion]")
{
    // The floor the two cases above stand on: with no geometry at all the
    // answer must be "not occluded". Guards the no-hit and no-BVH early-outs
    // (cwLeadView.cpp's !hit.hit() return, and the intersecter's null-BVH
    // path), which nothing else here reaches. It does NOT catch an isOccluded
    // stubbed to always-false — the occludes case above is what does that.
    Fixture f;

    const QVector3D leadPosition(0.0f, 0.0f, kLeadZ);
    const QPointF tap = f.camera.project(leadPosition);

    CHECK_FALSE(f.leadView.isOccluded(leadPosition, tap));
}
