//Catch includes
#include <catch2/catch_test_macros.hpp>

// Qt
#include <QMatrix4x4>
#include <QVector3D>
#include <QRay3D>

// SUT
#include "cwGeometryItersecter.h"
#include "cwRayHit.h"

namespace {

constexpr uint64_t kWallId = 1;
constexpr uint64_t kBillboardId = 2;

struct Mesh {
    QVector<QVector3D> verts;
    QVector<uint32_t> indices;
};

// A camera-facing quad in the same basis cwRenderBillboards::buildModelMatrix
// uses: right/up are the view matrix's first two rows; the quad lies in the
// plane perpendicular to the view direction at `center`.
Mesh makeCameraFacingQuad(const QVector3D& center,
                          const QVector3D& right,
                          const QVector3D& up,
                          float halfWidth,
                          float halfHeight)
{
    const QVector3D r = right * halfWidth;
    const QVector3D u = up * halfHeight;
    Mesh mesh;
    mesh.verts << center - r - u   // 0
               << center + r - u   // 1
               << center + r + u   // 2
               << center - r + u;  // 3
    mesh.indices << 0 << 1 << 2 << 0 << 2 << 3;
    return mesh;
}

cwGeometryItersecter::Object makeObject(uint64_t id, const Mesh& mesh)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, mesh.verts);
    geometry.setIndices(mesh.indices);
    geometry.setType(cwGeometry::Type::Triangles);
    geometry.setCullBackfaces(false);
    return cwGeometryItersecter::Object(nullptr, id, geometry, QMatrix4x4());
}

struct Basis {
    QVector3D right;
    QVector3D up;
    QVector3D viewDir; // eye -> center, normalized
};

Basis basisLookingAt(const QVector3D& eye, const QVector3D& center)
{
    QMatrix4x4 view;
    view.lookAt(eye, center, QVector3D(0.0f, 1.0f, 0.0f));
    Basis b;
    b.right = QVector3D(view(0, 0), view(0, 1), view(0, 2));
    b.up    = QVector3D(view(1, 0), view(1, 1), view(1, 2));
    b.viewDir = (center - eye).normalized();
    return b;
}

// Orthographic (parallel) ray that passes through `point` travelling along the
// view direction — matching CaveWhere's orthographic projection, so a ray's
// lateral offset is constant with depth (no perspective scaling to reason about).
QRay3D orthoRayThrough(const QVector3D& point, const QVector3D& viewDir, float originBack)
{
    return QRay3D(point - viewDir * originBack, viewDir);
}

} // namespace

// Validates the unification the billboard occlusion needs: pick the lead's
// camera-facing quad against the same BVH the cave geometry lives in, so the
// clickable surface is the quad the GPU actually draws. A single eye->center
// ray (today's cwLeadView::isOccluded) cannot match the per-pixel silhouette
// clipping the depth buffer does — this test pins down that the BVH can.
TEST_CASE("BVH picks a camera-facing billboard quad like the depth buffer",
          "[cwGeometryItersecter][bvh][billboardQuad]")
{
    // Large world coordinates, matching real leads / the brute-force test.
    const QVector3D eye(-328.0f, 864.0f, -626.0f);
    const QVector3D leadCenter(210.0f, 320.0f, -870.0f);
    const Basis basis = basisLookingAt(eye, leadCenter);

    constexpr float kHalf = 20.0f;       // billboard half-size, world units
    constexpr float kWallInFront = 50.0f; // wall sits 50 units toward the eye
    constexpr float kRayBack = 10000.0f;

    // The geometric centre of a two-triangle quad lies exactly on the diagonal
    // edge shared by its triangles, where Moller-Trumbore's u+v boundary test
    // can reject both on float rounding. Sample a near-centre pixel just off
    // that diagonal so "centre" rays are deterministic. (A real quad picker
    // must handle this — a click dead-centre can otherwise fall through.)
    const QVector3D nearCenter = leadCenter + basis.up * 2.0f;

    const Mesh billboard = makeCameraFacingQuad(leadCenter, basis.right, basis.up,
                                                kHalf, kHalf);

    auto buildIntersecter = [&](cwGeometryItersecter& intersecter,
                                const QVector3D& wallCenter,
                                float wallHalfW, float wallHalfH) {
        const Mesh wall = makeCameraFacingQuad(wallCenter, basis.right, basis.up,
                                               wallHalfW, wallHalfH);
        intersecter.addObject(makeObject(kWallId, wall));
        intersecter.addObject(makeObject(kBillboardId, billboard));
        intersecter.waitForFinish();
    };

    SECTION("ray through the center hits the quad when nothing covers it") {
        cwGeometryItersecter intersecter;
        // Wall covers only the far right of the quad, not the center.
        buildIntersecter(intersecter, leadCenter + basis.right * 20.0f - basis.viewDir * kWallInFront,
                         15.0f, 40.0f);

        const QRay3D ray = orthoRayThrough(nearCenter, basis.viewDir, kRayBack);
        const cwRayHit hit = intersecter.intersectsDetailed(ray);

        REQUIRE(hit.hit());
        CHECK(hit.objectId() == kBillboardId);
    }

    SECTION("ray through a covered region hits the wall, not the quad (silhouette clip)") {
        cwGeometryItersecter intersecter;
        // Wall centred at +20 right, covering [5,35] in the right axis.
        buildIntersecter(intersecter, leadCenter + basis.right * 20.0f - basis.viewDir * kWallInFront,
                         15.0f, 40.0f);

        // A point on the quad's right half (within +/-20) and within the wall.
        const QVector3D coveredPoint = leadCenter + basis.right * 15.0f;
        const QRay3D ray = orthoRayThrough(coveredPoint, basis.viewDir, kRayBack);
        const cwRayHit hit = intersecter.intersectsDetailed(ray);

        REQUIRE(hit.hit());
        CHECK(hit.objectId() == kWallId);
    }

    SECTION("center occluded but rim visible — the see-but-can't-click case") {
        cwGeometryItersecter intersecter;
        // A small wall over the quad CENTER only ([-5,5] in the right axis).
        buildIntersecter(intersecter, leadCenter - basis.viewDir * kWallInFront,
                         5.0f, 5.0f);

        // What today's isOccluded does: one ray through the lead center. The
        // wall is in front, so the center reads as occluded.
        const QRay3D centerRay = orthoRayThrough(nearCenter, basis.viewDir, kRayBack);
        const cwRayHit centerHit = intersecter.intersectsDetailed(centerRay);
        REQUIRE(centerHit.hit());
        CHECK(centerHit.objectId() == kWallId);

        // But a ray through a visible rim pixel of the same quad hits the quad,
        // exactly as the GPU keeps that pixel on screen — so the lead is in fact
        // clickable there. Quad-vs-point is the whole discrepancy.
        const QVector3D rimPoint = leadCenter + basis.right * 15.0f;
        const QRay3D rimRay = orthoRayThrough(rimPoint, basis.viewDir, kRayBack);
        const cwRayHit rimHit = intersecter.intersectsDetailed(rimRay);
        REQUIRE(rimHit.hit());
        CHECK(rimHit.objectId() == kBillboardId);
    }
}
