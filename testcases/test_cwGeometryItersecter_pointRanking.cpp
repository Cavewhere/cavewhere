//Catch includes
#include <catch2/catch_test_macros.hpp>

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

namespace {

// Pick radius large enough that a head-on hit at depth 10 has a sphere
// entry (tNear = 5) well in front of a near-grazing hit at depth 9
// (tNear ~= 8). Without that margin the test would not distinguish the
// tCenter-based ranking from the tNear-based ranking.
constexpr float kPickRadius = 5.0f;

constexpr int kIndexHeadOn = 0;
constexpr int kIndexGrazing = 1;

} // namespace

TEST_CASE("Point picking ranks by sphere entry, not by center depth",
          "[cwGeometryItersecter][PointRanking]")
{
    // Enable cw.picking debug output to aid diagnosis on failure.
    QLoggingCategory& cat = const_cast<QLoggingCategory&>(lcPick());
    const bool wasEnabled = cat.isDebugEnabled();
    cat.setEnabled(QtDebugMsg, true);
    auto restoreFilter = qScopeGuard([&cat, wasEnabled]() {
        cat.setEnabled(QtDebugMsg, wasEnabled);
    });

    // Two competing points along a ray cast down -Z from the origin.
    //
    // - Head-on point: exactly on the ray axis at depth 10. Perpendicular
    //   distance d = 0, so its sphere protrudes a full kPickRadius toward
    //   the camera (front surface at z = -5).
    // - Grazing point: off-axis at depth 9 with d ~= 0.98 * kPickRadius,
    //   so the ray barely pierces its sphere (front surface near z = -8).
    //
    // The user clicking the cursor over the head-on point expects to
    // pick it: visually it is in front (its splat covers the grazing
    // point) and it sits exactly under the cursor. The current ranking
    // wrongly picks the grazing point because its center is shallower
    // in depth.
    const QVector3D positionHeadOn(0.0f, 0.0f, -10.0f);
    const QVector3D positionGrazing(0.98f * kPickRadius, 0.0f, -9.0f);

    QVector<QVector3D> points;
    points.append(positionHeadOn);
    points.append(positionGrazing);

    cwGeometry geometry {
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    };
    geometry.setType(cwGeometry::Type::Points);
    REQUIRE(geometry.set(cwGeometry::Semantic::Position, points));

    cwGeometryItersecter picker;
    cwGeometryItersecter::Object object(
        cwGeometryItersecter::Key{nullptr, /*id=*/1},
        geometry,
        QMatrix4x4(),
        kPickRadius);
    picker.addObject(object);
    picker.waitForFinish();

    const auto stats = picker.debugStatistics();
    REQUIRE(stats.hasBvh);
    REQUIRE(stats.pointSourceNodes == 1);
    REQUIRE(stats.totalPrimitives == 2);

    const QRay3D ray(QVector3D(0.0f, 0.0f, 0.0f),
                     QVector3D(0.0f, 0.0f, -1.0f));
    const cwRayHit hit = picker.intersectsDetailed(ray);

    REQUIRE(hit.hit());

    INFO("hit.firstIndex=" << hit.firstIndex()
         << " hit.pointWorld=("
         << hit.pointWorld().x() << ","
         << hit.pointWorld().y() << ","
         << hit.pointWorld().z() << ")"
         << " hit.tWorld=" << hit.tWorld()
         << " (expected index " << kIndexHeadOn
         << " — head-on point at z=" << positionHeadOn.z()
         << "; current ranking wrongly returns index " << kIndexGrazing
         << " — grazing point at z=" << positionGrazing.z() << ")");
    REQUIRE(hit.firstIndex() == kIndexHeadOn);
}
