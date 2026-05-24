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

// At world-magnitude coords (~10^4), float32 (V·D)^2 - V·V cancels into
// r^2 noise, so QSphere3D::intersection both falsely accepts off-axis
// spheres and falsely rejects on-axis ones. Coordinates here mirror a
// real failing cwRenderPointCloud pick.

constexpr float kPickRadius = 0.12f;

// Compute the perpendicular distance from a point to a ray in double
// precision. This is the ground truth the picker should agree with.
double doublePerpDistance(const QVector3D& point,
                          const QVector3D& rayOrigin,
                          const QVector3D& rayDirNormalized)
{
    const double vx = double(point.x()) - double(rayOrigin.x());
    const double vy = double(point.y()) - double(rayOrigin.y());
    const double vz = double(point.z()) - double(rayOrigin.z());
    const double dx = double(rayDirNormalized.x());
    const double dy = double(rayDirNormalized.y());
    const double dz = double(rayDirNormalized.z());
    const double t = vx * dx + vy * dy + vz * dz;
    const double projX = double(rayOrigin.x()) + t * dx;
    const double projY = double(rayOrigin.y()) + t * dy;
    const double projZ = double(rayOrigin.z()) + t * dz;
    const double rx = double(point.x()) - projX;
    const double ry = double(point.y()) - projY;
    const double rz = double(point.z()) - projZ;
    return std::sqrt(rx * rx + ry * ry + rz * rz);
}

} // namespace

TEST_CASE("Point picking is stable at world-magnitude coordinates",
          "[cwGeometryItersecter][FloatPrecision]")
{
    // Enable cw.picking debug output for diagnosis on failure.
    QLoggingCategory& cat = const_cast<QLoggingCategory&>(lcPick());
    const bool wasEnabled = cat.isDebugEnabled();
    cat.setEnabled(QtDebugMsg, true);
    auto restoreFilter = qScopeGuard([&cat, wasEnabled]() {
        cat.setEnabled(QtDebugMsg, wasEnabled);
    });

    // Ray origin and direction at production magnitudes (~10000 units
    // from the cloud). Direction is the unit vector from origin toward
    // the head-on point.
    const QVector3D rayOrigin(1000.0f, -8000.0f, 5600.0f);
    const QVector3D headOn(-700.0f, 0.0f, 0.0f);
    const QVector3D rayDir = (headOn - rayOrigin).normalized();
    const QRay3D ray(rayOrigin, rayDir);

    // Off-axis decoy: 1.0 world units perpendicular to the ray — over
    // 8x the pickRadius. In exact math this MUST be a sphere miss. The
    // perpendicular axis is the cross product of world-up and rayDir.
    const QVector3D perpAxis = QVector3D::crossProduct(
        QVector3D(0.0f, 0.0f, 1.0f), rayDir).normalized();
    const QVector3D offAxis = headOn + perpAxis * 1.0f;

    // Sanity-check the test scenario in double precision so a future
    // change to the helper or the constants is caught here, not in the
    // picker assertion.
    const double dHeadOn = doublePerpDistance(headOn, rayOrigin, rayDir);
    const double dOffAxis = doublePerpDistance(offAxis, rayOrigin, rayDir);
    REQUIRE(dHeadOn < double(kPickRadius) * 0.1);          // d_A ~= 0
    REQUIRE(dOffAxis > double(kPickRadius) * 5.0);         // d_B >> r

    constexpr int kIndexHeadOn = 0;
    constexpr int kIndexOffAxis = 1;
    QVector<QVector3D> points;
    points.append(headOn);
    points.append(offAxis);

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

    const cwRayHit hit = picker.intersectsDetailed(ray);

    INFO("hit.hit=" << hit.hit()
        << " firstIndex=" << hit.firstIndex()
        << " tWorld=" << hit.tWorld()
        << " | expected firstIndex=" << kIndexHeadOn
        << " (d_headOn=" << dHeadOn
        << " d_offAxis=" << dOffAxis
        << " radius=" << kPickRadius << ")");

    REQUIRE(hit.hit());
    REQUIRE(hit.firstIndex() == kIndexHeadOn);
}
