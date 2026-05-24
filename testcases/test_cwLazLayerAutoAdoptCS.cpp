// test_cwLazLayerAutoAdoptCS.cpp
// Catch2 tests for cwLazLayerModel auto-adoption of project CS + worldOrigin
// from the first added LAZ when the project is empty.

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include "cwCavingRegion.h"
#include "cwFutureManagerModel.h"
#include "cwGeoPoint.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"
#include "cwProject.h"
#include "cwRootData.h"

#include "LazFixtureHelper.h"

namespace {

// Minimal valid OGC WKT for UTM Zone 10N (WGS 84). PROJ accepts this; for
// these tests only the round-trip string equality matters — the loader
// constructs cwCoordinateTransform(sourceCS, globalCS) with the same string
// on both sides, so the transform is identity and points keep their raw
// coordinates from the .laz fixture.
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

} // namespace

TEST_CASE("Auto-adopt: empty project + LAZ with embedded CS adopts both",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    REQUIRE(region->globalCoordinateSystem().isEmpty());
    REQUIRE(region->worldOrigin() == cwGeoPoint{});

    QSignalSpy csSpy(region, &cwCavingRegion::globalCoordinateSystemChanged);
    QSignalSpy originSpy(region, &cwCavingRegion::worldOriginChanged);

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("with-cs")),
        kUtmZone10N);
    REQUIRE(!path.isEmpty());

    addLazAndWait(root.get(), QStringList{path});

    REQUIRE(region->globalCoordinateSystem() == kUtmZone10N);
    REQUIRE(csSpy.size() == 1);

    // bbox of minimalLazPoints() is (0,0,0)-(4,4,4) → center (2,2,2).
    const cwGeoPoint expectedCenter{2.0, 2.0, 2.0};
    REQUIRE(region->worldOrigin() == expectedCenter);
    REQUIRE(originSpy.size() >= 1);
}

TEST_CASE("Auto-adopt: empty project + LAZ without embedded CS only sets worldOrigin",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("no-cs")));
    REQUIRE(!path.isEmpty());

    addLazAndWait(root.get(), QStringList{path});

    REQUIRE(region->globalCoordinateSystem().isEmpty());
    REQUIRE(region->worldOrigin() == cwGeoPoint{2.0, 2.0, 2.0});
}

TEST_CASE("Auto-adopt: project with existing globalCoordinateSystem is left untouched",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();
    const QString existingCS = QStringLiteral("EPSG:32611");  // UTM 11N
    const cwGeoPoint existingOrigin{500000.0, 4000000.0, 100.0};
    region->setGlobalCoordinateSystem(existingCS);    // resets worldOrigin to {}
    region->setWorldOrigin(existingOrigin);

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("preset")),
        kUtmZone10N);
    REQUIRE(!path.isEmpty());

    addLazAndWait(root.get(), QStringList{path});

    REQUIRE(region->globalCoordinateSystem() == existingCS);
    REQUIRE(region->worldOrigin() == existingOrigin);
}

TEST_CASE("Auto-adopt: explicit setWorldOrigin(0,0,0) is honored, not overwritten",
          "[cwLazAutoAdoptCS]") {
    // Regression: cwGeoPoint{} == cwGeoPoint(0,0,0) by value, so a
    // value-equality check in maybeAdoptRegionDefaultsFromLaz can't tell an
    // explicit pin-to-origin from "never set." cwCavingRegion now tracks an
    // explicit-set flag; this test guards that the flag actually blocks the
    // auto-adopt path. Caller chose (0,0,0) on purpose (the
    // sink-training tests do this so render-XY == LAZ-source-XY), and the
    // origin must not be silently moved to the LAZ bbox center.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    const QString explicitCS = QStringLiteral("EPSG:32611");
    region->setGlobalCoordinateSystem(explicitCS);
    region->setWorldOrigin(cwGeoPoint{0.0, 0.0, 0.0});
    REQUIRE(region->hasExplicitWorldOrigin());

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("zero-pin")),
        kUtmZone10N);
    REQUIRE(!path.isEmpty());

    addLazAndWait(root.get(), QStringList{path});

    REQUIRE(region->globalCoordinateSystem() == explicitCS);
    REQUIRE(region->worldOrigin() == cwGeoPoint{0.0, 0.0, 0.0});
    REQUIRE(region->hasExplicitWorldOrigin());
}

TEST_CASE("Auto-adopt: setGlobalCoordinateSystem clears the explicit-set flag",
          "[cwLazAutoAdoptCS]") {
    // setGlobalCoordinateSystem internally resets worldOrigin because the
    // stored value was in the old CS. That reset is not a user choice, so
    // the flag must drop back to false — otherwise the convenient
    // bbox-center auto-adopt would be permanently disabled after the very
    // first CS change.
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    region->setGlobalCoordinateSystem(QStringLiteral("EPSG:32611"));
    region->setWorldOrigin(cwGeoPoint{500000.0, 4000000.0, 100.0});
    REQUIRE(region->hasExplicitWorldOrigin());

    // Change CS → both the value and the flag are reset.
    region->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));
    REQUIRE(region->worldOrigin() == cwGeoPoint{});
    REQUIRE_FALSE(region->hasExplicitWorldOrigin());

    // A LAZ added afterwards may seed the origin from its bbox center.
    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("post-cs-reset")));
    REQUIRE(!path.isEmpty());
    addLazAndWait(root.get(), QStringList{path});

    REQUIRE(region->worldOrigin() == cwGeoPoint{2.0, 2.0, 2.0});
    REQUIRE(region->hasExplicitWorldOrigin());
}

TEST_CASE("Auto-adopt: second add to fresh project leaves CS unchanged",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    auto root = std::make_unique<cwRootData>();
    auto* region = root->project()->cavingRegion();

    const QString first = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("first")), kUtmZone10N);
    addLazAndWait(root.get(), QStringList{first});

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

    // First add wins; subsequent adds skip auto-adopt because the CS is set.
    REQUIRE(region->globalCoordinateSystem() == kUtmZone10N);
}
