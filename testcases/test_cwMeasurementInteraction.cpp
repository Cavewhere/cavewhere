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
#include "cwAzimuthReference.h"
#include "cwCamera.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"
#include "cwGeometry.h"
#include "cwGeometryItersecter.h"
#include "cwGridConvergence.h"
#include "cwMath.h"
#include "cwProjection.h"
#include "cwRenderLinePlot.h"
#include "cwScene.h"

#include <QSettings>

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

TEST_CASE("cwMeasurementInteraction resolves the azimuth north reference",
          "[cwMeasurementInteraction]")
{
    // Each test process has its own PID-scoped QSettings, cleared at startup, so
    // a fresh interaction defaults to Grid; clear again to undo prior sections.
    QSettings().clear();

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

    // UTM 13N with an origin well east of the central meridian, so the grid
    // convergence is clearly non-zero and positive.
    const QString cs = QStringLiteral("EPSG:32613");
    cwCoordinateTransform geoToUtm(QStringLiteral("EPSG:4326"), cs);
    REQUIRE(geoToUtm.isValid());
    const cwGeoPoint origin = geoToUtm.transform(cwGeoPoint(-104.0, 40.015, 1655.0));

    cwCavingRegion region;
    region.setGlobalCoordinateSystem(cs);
    region.setWorldOrigin(origin);

    cwMeasurementInteraction interaction;
    interaction.setCamera(&camera);
    interaction.setScene(&scene);
    interaction.setRegion(&region);

    // Place A then B: 60 m due east, so the grid azimuth is 90°.
    interaction.place(camera.project(a));
    interaction.place(camera.project(b));
    REQUIRE(interaction.hasMeasurement());
    REQUIRE(interaction.azimuth() == Approx(90.0));

    // The location the resolver sees is the first point plus the worldOrigin.
    const cwGeoPoint location(double(a.x()) + origin.x,
                              double(a.y()) + origin.y,
                              double(a.z()) + origin.z);

    SECTION("Grid is the default and mirrors the grid azimuth") {
        CHECK(interaction.azimuthReference() == cwAzimuthReference::Reference::Grid);
        CHECK(interaction.geoReferenced());
        CHECK(interaction.referenceAvailable());
        CHECK(interaction.referenceAzimuth() == Approx(interaction.azimuth()));
        CHECK(interaction.convergence() == Approx(0.0));
        CHECK(interaction.declination() == Approx(0.0));
        CHECK(interaction.referenceReason().isEmpty());
    }

    SECTION("True applies the grid convergence") {
        auto convergence = cwGridConvergence::computeAt(location, cs);
        REQUIRE_FALSE(convergence.hasError());
        REQUIRE(convergence.value() > 0.0); // east of the central meridian

        interaction.setAzimuthReference(cwAzimuthReference::Reference::True);
        CHECK(interaction.referenceAvailable());
        CHECK(interaction.convergence() == Approx(convergence.value()));
        CHECK(interaction.referenceAzimuth()
              == Approx(cwWrapDegrees360(interaction.azimuth() + convergence.value())));
        CHECK(interaction.declination() == Approx(0.0));
    }

    SECTION("Magnetic resolves with a non-zero declination") {
        interaction.setAzimuthReference(cwAzimuthReference::Reference::Magnetic);
        CHECK(interaction.referenceAvailable());
        CHECK(interaction.referenceReason().isEmpty());
        // Boulder has a real declination today; the exact value is date-dependent
        // and exercised precisely in test_cwAzimuthReference.
        CHECK(interaction.declination() != 0.0);
    }

    SECTION("switching the selection re-resolves the bearing") {
        const double gridBearing = interaction.referenceAzimuth();
        interaction.setAzimuthReference(cwAzimuthReference::Reference::True);
        CHECK(interaction.referenceAzimuth() != Approx(gridBearing));
    }

    SECTION("losing the coordinate system snaps the selection back to Grid") {
        interaction.setAzimuthReference(cwAzimuthReference::Reference::True);
        REQUIRE(interaction.azimuthReference() == cwAzimuthReference::Reference::True);

        region.setGlobalCoordinateSystem(QString());
        CHECK_FALSE(interaction.geoReferenced());
        CHECK(interaction.azimuthReference() == cwAzimuthReference::Reference::Grid);
        CHECK(interaction.referenceAvailable());
        CHECK(interaction.referenceAzimuth() == Approx(interaction.azimuth()));
    }

    SECTION("a local-only project can't select True or Magnetic") {
        region.setGlobalCoordinateSystem(QString());
        REQUIRE_FALSE(interaction.geoReferenced());

        interaction.setAzimuthReference(cwAzimuthReference::Reference::True);
        CHECK(interaction.azimuthReference() == cwAzimuthReference::Reference::Grid);

        interaction.setAzimuthReference(cwAzimuthReference::Reference::Magnetic);
        CHECK(interaction.azimuthReference() == cwAzimuthReference::Reference::Grid);
        CHECK(interaction.referenceAvailable());
    }

    SECTION("a present but invalid coordinate system reports n/a") {
        // geoReferenced is true (the CRS is non-empty) so True stays selectable,
        // but the resolve fails and surfaces as n/a for the readout.
        region.setGlobalCoordinateSystem(QStringLiteral("EPSG:999999"));
        REQUIRE(interaction.geoReferenced());

        interaction.setAzimuthReference(cwAzimuthReference::Reference::True);
        CHECK(interaction.azimuthReference() == cwAzimuthReference::Reference::True);
        CHECK_FALSE(interaction.referenceAvailable());
        CHECK_FALSE(interaction.referenceReason().isEmpty());
        CHECK(interaction.referenceAzimuth() == Approx(0.0));
    }

    SECTION("the selected reference persists across instances") {
        interaction.setAzimuthReference(cwAzimuthReference::Reference::True);
        REQUIRE(interaction.azimuthReference() == cwAzimuthReference::Reference::True);

        // A fresh interaction reads the persisted choice back in its constructor.
        cwMeasurementInteraction reopened;
        CHECK(reopened.azimuthReference() == cwAzimuthReference::Reference::True);
    }

    SECTION("without a region True/Magnetic are not selectable") {
        cwMeasurementInteraction noRegion;
        noRegion.setCamera(&camera);
        noRegion.setScene(&scene);
        noRegion.place(camera.project(a));
        noRegion.place(camera.project(b));
        CHECK_FALSE(noRegion.geoReferenced());
        noRegion.setAzimuthReference(cwAzimuthReference::Reference::True);
        CHECK(noRegion.azimuthReference() == cwAzimuthReference::Reference::Grid);
        CHECK(noRegion.referenceAvailable());
    }
}
