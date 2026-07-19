//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Std
#include <cmath>
#include <limits>
#include <random>

// Qt
#include <QMatrix4x4>
#include <QVector3D>
#include <QRay3D>

// SUT
#include "cwGeometryItersecter.h"
#include "TestGeometryBuilders.h"
#include "cwRayHit.h"

using namespace Catch;

namespace {

// Ground-truth ray/triangle nearest hit over every triangle (Moller-Trumbore,
// double precision). The BVH's intersectsDetailed must agree with this; any
// disagreement (e.g. returning a bounding-box entry distance instead of the
// triangle hit) is the bug we are hunting.
struct BruteHit {
    bool hit = false;
    double t = (std::numeric_limits<double>::max)();
    QVector3D point;
    int triangle = -1;
};

BruteHit bruteForceNearest(const QVector<QVector3D>& worldVerts,
                           const QVector<uint32_t>& indices,
                           const QRay3D& ray)
{
    const QVector3D o = ray.origin();
    const QVector3D d = ray.direction().normalized();
    constexpr double kEps = 1e-9;

    BruteHit best;
    for (int i = 0; i + 2 < indices.size(); i += 3) {
        const QVector3D& v0 = worldVerts.at(indices.at(i));
        const QVector3D& v1 = worldVerts.at(indices.at(i + 1));
        const QVector3D& v2 = worldVerts.at(indices.at(i + 2));

        const QVector3D e1 = v1 - v0;
        const QVector3D e2 = v2 - v0;
        const QVector3D p = QVector3D::crossProduct(d, e2);
        const double det = double(QVector3D::dotProduct(e1, p));
        if (std::abs(det) < kEps) {
            continue;
        }
        const double invDet = 1.0 / det;

        const QVector3D tvec = o - v0;
        const double u = double(QVector3D::dotProduct(tvec, p)) * invDet;
        if (u < 0.0 || u > 1.0) {
            continue;
        }
        const QVector3D q = QVector3D::crossProduct(tvec, e1);
        const double v = double(QVector3D::dotProduct(d, q)) * invDet;
        if (v < 0.0 || u + v > 1.0) {
            continue;
        }
        const double t = double(QVector3D::dotProduct(e2, q)) * invDet;
        if (t > kEps && t < best.t) {
            best.hit = true;
            best.t = t;
            best.triangle = i / 3;
            best.point = o + d * float(t);
        }
    }
    return best;
}

// A displaced (folded) grid surface offset to large world coordinates near the
// real failing leads, so a single eye ray can pierce a near fold before a far
// one — the curved-scrap self-occlusion regime, at the scale where it bites.
struct Mesh {
    QVector<QVector3D> verts;
    QVector<uint32_t> indices;
};

Mesh makeFoldedGrid(int n, const QVector3D& origin, float span)
{
    Mesh mesh;
    const float step = span / float(n - 1);
    for (int iy = 0; iy < n; ++iy) {
        for (int ix = 0; ix < n; ++ix) {
            const float x = origin.x() + float(ix) * step;
            const float y = origin.y() + float(iy) * step;
            // Fold the surface in z so it doubles back toward the eye.
            const float z = origin.z()
                            + 12.0f * std::sin(float(ix) * 0.9f)
                            + 8.0f * std::cos(float(iy) * 0.7f);
            mesh.verts << QVector3D(x, y, z);
        }
    }
    for (int iy = 0; iy < n - 1; ++iy) {
        for (int ix = 0; ix < n - 1; ++ix) {
            const uint32_t a = uint32_t(iy * n + ix);
            const uint32_t b = uint32_t(iy * n + ix + 1);
            const uint32_t c = uint32_t((iy + 1) * n + ix);
            const uint32_t d = uint32_t((iy + 1) * n + ix + 1);
            mesh.indices << a << b << c;
            mesh.indices << b << d << c;
        }
    }
    return mesh;
}

cwGeometryItersecter::Object makeTriangleObject(uint64_t id, const Mesh& mesh)
{
    // Double-sided (cullBackfaces=false), as cwTriangulateTask sets on scraps.
    return cwGeometryItersecter::Object(
        nullptr, id, cwTestGeometry::triangles(mesh.verts, mesh.indices, false),
        QMatrix4x4());
}

} // namespace

// Parity guard for the BVH picker against a double-precision brute-force
// reference, on a folded surface at large world coordinates (the curved-scrap
// self-occlusion regime where a single eye ray pierces a near fold before a
// far one). The geometry is double-sided (cullBackfaces=false), matching what
// cwTriangulateTask sets on every scrap carpet — so intersectsDetailed must
// return the same nearest triangle brute force does.
//
// Note: the divergence originally chased here was entirely the cullBackfaces
// flag. With the default (true), the picker honours it and skips backface
// triangles, so on a folded surface it returns the nearer-facing fold farther
// along the ray while brute force (which never culls) finds the near backface.
// That is by design for the render flag; scraps disable it, so this test fixes
// it off to lock in true nearest-surface parity for the picker.
TEST_CASE("BVH intersectsDetailed matches brute-force triangle intersection",
          "[cwGeometryItersecter][bvh][bruteForce]")
{
    // Mesh and eye placed at the same large coordinates as the real leads.
    const QVector3D meshOrigin(180.0f, 290.0f, -870.0f);
    const Mesh mesh = makeFoldedGrid(24, meshOrigin, 60.0f);

    cwGeometryItersecter intersector;
    intersector.addObject(makeTriangleObject(1, mesh));
    intersector.waitForFinish();

    // An eye roughly where the real camera sat, looking at the mesh.
    const QVector3D eye(-328.0f, 864.0f, -626.0f);
    const QVector3D meshCenter = meshOrigin + QVector3D(30.0f, 30.0f, 0.0f);

    std::mt19937 rng(1234567u);
    std::uniform_real_distribution<float> jitter(-28.0f, 28.0f);

    int compared = 0;
    int hitMismatches = 0;
    int distanceMismatches = 0;
    double worstDistanceError = 0.0;

    for (int i = 0; i < 4000; ++i) {
        const QVector3D target = meshCenter
                                 + QVector3D(jitter(rng), jitter(rng), jitter(rng));
        const QVector3D dir = (target - eye).normalized();
        const QRay3D ray(eye, dir);

        const BruteHit brute = bruteForceNearest(mesh.verts, mesh.indices, ray);
        const cwRayHit bvh = intersector.intersectsDetailed(ray);

        compared++;

        if (bvh.hit() != brute.hit) {
            hitMismatches++;
            continue;
        }
        if (!brute.hit) {
            continue;
        }

        // Both hit: the world distance must agree. A bounding-box hit instead of
        // a triangle hit would make the BVH distance much smaller than brute.
        const double error = std::abs(bvh.tWorld() - brute.t);
        worstDistanceError = (std::max)(worstDistanceError, error);
        // Tolerance well below the 2-28 unit gaps seen in the field, but above
        // float round-off at these coordinates.
        if (error > 0.25) {
            distanceMismatches++;
        }
    }

    INFO("compared=" << compared
         << " hitMismatches=" << hitMismatches
         << " distanceMismatches=" << distanceMismatches
         << " worstDistanceError=" << worstDistanceError);

    CHECK(hitMismatches == 0);
    CHECK(distanceMismatches == 0);
}
