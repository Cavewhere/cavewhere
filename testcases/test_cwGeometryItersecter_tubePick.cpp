//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwGeometry.h"
#include "cwGeometryItersecter.h"
#include "cwPickingLog.h"
#include "cwRayHit.h"

//Qt includes
#include <QLoggingCategory>
#include <QMatrix4x4>
#include <QRay3D>
#include <QScopeGuard>
#include <QVector3D>
#include <QVector>

//Std includes
#include <cmath>

using namespace Catch;

namespace {

constexpr float kPickRadius = 0.1f;
constexpr float kSpacing = 0.5f;        // 5x pickRadius — gap between columns
constexpr int kGridDim = 10;            // 10x10 grid per cluster
constexpr float kClusterAZ = 0.0f;      // cluster the user aimed at
constexpr float kClusterBZ = -500.0f;   // far cluster — inflates BVH-root AABB
                                        // so a corner-based fallback can pick
                                        // the wrong depth (mirrors production
                                        // cave with vertical extent)
constexpr float kCameraZ = 100.0f;
constexpr float kCursorOffsetX = 0.5f;  // sub-cell gap (between x=0.25 and x=0.75)
constexpr float kClusterAToleranceFactor = 5.0f; // few-radius tube margin

void appendClusterAt(QVector<QVector3D>& points, float z)
{
    const float origin = -((kGridDim - 1) * kSpacing) * 0.5f;
    for (int i = 0; i < kGridDim; ++i) {
        for (int j = 0; j < kGridDim; ++j) {
            points.append(QVector3D(origin + i * kSpacing,
                                    origin + j * kSpacing,
                                    z));
        }
    }
}

} // namespace

// Reproduces the production camera-jump in cwBaseTurnTableInteraction:
// when the cursor sits in a sub-pixel gap that misses every sphere but
// is within a few pickRadii of a real point, the user expects to pivot
// on that point. The current intersects() falls back to a brute-force
// nearestNeighbor that projects the BVH-root AABB corners onto the ray;
// with vertical cloud extent (cluster B at z=-500) the corner-pick
// lands hundreds of units off in depth, jerking the camera.
TEST_CASE("Ray grazing a dense cluster returns a near-cluster pivot, "
          "not a far-cluster AABB-corner garbage point",
          "[cwGeometryItersecter][NearestNeighborTube]")
{
    QLoggingCategory& cat = const_cast<QLoggingCategory&>(lcPick());
    const bool wasEnabled = cat.isDebugEnabled();
    cat.setEnabled(QtDebugMsg, true);
    auto restoreFilter = qScopeGuard([&cat, wasEnabled]() {
        cat.setEnabled(QtDebugMsg, wasEnabled);
    });

    QVector<QVector3D> points;
    appendClusterAt(points, kClusterAZ);
    appendClusterAt(points, kClusterBZ);

    cwGeometry geometry {
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    };
    geometry.setType(cwGeometry::Type::Points);
    REQUIRE(geometry.set(cwGeometry::Semantic::Position, points));

    cwGeometryItersecter picker;
    picker.addObject(cwGeometryItersecter::Object(
        cwGeometryItersecter::Key{nullptr, /*id=*/1},
        geometry,
        QMatrix4x4(),
        kPickRadius));
    picker.waitForFinish();

    const auto stats = picker.debugStatistics();
    REQUIRE(stats.hasBvh);
    REQUIRE(stats.totalPrimitives == 2 * kGridDim * kGridDim);

    // Top-down ray through a gap between the x=0.25 and x=0.75 columns,
    // and the y=-0.25 and y=0.25 rows. Perpendicular distance to the
    // four nearest points (0.25, ±0.25, 0) / (0.75, ±0.25, 0) is
    // sqrt(0.25^2 + 0.25^2) = 0.354 = 3.54x pickRadius — outside every
    // sphere, well within any reasonable tube.
    const QVector3D rayOrigin(kCursorOffsetX, 0.0f, kCameraZ);
    const QRay3D ray(rayOrigin, QVector3D(0.0f, 0.0f, -1.0f));

    SECTION("intersectsDetailed snaps to the near cluster") {
        const cwRayHit hit = picker.intersectsDetailed(ray);

        INFO("hit.hit=" << hit.hit()
             << " hit.tWorld=" << hit.tWorld()
             << " hit.pointWorld=("
             << hit.pointWorld().x() << ","
             << hit.pointWorld().y() << ","
             << hit.pointWorld().z() << ")"
             << " | expected z near " << kClusterAZ);

        REQUIRE(hit.hit());
        REQUIRE(hit.pointWorld().z()
                == Approx(kClusterAZ).margin(kPickRadius * kClusterAToleranceFactor));
    }

    SECTION("intersects(t-only) lands at the near cluster's depth") {
        const double t = picker.intersects(ray);
        REQUIRE(!std::isnan(t));

        // Ray is (0.5, 0, 100) going (0,0,-1), so world.z = 100 - t.
        // Cluster A hit -> t ≈ 100, world.z ≈ 0.
        // Brute-force fallback picks a (z=-500) AABB corner -> t ≈ 600,
        // world.z ≈ -500 (the jump documented in the failing log).
        const QVector3D world = ray.point(t);
        INFO("t=" << t << " world=(" << world.x() << ","
             << world.y() << "," << world.z() << ")"
             << " | expected z near " << kClusterAZ);

        REQUIRE(world.z()
                == Approx(kClusterAZ).margin(kPickRadius * kClusterAToleranceFactor));
    }

    SECTION("disabling the tube fallback restores strict sphere semantics") {
        REQUIRE(picker.isTubePickEnabled());
        picker.setTubePickEnabled(false);
        REQUIRE_FALSE(picker.isTubePickEnabled());

        const cwRayHit hit = picker.intersectsDetailed(ray);
        REQUIRE_FALSE(hit.hit());

        const double t = picker.intersects(ray);
        REQUIRE(std::isnan(t));
    }
}
