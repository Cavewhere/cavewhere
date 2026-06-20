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
#include "cwMeasurementInteraction.h"
#include "cwCamera.h"
#include "cwGeometry.h"
#include "cwGeometryItersecter.h"
#include "cwProjection.h"
#include "cwRenderLinePlot.h"
#include "cwScene.h"

using namespace Catch;

namespace {

// A perspective camera at the origin looking down -Z, so points at z < 0 are in
// front of the eye (matching test_cwScenePick).
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

// Registers a centerline segment in the BVH the way cwRenderLinePlot does
// (non-indexed pairs + iota indices) so a hit's object() casts back to the plot.
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

TEST_CASE("cwMeasurementInteraction measures between two stations", "[cwMeasurementInteraction]")
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

    cwMeasurementInteraction interaction;
    interaction.setCamera(&camera);
    interaction.setScene(&scene);

    const QVector3D mid(0.0f, 0.0f, -100.0f);

    SECTION("hover near a station reports a valid snap") {
        interaction.hover(camera.project(a));
        CHECK(interaction.hoverSnapped());
        CHECK(interaction.hoverValid());
        CHECK(interaction.hoverPoint() == a);
    }

    SECTION("placing A then B fills the readout") {
        interaction.place(camera.project(a));
        CHECK(interaction.hasFirst());
        CHECK_FALSE(interaction.hasMeasurement());
        CHECK(interaction.firstPoint() == a);

        interaction.place(camera.project(b));
        CHECK(interaction.hasMeasurement());
        CHECK(interaction.secondPoint() == b);
        CHECK(interaction.distance() == Approx(60.0));
        CHECK(interaction.horizontal() == Approx(60.0));
        CHECK(interaction.vertical() == Approx(0.0).margin(1e-6));
        CHECK(interaction.azimuth() == Approx(90.0));     // due east (+X)
        CHECK(interaction.inclination() == Approx(0.0).margin(1e-6));
        CHECK(interaction.deltaEast() == Approx(60.0));
        CHECK(interaction.deltaNorth() == Approx(0.0).margin(1e-6));
    }

    SECTION("awaiting-second hover previews the live measurement") {
        interaction.place(camera.project(a));
        interaction.hover(camera.project(b));
        CHECK_FALSE(interaction.hasMeasurement());        // not frozen until placed
        CHECK(interaction.distance() == Approx(60.0));     // but previewed
        CHECK(interaction.azimuth() == Approx(90.0));
    }

    SECTION("Station-only mode rejects a non-station click") {
        interaction.setMode(cwMeasurementInteraction::Mode::StationOnly);

        // Mid-segment: hits the centerline but is not within snap range of a
        // station, so it must not place a point.
        interaction.place(camera.project(mid));
        CHECK_FALSE(interaction.hasFirst());

        // Empty space misses entirely.
        interaction.place(QPointF(1.0, 1.0));
        CHECK_FALSE(interaction.hasFirst());

        // A click on a station still places.
        interaction.place(camera.project(a));
        CHECK(interaction.hasFirst());
    }

    SECTION("a click after Complete restarts the measurement") {
        interaction.place(camera.project(a));
        interaction.place(camera.project(b));
        REQUIRE(interaction.hasMeasurement());

        interaction.place(camera.project(b));
        CHECK(interaction.hasFirst());
        CHECK_FALSE(interaction.hasMeasurement());
        CHECK(interaction.firstPoint() == b);
    }

    SECTION("reset clears the measurement") {
        interaction.place(camera.project(a));
        interaction.place(camera.project(b));
        REQUIRE(interaction.hasMeasurement());

        interaction.reset();
        CHECK_FALSE(interaction.hasFirst());
        CHECK_FALSE(interaction.hasMeasurement());
        CHECK(interaction.distance() == Approx(0.0).margin(1e-9));
    }
}
