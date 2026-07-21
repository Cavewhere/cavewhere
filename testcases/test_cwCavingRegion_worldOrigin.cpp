//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
#include "cwGeoReference.h"
#include "cwFixStationModel.h"
#include "cwGeoPoint.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Qt includes
#include <QSignalSpy>

using Catch::Matchers::WithinAbs;

namespace {

cwFixStation makeFix(const QString& name,
                     const QString& cs,
                     double easting,
                     double northing,
                     double elevation)
{
    cwFixStation fix;
    fix.setStationName(name);
    fix.setInputCS(cs);
    fix.setEasting(easting);
    fix.setNorthing(northing);
    fix.setElevation(elevation);
    return fix;
}

} // namespace

TEST_CASE("recomputeWorldOrigin is a no-op on an empty region",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    QSignalSpy spy(region.geoReference(), &cwGeoReference::worldOriginChanged);

    region.recomputeWorldOrigin();

    CHECK(region.geoReference()->worldOrigin() == cwGeoPoint{});
    CHECK(spy.count() == 0);
}

TEST_CASE("recomputeWorldOrigin sets worldOrigin to a single fix's coords",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"),
                500000.0, 4194000.0, 2700.0));

    QSignalSpy spy(region.geoReference(), &cwGeoReference::worldOriginChanged);
    region.recomputeWorldOrigin();

    REQUIRE(spy.count() == 1);
    const cwGeoPoint origin = region.geoReference()->worldOrigin();
    CHECK_THAT(origin.x, WithinAbs(500000.0, 1e-6));
    CHECK_THAT(origin.y, WithinAbs(4194000.0, 1e-6));
    CHECK_THAT(origin.z, WithinAbs(2700.0, 1e-6));
}

TEST_CASE("recomputeWorldOrigin averages multiple fixes",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"),
                500000.0, 4194000.0, 2700.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("B1"), QStringLiteral("EPSG:32612"),
                500200.0, 4194100.0, 2750.0));

    region.recomputeWorldOrigin();

    const cwGeoPoint origin = region.geoReference()->worldOrigin();
    CHECK_THAT(origin.x, WithinAbs(500100.0, 1e-6));
    CHECK_THAT(origin.y, WithinAbs(4194050.0, 1e-6));
    CHECK_THAT(origin.z, WithinAbs(2725.0, 1e-6));
}

TEST_CASE("recomputeWorldOrigin ignores an outlier fix when centering",
          "[cwCavingRegion][worldOrigin]")
{
    // Four good fixes in a tight cluster plus one typo'd fix ~1000 km north
    // (a mistyped leading digit in the northing). The origin must be the
    // centroid of the four inliers only — averaging the outlier in would drag
    // it hundreds of km off the real data and render the cave as a sub-pixel dot.
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"),
                500000.0, 4194000.0, 2700.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A2"), QStringLiteral("EPSG:32612"),
                500200.0, 4194100.0, 2710.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A3"), QStringLiteral("EPSG:32612"),
                499900.0, 4193900.0, 2705.0));
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A4"), QStringLiteral("EPSG:32612"),
                500100.0, 4193950.0, 2708.0));
    // The typo: northing's leading 4 mistyped as 5 → ~5.19 million, ~1000 km away.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("Bad"), QStringLiteral("EPSG:32612"),
                500150.0, 5194000.0, 2708.0));

    region.recomputeWorldOrigin();

    const cwGeoPoint origin = region.geoReference()->worldOrigin();
    // Centroid of the four inliers only (the outlier's 5.19M northing is absent).
    CHECK_THAT(origin.x, WithinAbs(500050.0, 1e-6));
    CHECK_THAT(origin.y, WithinAbs(4193987.5, 1e-6));
    CHECK_THAT(origin.z, WithinAbs(2705.75, 1e-6));
}

TEST_CASE("recomputeWorldOrigin falls back to globalCS when fix inputCS is empty",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QString(),
                500000.0, 4194000.0, 2700.0));

    region.recomputeWorldOrigin();

    CHECK_THAT(region.geoReference()->worldOrigin().x, WithinAbs(500000.0, 1e-6));
}

TEST_CASE("recomputeWorldOrigin skips fixes with no resolvable CS",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    // No globalCS set, fix has no inputCS — nothing to project against.

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QString(),
                500000.0, 4194000.0, 2700.0));

    QSignalSpy spy(region.geoReference(), &cwGeoReference::worldOriginChanged);
    region.recomputeWorldOrigin();

    CHECK(region.geoReference()->worldOrigin() == cwGeoPoint{});
    CHECK(spy.count() == 0);
}

TEST_CASE("recomputeWorldOrigin reprojects when fix inputCS differs from globalCS",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    // Lat/lon WGS84 — same point as roughly UTM 12N 500000 / 4194000.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:4326"),
                -111.0, 37.871, 2700.0));

    region.recomputeWorldOrigin();

    const cwGeoPoint origin = region.geoReference()->worldOrigin();
    // Sanity: UTM 12N origin near 500000 east, 4193000-ish north.
    CHECK_THAT(origin.x, WithinAbs(500000.0, 5000.0));
    CHECK_THAT(origin.y, WithinAbs(4193000.0, 5000.0));
}

TEST_CASE("setGlobalCS resets worldOrigin so the next solve re-arms auto-compute",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));
    region.geoReference()->setWorldOrigin(cwGeoPoint{500000.0, 4194000.0, 2700.0});
    REQUIRE(region.geoReference()->worldOrigin() != cwGeoPoint{});

    QSignalSpy spy(region.geoReference(), &cwGeoReference::worldOriginChanged);
    region.geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32613"));

    CHECK(region.geoReference()->worldOrigin() == cwGeoPoint{});
    CHECK(spy.count() == 1);
}
