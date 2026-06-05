// test_cwRenderPointCloud.cpp
// Catch2 unit tests for cwRenderPointCloud — front-end only (no RHI).

#include <catch2/catch_test_macros.hpp>

#include <QVector3D>

#include "cwGeometry.h"
#include "cwRHIPointCloud.h"
#include "cwRenderPointCloud.h"

// Friend accessor (declared friend in cwRenderPointCloud.h) so the
// re-upload regression test can observe the private change trackers
// without putting test scaffolding on the production API.
struct CwRenderPointCloudTestAccess {
    static bool geometryChanged(const cwRenderPointCloud& c) {
        return c.m_geometry.isChanged();
    }
    static bool renderStateChanged(const cwRenderPointCloud& c) {
        return c.m_renderState.isChanged();
    }
    // Mimics the RHI back-end consuming a geometry upload — updateResources
    // resets the tracker after staging the vertex buffer.
    static void clearGeometryChanged(cwRenderPointCloud& c) {
        c.m_geometry.resetChanged();
    }
};

namespace {

cwGeometry makePointGeometry(const QVector<QVector3D>& points)
{
    cwGeometry geometry({
        { cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3 }
    });
    geometry.setType(cwGeometry::Type::Points);
    geometry.set(cwGeometry::Semantic::Position, points);
    return geometry;
}

} // namespace

TEST_CASE("cwRenderPointCloud: starts empty", "[cwRenderPointCloud]") {
    cwRenderPointCloud cloud;
    REQUIRE(cloud.pointCount() == 0);
    REQUIRE(cloud.bboxMin() == QVector3D());
    REQUIRE(cloud.bboxMax() == QVector3D());
}

TEST_CASE("cwRenderPointCloud: setGeometry populates Data", "[cwRenderPointCloud]") {
    cwRenderPointCloud cloud;

    const QVector<QVector3D> points = {
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(1.0f, 2.0f, 3.0f),
        QVector3D(-1.0f, -2.0f, -3.0f)
    };
    const QVector3D bboxMin(-1.0f, -2.0f, -3.0f);
    const QVector3D bboxMax(1.0f, 2.0f, 3.0f);

    cloud.setGeometry({
        .geometry = makePointGeometry(points),
        .bboxMin = bboxMin,
        .bboxMax = bboxMax,
        .meanSpacingXY = 0.5f,
    });

    REQUIRE(cloud.pointCount() == points.size());
    REQUIRE(cloud.bboxMin() == bboxMin);
    REQUIRE(cloud.bboxMax() == bboxMax);
    REQUIRE(cloud.meanSpacingXY() == 0.5f);
}

TEST_CASE("cwRenderPointCloud: clear resets vertex data but keeps pointSize",
          "[cwRenderPointCloud]") {
    cwRenderPointCloud cloud;
    cloud.setPointSize(5.5f);

    cloud.setGeometry({
        .geometry = makePointGeometry({ QVector3D(1.0f, 1.0f, 1.0f) }),
        .bboxMin = QVector3D(1.0f, 1.0f, 1.0f),
        .bboxMax = QVector3D(1.0f, 1.0f, 1.0f),
        .meanSpacingXY = 0.0f,
    });
    REQUIRE(cloud.pointCount() == 1);

    cloud.clear();
    REQUIRE(cloud.pointCount() == 0);
    REQUIRE(cloud.pointSize() == 5.5f);
}

TEST_CASE("cwRenderPointCloud: createRHIObject returns cwRHIPointCloud",
          "[cwRenderPointCloud]") {
    // createRHIObject is protected on cwRenderObject; exercise it through a
    // derived helper that exposes the call.
    struct Exposer : cwRenderPointCloud {
        using cwRenderPointCloud::createRHIObject;
    };

    Exposer cloud;
    cwRHIObject* rhiObject = cloud.createRHIObject();
    REQUIRE(rhiObject != nullptr);
    REQUIRE(dynamic_cast<cwRHIPointCloud*>(rhiObject) != nullptr);
    delete rhiObject;
}

TEST_CASE("cwRenderPointCloud: a uniform-only change does not dirty the geometry tracker",
          "[cwRenderPointCloud]") {
    // Regression guard for the point-cloud re-upload trap.
    //
    // The RHI back-end (cwRHIPointCloud::updateResources) re-stages the
    // potentially multi-GB vertex buffer only when the geometry tracker
    // reports a change. Geometry and the cheap render knobs (world radius,
    // point size) are tracked separately for exactly this reason: a
    // uniform-only setter must NOT mark geometry dirty, or every P+wheel
    // tick / offline render view would re-upload the whole cloud (an
    // unbounded staging leak across a sweep). Before the split, one shared
    // always-"changed" tracker meant any setter re-uploaded geometry.
    using Access = CwRenderPointCloudTestAccess;

    cwRenderPointCloud cloud;
    cloud.setGeometry({
        .geometry = makePointGeometry({
            QVector3D(0.0f, 0.0f, 0.0f),
            QVector3D(1.0f, 2.0f, 3.0f),
            QVector3D(-1.0f, -2.0f, -3.0f),
        }),
        .bboxMin = QVector3D(-1.0f, -2.0f, -3.0f),
        .bboxMax = QVector3D(1.0f, 2.0f, 3.0f),
        .meanSpacingXY = 0.5f,
    });

    // setGeometry dirties the geometry tracker → the back-end uploads.
    REQUIRE(Access::geometryChanged(cloud));

    // Simulate the back-end consuming that upload.
    Access::clearGeometryChanged(cloud);
    REQUIRE_FALSE(Access::geometryChanged(cloud));

    // A uniform-only change must leave the geometry tracker clean (no
    // re-upload) while marking render state dirty (cheap UBO update).
    cloud.setWorldRadius(cloud.worldRadius() + 1.0f);
    REQUIRE_FALSE(Access::geometryChanged(cloud));
    REQUIRE(Access::renderStateChanged(cloud));

    // Same for point size.
    Access::clearGeometryChanged(cloud);
    cloud.setPointSize(cloud.pointSize() + 1.0f);
    REQUIRE_FALSE(Access::geometryChanged(cloud));

    // A genuine geometry change does re-dirty the geometry tracker.
    cloud.setGeometry({
        .geometry = makePointGeometry({ QVector3D(2.0f, 2.0f, 2.0f) }),
        .bboxMin = QVector3D(2.0f, 2.0f, 2.0f),
        .bboxMax = QVector3D(2.0f, 2.0f, 2.0f),
        .meanSpacingXY = 0.25f,
    });
    REQUIRE(Access::geometryChanged(cloud));
}
