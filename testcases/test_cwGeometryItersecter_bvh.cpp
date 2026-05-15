//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Std
#include <chrono>
#include <random>

// Qt
#include <QMatrix4x4>
#include <QVector3D>
#include <QRay3D>

// SUT
#include "cwGeometryItersecter.h"
#include "cwRayHit.h"

using namespace Catch;

namespace {

constexpr float kPickRadius = 0.1f;

cwGeometryItersecter::Object makePointObject(uint64_t id,
                                             const QVector<QVector3D>& points,
                                             const QMatrix4x4& modelMatrix = QMatrix4x4(),
                                             float pickRadius = kPickRadius)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    geometry.setType(cwGeometry::Type::Points);

    return cwGeometryItersecter::Object({nullptr, id},
                                        geometry,
                                        modelMatrix,
                                        pickRadius);
}

cwGeometryItersecter::Object makeTriangleObject(uint64_t id,
                                                float worldZ)
{
    QVector<QVector3D> points;
    points << QVector3D(-1.0f, -1.0f, 0.0f)
           << QVector3D( 1.0f, -1.0f, 0.0f)
           << QVector3D( 0.0f,  1.0f, 0.0f);

    QVector<uint32_t> indexes;
    indexes << 0u << 1u << 2u;

    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    geometry.setIndices(indexes);
    geometry.setType(cwGeometry::Type::Triangles);

    QMatrix4x4 modelMatrix;
    modelMatrix.translate(0.0f, 0.0f, worldZ);

    return cwGeometryItersecter::Object({nullptr, id},
                                        geometry,
                                        modelMatrix);
}

} // namespace

TEST_CASE("BVH rebuilds after mutation", "[cwGeometryItersecter][bvh]")
{
    const QRay3D ray(QVector3D(0.0f, 0.0f, 100.0f),
                     QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;

    // Start with one point at z=0
    intersector.addObject(makePointObject(1, {QVector3D(0.0f, 0.0f, 0.0f)}));
    intersector.waitForFinish();
    {
        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE(hit.hit());
        REQUIRE(hit.objectId() == 1u);
    }

    // Add a closer point at z=50 — BVH must rebuild and prefer the closer one
    intersector.addObject(makePointObject(2, {QVector3D(0.0f, 0.0f, 50.0f)}));
    intersector.waitForFinish();
    {
        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE(hit.hit());
        REQUIRE(hit.objectId() == 2u);
    }

    // Remove the closer point — BVH must rebuild and the farther one wins again
    intersector.removeObject(nullptr, 2);
    intersector.waitForFinish();
    {
        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE(hit.hit());
        REQUIRE(hit.objectId() == 1u);
    }

    // Remove everything — no hit at all
    intersector.removeObject(nullptr, 1);
    intersector.waitForFinish();
    {
        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE_FALSE(hit.hit());
    }
}

TEST_CASE("BVH rebuilds after setModelMatrix", "[cwGeometryItersecter][bvh]")
{
    const QRay3D ray(QVector3D(0.0f, 0.0f, 100.0f),
                     QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;

    // Point at model origin, no transform — sits at world z=0
    intersector.addObject(makePointObject(1, {QVector3D(0.0f, 0.0f, 0.0f)}));
    intersector.waitForFinish();
    {
        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE(hit.hit());
        REQUIRE(hit.pointWorld().z() == Approx(0.0f).margin(1e-4));
    }

    // Translate the cloud up to world z=20 — the BVH must reflect the new position
    QMatrix4x4 m;
    m.translate(0.0f, 0.0f, 20.0f);
    intersector.setModelMatrix(nullptr, 1, m);
    intersector.waitForFinish();
    {
        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE(hit.hit());
        REQUIRE(hit.pointWorld().z() == Approx(20.0f).margin(1e-4));
    }
}

TEST_CASE("BVH finds closest hit across many primitives", "[cwGeometryItersecter][bvh]")
{
    // Build a 20x20 grid of points in the XY plane at z=0. The ray goes
    // straight down through a known cell; only that cell's point should hit.
    QVector<QVector3D> points;
    points.reserve(20 * 20);
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 20; ++x) {
            // Spacing 1.0 — well outside pickRadius=0.1, so no ambiguity
            points.append(QVector3D(static_cast<float>(x),
                                    static_cast<float>(y),
                                    0.0f));
        }
    }

    // Ray aimed exactly at point (7, 13, 0)
    const QRay3D ray(QVector3D(7.0f, 13.0f, 100.0f),
                     QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;
    intersector.addObject(makePointObject(1, points));
    intersector.waitForFinish();

    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE(hit.hit());
    // Expected vertex index: y * 20 + x = 13 * 20 + 7 = 267
    REQUIRE(hit.firstIndex() == 267);
    REQUIRE(hit.pointWorld().x() == Approx(7.0f).margin(1e-4));
    REQUIRE(hit.pointWorld().y() == Approx(13.0f).margin(1e-4));
}

TEST_CASE("BVH returns closest across mixed triangle + point primitives", "[cwGeometryItersecter][bvh]")
{
    const QRay3D ray(QVector3D(0.0f, 0.0f, 100.0f),
                     QVector3D(0.0f, 0.0f, -1.0f));

    cwGeometryItersecter intersector;

    // Sprinkle several triangles at varying depths.
    for (uint64_t i = 0; i < 10; ++i) {
        intersector.addObject(makeTriangleObject(i + 1, static_cast<float>(i)));
    }

    // Add a single point closer than every triangle.
    intersector.addObject(makePointObject(100, {QVector3D(0.0f, 0.0f, 50.0f)}));
    intersector.waitForFinish();

    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE(hit.hit());
    REQUIRE(hit.objectId() == 100u);

    // Remove the point — the closest triangle (z=9, id=10) should win.
    intersector.removeObject(nullptr, 100);
    intersector.waitForFinish();
    const cwRayHit hit2 = intersector.intersectsDetailed(ray);
    REQUIRE(hit2.hit());
    REQUIRE(hit2.objectId() == 10u);
}

TEST_CASE("BVH perf smoke: 100k random points pickable quickly", "[cwGeometryItersecter][bvh][.perf]")
{
    // 100k random points in a [0,100]^3 cube. We aim the ray straight at a
    // known target and require the hit comes back fast. The ".perf" tag in
    // the Catch2 list keeps this off the default run; invoke with the tag
    // explicitly when measuring.
    constexpr int kCount = 100000;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 100.0f);

    QVector<QVector3D> points;
    points.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        points.append(QVector3D(dist(rng), dist(rng), dist(rng)));
    }

    // Pin the last vertex to a deterministic location so the ray hits it.
    const QVector3D target(50.0f, 50.0f, 50.0f);
    points.last() = target;

    cwGeometryItersecter intersector;
    intersector.addObject(makePointObject(1, points, QMatrix4x4(), 0.5f));
    intersector.waitForFinish();

    const QRay3D ray(QVector3D(50.0f, 50.0f, 200.0f),
                     QVector3D(0.0f, 0.0f, -1.0f));

    // First call pays the build cost. Subsequent calls reuse the BVH.
    (void)intersector.intersectsDetailed(ray);

    const auto start = std::chrono::steady_clock::now();
    constexpr int kIterations = 50;
    for (int i = 0; i < kIterations; ++i) {
        const cwRayHit hit = intersector.intersectsDetailed(ray);
        REQUIRE(hit.hit());
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double msPerPick =
        std::chrono::duration<double, std::milli>(elapsed).count() / kIterations;

    // Loose upper bound — release builds should be far under this on dev
    // hardware. The target from the plan is <16ms per pick on a 1M-point
    // cloud; 100k under 50ms in debug mode is plenty of headroom.
    INFO("ms per pick (100k points, debug build): " << msPerPick);
    REQUIRE(msPerPick < 50.0);
}
