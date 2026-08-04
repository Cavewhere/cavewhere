// test_cwLazLayerAutoAdoptCS.cpp
// Catch2 tests for cwLazLayerModel deriving the project's local projection from
// the first added LAZ when the project has no frame of its own.

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include "cwCavingRegion.h"
#include "cwFutureManagerModel.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwLocalProjection.h"
#include "cwProject.h"
#include "cwRootData.h"

#include "LazFixtureHelper.h"

namespace {

// Minimal valid OGC WKT for UTM Zone 10N (WGS 84). PROJ accepts this, which is
// all the derivation needs — it reads the header's CS and the bbox center and
// builds a transverse Mercator centered there.
const QString kUtmZone10N = QStringLiteral(
    "PROJCS[\"WGS 84 / UTM zone 10N\","
        "GEOGCS[\"WGS 84\","
            "DATUM[\"WGS_1984\","
                "SPHEROID[\"WGS 84\",6378137,298.257223563]],"
            "PRIMEM[\"Greenwich\",0],"
            "UNIT[\"degree\",0.0174532925199433]],"
        "PROJECTION[\"Transverse_Mercator\"],"
        "PARAMETER[\"latitude_of_origin\",0],"
        "PARAMETER[\"central_meridian\",-123],"
        "PARAMETER[\"scale_factor\",0.9996],"
        "PARAMETER[\"false_easting\",500000],"
        "PARAMETER[\"false_northing\",0],"
        "UNIT[\"metre\",1]]");

// bbox of minimalLazPoints() is (0,0,0)-(4,4,4), so the derivation anchors on
// its center.
const cwGeoPoint kBboxCenter{2.0, 2.0, 2.0};

} // namespace

TEST_CASE("Derive frame: empty project + LAZ with embedded CS anchors on the layer",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    auto* geoReference = region->geoReference();
    REQUIRE(geoReference->state() == cwGeoReference::Ungeoreferenced);

    QSignalSpy frameSpy(geoReference, &cwGeoReference::localProjectionChanged);

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("with-cs")),
        kUtmZone10N);
    REQUIRE(!path.isEmpty());

    addLazAndWait(root.get(), QStringList{path});

    REQUIRE(geoReference->state() == cwGeoReference::Anchored);
    CHECK(geoReference->anchor().kind == cwGeoReference::Anchor::LazLayer);
    REQUIRE(region->lazLayers()->count() == 1);
    CHECK(geoReference->anchor().id == region->lazLayers()->layerAt(0)->id());
    CHECK(geoReference->localCoordinateSystem()
          == cwLocalProjection::deriveFrom(kUtmZone10N, kBboxCenter));
    CHECK(frameSpy.size() >= 1);
}

TEST_CASE("Derive frame: a LAZ without an embedded CS leaves the project local",
          "[cwLazAutoAdoptCS]") {
    // Nothing says where the cloud is, so there is nothing to center a
    // projection on — the project stays in its floating local frame.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("no-cs")));
    REQUIRE(!path.isEmpty());

    addLazAndWait(root.get(), QStringList{path});

    CHECK(region->geoReference()->state() == cwGeoReference::Ungeoreferenced);
    CHECK(region->geoReference()->localCoordinateSystem().isEmpty());
}

TEST_CASE("Derive frame: a project that already has a frame is left untouched",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    auto* geoReference = region->geoReference();

    const QString existingFrame =
        cwLocalProjection::deriveFrom(QStringLiteral("EPSG:32611"),
                                      cwGeoPoint{500000.0, 4000000.0, 100.0});
    REQUIRE_FALSE(existingFrame.isEmpty());
    geoReference->restore(cwGeoReference::Frozen, existingFrame, {}, QString());

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("preset")),
        kUtmZone10N);
    REQUIRE(!path.isEmpty());

    addLazAndWait(root.get(), QStringList{path});

    CHECK(geoReference->localCoordinateSystem() == existingFrame);
}

TEST_CASE("Derive frame: a second add leaves the frame on the first layer",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    const QString first = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("first")), kUtmZone10N);
    addLazAndWait(root.get(), QStringList{first});

    const QString frameAfterFirst = region->geoReference()->localCoordinateSystem();
    REQUIRE_FALSE(frameAfterFirst.isEmpty());
    const QUuid anchorAfterFirst = region->geoReference()->anchor().id;

    const QString utm11n = QStringLiteral(
        "PROJCS[\"WGS 84 / UTM zone 11N\",GEOGCS[\"WGS 84\","
        "DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],"
        "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],"
        "PROJECTION[\"Transverse_Mercator\"],"
        "PARAMETER[\"latitude_of_origin\",0],"
        "PARAMETER[\"central_meridian\",-117],"
        "PARAMETER[\"scale_factor\",0.9996],"
        "PARAMETER[\"false_easting\",500000],"
        "PARAMETER[\"false_northing\",0],UNIT[\"metre\",1]]");

    const QString second = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("second")), utm11n);
    addLazAndWait(root.get(), QStringList{second});

    CHECK(region->geoReference()->localCoordinateSystem() == frameAfterFirst);
    CHECK(region->geoReference()->anchor().id == anchorAfterFirst);
}
