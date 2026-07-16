//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Std
#include <numeric>

// Qt
#include <QMatrix4x4>
#include <QPointF>
#include <QRect>
#include <QVector3D>

// SUT
#include "cwScenePick.h"
#include "cwCamera.h"
#include "cwGeometry.h"
#include "cwGeometryItersecter.h"
#include "cwProjection.h"
#include "cwRenderLinePlot.h"
#include "cwScene.h"

using namespace Catch;

namespace {

// A perspective camera at the origin looking down -Z, so points at z < 0 are in
// front of the eye.
void configureCamera(cwCamera& camera)
{
    camera.setViewport(QRect(0, 0, 800, 600));

    cwProjection projection;
    projection.setPerspective(55.0, 800.0 / 600.0, 1.0, 10000.0);
    camera.setProjection(projection);

    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0f, 0.0f, 0.0f),
                QVector3D(0.0f, 0.0f, -1.0f),
                QVector3D(0.0f, 1.0f, 0.0f));
    camera.setViewMatrix(view);
}

// Registers a segment in the BVH the way cwRenderLinePlot::setGeometry does
// (non-indexed pairs + iota indices), keyed to a real render object so a hit's
// object() casts back to the plot.
cwGeometryItersecter::Object makeLineObjectFor(cwRenderObject* owner,
                                               const QVector<QVector3D>& points)
{
    cwGeometry geometry {
        {cwGeometry::Semantic::Position, cwGeometry::AttributeFormat::Vec3}
    };
    geometry.set(cwGeometry::Semantic::Position, points);

    QVector<uint32_t> indices(points.size());
    std::iota(indices.begin(), indices.end(), 0u);
    geometry.setIndices(std::move(indices));
    geometry.setType(cwGeometry::Type::Lines);

    return cwGeometryItersecter::Object({owner, 0}, geometry);
}

}

TEST_CASE("cwScenePick::snappedPoint clamps a near-station pick to the station",
          "[cwScenePick]")
{
    cwScene scene;

    const QVector3D a(-30.0f, 0.0f, -100.0f);
    const QVector3D b(30.0f, 0.0f, -100.0f);
    QVector<QVector3D> points;
    points << a << b;

    cwRenderLinePlot linePlot;
    linePlot.setGeometry(points);
    scene.geometryItersecter()->addObject(makeLineObjectFor(&linePlot, points));
    scene.geometryItersecter()->waitForFinish();

    cwCamera camera;
    configureCamera(camera);

    SECTION("a click near a station snaps to its exact coordinate") {
        const auto result = cwScenePick::snappedPoint(
                    camera.project(a), camera, *scene.geometryItersecter(), 6.0);
        CHECK(result.hit);
        CHECK(result.snappedToStation);
        CHECK(result.stationVertexIndex == 0);
        // The snap returns the stored vertex verbatim, so this is exact.
        CHECK(result.world == a);
    }

    SECTION("a mid-segment click keeps the free point on the line") {
        const QVector3D mid(0.0f, 0.0f, -100.0f);
        const auto result = cwScenePick::snappedPoint(
                    camera.project(mid), camera, *scene.geometryItersecter(), 6.0);
        CHECK(result.hit);
        CHECK_FALSE(result.snappedToStation);
        CHECK(result.stationVertexIndex == -1);
        CHECK(result.world.x() == Approx(mid.x()).margin(0.5));
        CHECK(result.world.y() == Approx(mid.y()).margin(0.5));
        CHECK(result.world.z() == Approx(mid.z()).margin(0.5));
    }

    SECTION("a click into empty space misses") {
        const QPointF corner(1.0, 1.0);
        const auto result = cwScenePick::snappedPoint(
                    corner, camera, *scene.geometryItersecter(), 6.0);
        CHECK_FALSE(result.hit);
        CHECK_FALSE(result.snappedToStation);
    }
}
