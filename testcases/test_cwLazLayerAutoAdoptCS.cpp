// test_cwLazLayerAutoAdoptCS.cpp
// Catch2 tests for cwLazLayerModel auto-adoption of project CS + worldOrigin
// from the first added LAZ when the project is empty.

#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "cwCavingRegion.h"
#include "cwGeoPoint.h"
#include "cwLazLayer.h"
#include "cwLazLayerModel.h"

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

    cwCavingRegion region;
    REQUIRE(region.globalCS().isEmpty());
    REQUIRE(region.worldOrigin() == cwGeoPoint{});

    QSignalSpy csSpy(&region, &cwCavingRegion::globalCSChanged);
    QSignalSpy originSpy(&region, &cwCavingRegion::worldOriginChanged);

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("with-cs")),
        kUtmZone10N);
    REQUIRE(!path.isEmpty());

    region.lazLayers()->addLayer(path);

    REQUIRE(region.globalCS() == kUtmZone10N);
    REQUIRE(csSpy.size() == 1);

    // bbox of minimalLazPoints() is (0,0,0)-(4,4,4) → center (2,2,2).
    const cwGeoPoint expectedCenter{2.0, 2.0, 2.0};
    REQUIRE(region.worldOrigin() == expectedCenter);
    REQUIRE(originSpy.size() >= 1);
}

TEST_CASE("Auto-adopt: empty project + LAZ without embedded CS only sets worldOrigin",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwCavingRegion region;

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("no-cs")));
    REQUIRE(!path.isEmpty());

    region.lazLayers()->addLayer(path);

    REQUIRE(region.globalCS().isEmpty());
    REQUIRE(region.worldOrigin() == cwGeoPoint{2.0, 2.0, 2.0});
}

TEST_CASE("Auto-adopt: project with existing globalCS is left untouched",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwCavingRegion region;
    const QString existingCS = QStringLiteral("EPSG:32611");  // UTM 11N
    const cwGeoPoint existingOrigin{500000.0, 4000000.0, 100.0};
    region.setGlobalCS(existingCS);                  // resets worldOrigin to {}
    region.setWorldOrigin(existingOrigin);

    const QString path = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("preset")),
        kUtmZone10N);
    REQUIRE(!path.isEmpty());

    region.lazLayers()->addLayer(path);

    REQUIRE(region.globalCS() == existingCS);
    REQUIRE(region.worldOrigin() == existingOrigin);
}

TEST_CASE("Auto-adopt: second add to fresh project leaves CS unchanged",
          "[cwLazAutoAdoptCS]") {
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    cwCavingRegion region;

    const QString first = writeMinimalLaz(
        tempLazPath(tempDir, QStringLiteral("first")), kUtmZone10N);
    region.lazLayers()->addLayer(first);

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
    region.lazLayers()->addLayer(second);

    // First add wins; subsequent adds skip auto-adopt because globalCS is set.
    REQUIRE(region.globalCS() == kUtmZone10N);
}
