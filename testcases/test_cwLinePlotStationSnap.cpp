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
#include "cwLinePlotStationSnap.h"
#include "cwCamera.h"
#include "cwCavingRegion.h"
#include "cwCoordinatePicker.h"
#include "cwGeometry.h"
#include "cwGeometryItersecter.h"
#include "cwProjection.h"
#include "cwRenderLinePlot.h"
#include "cwScene.h"

using namespace Catch;
using cwLinePlotStationSnap::snapToStation;

namespace {

// A perspective camera at the origin looking down -Z, so points at z < 0 are in
// front of the eye and points at z > 0 are behind it.
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
// (non-indexed pairs + iota indices), but keyed to a real render object so a
// hit's object() is the plot the picker casts to.
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

TEST_CASE("cwRenderLinePlot::segmentEndpoints returns shot vertex pairs",
          "[cwLinePlotStationSnap]")
{
    cwRenderLinePlot plot;

    // Two shots = four vertices: shot 0 owns [0,1], shot 1 owns [2,3].
    QVector<QVector3D> points;
    points << QVector3D(1.0f, 2.0f, 3.0f)
           << QVector3D(4.0f, 5.0f, 6.0f)
           << QVector3D(7.0f, 8.0f, 9.0f)
           << QVector3D(10.0f, 11.0f, 12.0f);
    plot.setGeometry(points);

    const auto shot0 = plot.segmentEndpoints(0);
    REQUIRE(shot0.has_value());
    CHECK(shot0->first == QVector3D(1.0f, 2.0f, 3.0f));
    CHECK(shot0->second == QVector3D(4.0f, 5.0f, 6.0f));

    const auto shot1 = plot.segmentEndpoints(2);
    REQUIRE(shot1.has_value());
    CHECK(shot1->first == QVector3D(7.0f, 8.0f, 9.0f));
    CHECK(shot1->second == QVector3D(10.0f, 11.0f, 12.0f));
}

TEST_CASE("cwRenderLinePlot::segmentEndpoints rejects out-of-range indices",
          "[cwLinePlotStationSnap]")
{
    cwRenderLinePlot plot;
    QVector<QVector3D> points;
    points << QVector3D(0.0f, 0.0f, 0.0f)
           << QVector3D(1.0f, 0.0f, 0.0f)
           << QVector3D(2.0f, 0.0f, 0.0f)
           << QVector3D(3.0f, 0.0f, 0.0f);
    plot.setGeometry(points);

    CHECK_FALSE(plot.segmentEndpoints(-1).has_value());
    // firstIndex 3 would read vertex 4, which doesn't exist.
    CHECK_FALSE(plot.segmentEndpoints(3).has_value());
    CHECK_FALSE(plot.segmentEndpoints(4).has_value());
    CHECK_FALSE(plot.segmentEndpoints(100).has_value());

    // An empty plot has no segments.
    cwRenderLinePlot empty;
    empty.setGeometry({});
    CHECK_FALSE(empty.segmentEndpoints(0).has_value());
}

TEST_CASE("cwLinePlotStationSnap: cursor near a station snaps to it",
          "[cwLinePlotStationSnap]")
{
    cwCamera camera;
    configureCamera(camera);

    const QVector3D a(-30.0f, 0.0f, -100.0f);
    const QVector3D b(30.0f, 0.0f, -100.0f);
    const QVector3D freePoint(0.0f, 0.0f, -100.0f);

    const auto onA = snapToStation(camera.project(a), camera, {a, 0}, {b, 1}, freePoint, 6.0);
    CHECK(onA.snapped);
    CHECK(onA.stationVertexIndex == 0);
    CHECK(onA.worldPos == a);

    const auto onB = snapToStation(camera.project(b), camera, {a, 0}, {b, 1}, freePoint, 6.0);
    CHECK(onB.snapped);
    CHECK(onB.stationVertexIndex == 1);
    CHECK(onB.worldPos == b);
}

TEST_CASE("cwLinePlotStationSnap: mid-segment cursor keeps the free point",
          "[cwLinePlotStationSnap]")
{
    cwCamera camera;
    configureCamera(camera);

    const QVector3D a(-30.0f, 0.0f, -100.0f);
    const QVector3D b(30.0f, 0.0f, -100.0f);
    const QVector3D freePoint(0.0f, 0.0f, -100.0f);

    const auto snap = snapToStation(camera.project(freePoint), camera,
                                    {a, 0}, {b, 1}, freePoint, 6.0);
    CHECK_FALSE(snap.snapped);
    CHECK(snap.stationVertexIndex == -1);
    CHECK(snap.worldPos == freePoint);
}

TEST_CASE("cwLinePlotStationSnap: a pixel-distance tie resolves to the lower index",
          "[cwLinePlotStationSnap]")
{
    cwCamera camera;
    configureCamera(camera);

    // Coincident endpoints project to the same pixel, an exact distance tie.
    const QVector3D p(5.0f, 0.0f, -100.0f);
    const QVector3D freePoint(0.0f, 0.0f, -100.0f);

    const auto snap = snapToStation(camera.project(p), camera, {p, 2}, {p, 7}, freePoint, 6.0);
    CHECK(snap.snapped);
    CHECK(snap.stationVertexIndex == 2);
}

TEST_CASE("cwLinePlotStationSnap: an endpoint behind the eye is ineligible",
          "[cwLinePlotStationSnap]")
{
    cwCamera camera;
    configureCamera(camera);

    // 'behind' is behind the camera (z > 0 with the eye looking down -Z). Its
    // projection lands somewhere, but it must never win a snap.
    const QVector3D behind(50.0f, 50.0f, 200.0f);
    const QVector3D offscreen(100000.0f, 0.0f, -100.0f);
    const QVector3D freePoint(0.0f, 0.0f, -100.0f);

    const auto snap = snapToStation(camera.project(behind), camera,
                                    {behind, 0}, {offscreen, 1}, freePoint, 6.0);
    CHECK_FALSE(snap.snapped);
    CHECK(snap.worldPos == freePoint);
}

TEST_CASE("cwCoordinatePicker: a click near a station reports the station coordinate",
          "[cwCoordinatePicker]")
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

    cwCavingRegion region;
    cwCamera camera;
    configureCamera(camera);

    cwCoordinatePicker picker;
    picker.setCamera(&camera);
    picker.setScene(&scene);
    picker.setRegion(&region);

    SECTION("click on a station snaps to its exact coordinate") {
        picker.pick(camera.project(a));
        REQUIRE(picker.hasPick());
        // The snap returns the stored vertex verbatim, so this is exact.
        CHECK(picker.scenePoint() == a);
    }

    SECTION("click mid-segment keeps the free point on the line") {
        const QVector3D mid(0.0f, 0.0f, -100.0f);
        picker.pick(camera.project(mid));
        REQUIRE(picker.hasPick());
        CHECK(picker.scenePoint().x() == Approx(mid.x()).margin(0.5));
        CHECK(picker.scenePoint().y() == Approx(mid.y()).margin(0.5));
        CHECK(picker.scenePoint().z() == Approx(mid.z()).margin(0.5));
        // Not snapped to either station.
        CHECK(picker.scenePoint() != a);
        CHECK(picker.scenePoint() != b);
    }
}
