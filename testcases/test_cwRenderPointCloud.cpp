// test_cwRenderPointCloud.cpp
// Catch2 unit tests for cwRenderPointCloud — front-end only (no RHI).

#include <catch2/catch_test_macros.hpp>

#include <QVector3D>

#include "cwGeometry.h"
#include "cwRHIPointCloud.h"
#include "cwRenderPointCloud.h"

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

    cloud.setGeometry(makePointGeometry(points), bboxMin, bboxMax);

    REQUIRE(cloud.pointCount() == points.size());
    REQUIRE(cloud.bboxMin() == bboxMin);
    REQUIRE(cloud.bboxMax() == bboxMax);
}

TEST_CASE("cwRenderPointCloud: clear resets vertex data but keeps pointSize",
          "[cwRenderPointCloud]") {
    cwRenderPointCloud cloud;
    cloud.setPointSize(5.5f);

    cloud.setGeometry(makePointGeometry({ QVector3D(1.0f, 1.0f, 1.0f) }),
                      QVector3D(1.0f, 1.0f, 1.0f),
                      QVector3D(1.0f, 1.0f, 1.0f));
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
