//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFixStation.h"
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
    QSignalSpy spy(&region, &cwCavingRegion::worldOriginChanged);

    region.recomputeWorldOrigin();

    CHECK(region.worldOrigin() == cwGeoPoint{});
    CHECK(spy.count() == 0);
}

TEST_CASE("recomputeWorldOrigin sets worldOrigin to a single fix's coords",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    region.setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:32612"),
                500000.0, 4194000.0, 2700.0));

    QSignalSpy spy(&region, &cwCavingRegion::worldOriginChanged);
    region.recomputeWorldOrigin();

    REQUIRE(spy.count() == 1);
    const cwGeoPoint origin = region.worldOrigin();
    CHECK_THAT(origin.x, WithinAbs(500000.0, 1e-6));
    CHECK_THAT(origin.y, WithinAbs(4194000.0, 1e-6));
    CHECK_THAT(origin.z, WithinAbs(2700.0, 1e-6));
}

TEST_CASE("recomputeWorldOrigin averages multiple fixes",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    region.setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

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

    const cwGeoPoint origin = region.worldOrigin();
    CHECK_THAT(origin.x, WithinAbs(500100.0, 1e-6));
    CHECK_THAT(origin.y, WithinAbs(4194050.0, 1e-6));
    CHECK_THAT(origin.z, WithinAbs(2725.0, 1e-6));
}

TEST_CASE("recomputeWorldOrigin falls back to globalCS when fix inputCS is empty",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    region.setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QString(),
                500000.0, 4194000.0, 2700.0));

    region.recomputeWorldOrigin();

    CHECK_THAT(region.worldOrigin().x, WithinAbs(500000.0, 1e-6));
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

    QSignalSpy spy(&region, &cwCavingRegion::worldOriginChanged);
    region.recomputeWorldOrigin();

    CHECK(region.worldOrigin() == cwGeoPoint{});
    CHECK(spy.count() == 0);
}

TEST_CASE("recomputeWorldOrigin reprojects when fix inputCS differs from globalCS",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    region.setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

    region.addCave();
    auto* cave = region.cave(0);
    REQUIRE(cave != nullptr);
    // Lat/lon WGS84 — same point as roughly UTM 12N 500000 / 4194000.
    cave->fixStations()->appendFixStation(
        makeFix(QStringLiteral("A1"), QStringLiteral("EPSG:4326"),
                -111.0, 37.871, 2700.0));

    region.recomputeWorldOrigin();

    const cwGeoPoint origin = region.worldOrigin();
    // Sanity: UTM 12N origin near 500000 east, 4193000-ish north.
    CHECK_THAT(origin.x, WithinAbs(500000.0, 5000.0));
    CHECK_THAT(origin.y, WithinAbs(4193000.0, 5000.0));
}

TEST_CASE("setGlobalCS resets worldOrigin so the next solve re-arms auto-compute",
          "[cwCavingRegion][worldOrigin]")
{
    cwCavingRegion region;
    region.setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));
    region.setWorldOrigin(cwGeoPoint{500000.0, 4194000.0, 2700.0});
    REQUIRE(region.worldOrigin() != cwGeoPoint{});

    QSignalSpy spy(&region, &cwCavingRegion::worldOriginChanged);
    region.setGlobalCoordinateSystem(QStringLiteral("EPSG:32613"));

    CHECK(region.worldOrigin() == cwGeoPoint{});
    CHECK(spy.count() == 1);
}
