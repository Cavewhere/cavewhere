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
                                                const QVector3D& a,
                                                const QVector3D& b,
                                                const QVector3D& c,
                                                const QMatrix4x4& modelMatrix = QMatrix4x4())
{
    QVector<QVector3D> points;
    points << a << b << c;

    QVector<uint32_t> indexes;
    indexes << 0u << 1u << 2u;

    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);
    geometry.setIndices(indexes);
    geometry.setType(cwGeometry::Type::Triangles);

    return cwGeometryItersecter::Object({nullptr, id},
                                        geometry,
                                        modelMatrix);
}

// Generates a deterministic point cloud in [0, range]^3 with the last
// point pinned to `target` so a known ray can find it.
QVector<QVector3D> deterministicCloud(int count, float range, const QVector3D& target,
                                      uint32_t seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0.0f, range);
    QVector<QVector3D> points;
    points.reserve(count);
    for (int i = 0; i < count; ++i) {
        points.append(QVector3D(dist(rng), dist(rng), dist(rng)));
    }
    points.last() = target;
    return points;
}

} // namespace

// ============================================================================
// Correctness
// ============================================================================

TEST_CASE("setModelMatrix translates pick without invalidating sub-BVH",
          "[cwGeometryItersecter][twoLevel]")
{
    cwGeometryItersecter intersector;

    intersector.addObject(makePointObject(1, {QVector3D(0.0f, 0.0f, 0.0f)}));
    intersector.waitForFinish();

    // Ray straight down at world (0,0,0) hits it.
    const QRay3D rayAtOrigin(QVector3D(0.0f, 0.0f, 100.0f),
                             QVector3D(0.0f, 0.0f, -1.0f));
    REQUIRE(intersector.intersectsDetailed(rayAtOrigin).hit());

    // Translate the Object's modelMatrix; the same point now lives at
    // world (5, 0, 0). Ray at the new world position should hit.
    QMatrix4x4 translation;
    translation.translate(5.0f, 0.0f, 0.0f);
    intersector.setModelMatrix(nullptr, 1, translation);
    intersector.waitForFinish();

    const QRay3D rayAtFive(QVector3D(5.0f, 0.0f, 100.0f),
                           QVector3D(0.0f, 0.0f, -1.0f));
    REQUIRE(intersector.intersectsDetailed(rayAtFive).hit());

    // The old position no longer hits.
    REQUIRE_FALSE(intersector.intersectsDetailed(rayAtOrigin).hit());
}

TEST_CASE("Replacing one Object's geometry doesn't disturb others",
          "[cwGeometryItersecter][twoLevel]")
{
    cwGeometryItersecter intersector;

    // Object 1: point at (0,0,0). Object 2: point at (10,0,0).
    intersector.addObject(makePointObject(1, {QVector3D(0.0f, 0.0f, 0.0f)}));
    intersector.addObject(makePointObject(2, {QVector3D(10.0f, 0.0f, 0.0f)}));
    intersector.waitForFinish();

    const QRay3D rayAtZero(QVector3D(0.0f, 0.0f, 100.0f),
                           QVector3D(0.0f, 0.0f, -1.0f));
    const QRay3D rayAtTen(QVector3D(10.0f, 0.0f, 100.0f),
                          QVector3D(0.0f, 0.0f, -1.0f));
    REQUIRE(intersector.intersectsDetailed(rayAtZero).hit());
    REQUIRE(intersector.intersectsDetailed(rayAtTen).hit());

    // Replace Object 1 with a point at (20,0,0). Object 2 must still pick.
    intersector.addObject(makePointObject(1, {QVector3D(20.0f, 0.0f, 0.0f)}));
    intersector.waitForFinish();

    const QRay3D rayAtTwenty(QVector3D(20.0f, 0.0f, 100.0f),
                             QVector3D(0.0f, 0.0f, -1.0f));
    REQUIRE_FALSE(intersector.intersectsDetailed(rayAtZero).hit());
    REQUIRE(intersector.intersectsDetailed(rayAtTen).hit());
    REQUIRE(intersector.intersectsDetailed(rayAtTwenty).hit());
}

TEST_CASE("Remove + add same Key produces fresh sub-BVH",
          "[cwGeometryItersecter][twoLevel]")
{
    cwGeometryItersecter intersector;

    intersector.addObject(makePointObject(42, {QVector3D(0.0f, 0.0f, 0.0f)}));
    intersector.waitForFinish();

    const QRay3D rayAtOrigin(QVector3D(0.0f, 0.0f, 100.0f),
                             QVector3D(0.0f, 0.0f, -1.0f));
    REQUIRE(intersector.intersectsDetailed(rayAtOrigin).hit());

    intersector.removeObject(nullptr, 42);
    intersector.waitForFinish();
    REQUIRE_FALSE(intersector.intersectsDetailed(rayAtOrigin).hit());

    intersector.addObject(makePointObject(42, {QVector3D(3.0f, 0.0f, 0.0f)}));
    intersector.waitForFinish();

    REQUIRE_FALSE(intersector.intersectsDetailed(rayAtOrigin).hit());
    const QRay3D rayAtThree(QVector3D(3.0f, 0.0f, 100.0f),
                            QVector3D(0.0f, 0.0f, -1.0f));
    REQUIRE(intersector.intersectsDetailed(rayAtThree).hit());
}

TEST_CASE("Empty -> add -> clear -> add cycle",
          "[cwGeometryItersecter][twoLevel]")
{
    cwGeometryItersecter intersector;

    const QRay3D ray(QVector3D(0.0f, 0.0f, 100.0f),
                     QVector3D(0.0f, 0.0f, -1.0f));
    REQUIRE_FALSE(intersector.intersectsDetailed(ray).hit());

    intersector.addObject(makePointObject(1, {QVector3D(0.0f, 0.0f, 0.0f)}));
    intersector.waitForFinish();
    REQUIRE(intersector.intersectsDetailed(ray).hit());

    intersector.clear();
    intersector.waitForFinish();
    REQUIRE_FALSE(intersector.intersectsDetailed(ray).hit());

    intersector.addObject(makePointObject(2, {QVector3D(0.0f, 0.0f, 0.0f)}));
    intersector.waitForFinish();
    REQUIRE(intersector.intersectsDetailed(ray).hit());
}

TEST_CASE("Two-level traversal returns closest hit across mixed types",
          "[cwGeometryItersecter][twoLevel]")
{
    cwGeometryItersecter intersector;

    // A point at z=0 and a triangle at z=50 that blocks a downward ray.
    intersector.addObject(makePointObject(1, {QVector3D(0.0f, 0.0f, 0.0f)}));
    intersector.addObject(makeTriangleObject(
        2,
        QVector3D(-1.0f, -1.0f, 50.0f),
        QVector3D( 1.0f, -1.0f, 50.0f),
        QVector3D( 0.0f,  1.0f, 50.0f)));
    intersector.waitForFinish();

    // Ray down through the triangle; triangle is closer to the camera at
    // z=100 (at z=50) than the point (at z=0), so triangle wins.
    const QRay3D ray(QVector3D(0.0f, 0.0f, 100.0f),
                     QVector3D(0.0f, 0.0f, -1.0f));
    const cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE(hit.hit());
    REQUIRE(hit.objectId() == 2);
}

TEST_CASE("Tube-pick fallback works through the two-level path",
          "[cwGeometryItersecter][twoLevel]")
{
    cwGeometryItersecter intersector;

    // A point at (1, 0, 0) with pickRadius=0.1; the ray's X offset of 0.3
    // is outside the strict sphere (0.3 > 0.1) but within tube range
    // (kTubeFactor * pickRadius = 5 * 0.1 = 0.5).
    intersector.addObject(makePointObject(1, {QVector3D(1.0f, 0.0f, 0.0f)}));
    intersector.waitForFinish();

    const QRay3D ray(QVector3D(1.3f, 0.0f, 100.0f),
                     QVector3D(0.0f, 0.0f, -1.0f));

    // With tube-pick on (default), the pick should land on the point.
    cwRayHit hit = intersector.intersectsDetailed(ray);
    REQUIRE(hit.hit());

    // Disable tube-pick — pick should now miss.
    intersector.setTubePickEnabled(false);
    cwRayHit hitStrict = intersector.intersectsDetailed(ray);
    REQUIRE_FALSE(hitStrict.hit());
}

// ============================================================================
// Performance — opt-in via the .perf tag
//
// The point of the two-level BVH is that mutator cost on small Objects is
// independent of large-cloud size. These tests load a 1M-point cloud, then
// measure how long a small mutation takes — without the optimization, each
// would trigger a full 1M-point rebuild (seconds in debug). The thresholds
// below leave generous headroom on dev hardware.
// ============================================================================

namespace {

constexpr int kLargeCloudCount = 1'000'000;
constexpr float kLargeCloudRange = 100.0f;
constexpr uint32_t kCloudSeed = 42;

}

TEST_CASE("perf: adding small object beside large cloud is fast",
          "[.perf][cwGeometryItersecter][twoLevel]")
{
    cwGeometryItersecter intersector;
    intersector.addObject(makePointObject(
        1,
        deterministicCloud(kLargeCloudCount, kLargeCloudRange,
                           QVector3D(50.0f, 50.0f, 50.0f), kCloudSeed),
        QMatrix4x4(), 0.5f));
    intersector.waitForFinish();

    // Now add one tiny triangle Object. Old flat-BVH would rebuild the
    // full 1M-point cloud's primitives; new path rebuilds only this
    // Object's 1-triangle sub-BVH + the top-level (now 2 leaves).
    const auto start = std::chrono::steady_clock::now();
    intersector.addObject(makeTriangleObject(
        2,
        QVector3D(-1.0f, -1.0f, 200.0f),
        QVector3D( 1.0f, -1.0f, 200.0f),
        QVector3D( 0.0f,  1.0f, 200.0f)));
    intersector.waitForFinish();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double ms = std::chrono::duration<double, std::milli>(elapsed).count();

    INFO("ms to add small triangle beside 1M-point cloud: " << ms);
    REQUIRE(ms < 50.0);
}

TEST_CASE("perf: setModelMatrix on small object is near-instant",
          "[.perf][cwGeometryItersecter][twoLevel]")
{
    cwGeometryItersecter intersector;
    intersector.addObject(makePointObject(
        1,
        deterministicCloud(kLargeCloudCount, kLargeCloudRange,
                           QVector3D(50.0f, 50.0f, 50.0f), kCloudSeed),
        QMatrix4x4(), 0.5f));
    intersector.addObject(makeTriangleObject(
        2,
        QVector3D(-1.0f, -1.0f, 0.0f),
        QVector3D( 1.0f, -1.0f, 0.0f),
        QVector3D( 0.0f,  1.0f, 0.0f)));
    intersector.waitForFinish();

    constexpr int kIterations = 10;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        QMatrix4x4 m;
        m.translate(float(i), 0.0f, 0.0f);
        intersector.setModelMatrix(nullptr, 2, m);
        intersector.waitForFinish();
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double msPerOp =
        std::chrono::duration<double, std::milli>(elapsed).count() / kIterations;

    INFO("ms per setModelMatrix on small object beside 1M-point cloud: " << msPerOp);
    REQUIRE(msPerOp < 50.0);
}

TEST_CASE("perf: replacing small object beside large cloud is fast",
          "[.perf][cwGeometryItersecter][twoLevel]")
{
    cwGeometryItersecter intersector;
    intersector.addObject(makePointObject(
        1,
        deterministicCloud(kLargeCloudCount, kLargeCloudRange,
                           QVector3D(50.0f, 50.0f, 50.0f), kCloudSeed),
        QMatrix4x4(), 0.5f));
    intersector.addObject(makeTriangleObject(
        2,
        QVector3D(-1.0f, -1.0f, 0.0f),
        QVector3D( 1.0f, -1.0f, 0.0f),
        QVector3D( 0.0f,  1.0f, 0.0f)));
    intersector.waitForFinish();

    const auto start = std::chrono::steady_clock::now();
    intersector.addObject(makeTriangleObject(
        2,
        QVector3D(-5.0f, -5.0f, 0.0f),
        QVector3D( 5.0f, -5.0f, 0.0f),
        QVector3D( 0.0f,  5.0f, 0.0f)));
    intersector.waitForFinish();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double ms = std::chrono::duration<double, std::milli>(elapsed).count();

    INFO("ms to replace small triangle beside 1M-point cloud: " << ms);
    REQUIRE(ms < 50.0);
}

TEST_CASE("perf: pick latency on large cloud unchanged by two-level layout",
          "[.perf][cwGeometryItersecter][twoLevel]")
{
    cwGeometryItersecter intersector;

    constexpr int kCount = 100'000;
    const QVector3D target(50.0f, 50.0f, 50.0f);
    intersector.addObject(makePointObject(
        1,
        deterministicCloud(kCount, 100.0f, target, kCloudSeed),
        QMatrix4x4(), 0.5f));
    intersector.waitForFinish();

    const QRay3D ray(target + QVector3D(0.0f, 0.0f, 100.0f),
                     QVector3D(0.0f, 0.0f, -1.0f));
    // Warm-up
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

    // Matches the threshold the original flat-BVH perf test (in
    // test_cwGeometryItersecter_bvh.cpp) uses for 100k points in debug.
    INFO("ms per pick (100k points via two-level, debug build): " << msPerPick);
    REQUIRE(msPerPick < 50.0);
}
