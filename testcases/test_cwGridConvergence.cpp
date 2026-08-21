/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch2 includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoPoint.h"
#include "cwGeoReference.h"
#include "cwGridConvergence.h"
#include "FixStationFixtureHelper.h"

//Qt includes
#include <QtMath>

//Std includes
#include <memory>

using Catch::Matchers::WithinAbs;

namespace {
    const QString Wgs84 = QStringLiteral("EPSG:4326");
    const QString Utm13N = QStringLiteral("EPSG:32613"); // central meridian -105°

    // On the central meridian at latitude ~40°, where UTM's convergence is 0 and
    // eastings measure straight out from it.
    constexpr double kCentralMeridianEasting = 500000.0;
    constexpr double kNorthing = 4430000.0;
    constexpr double kElevation = 1655.0;

    cwGeoPoint utm13NFor(double lon, double lat, double elev)
    {
        cwCoordinateTransform geoToUtm(Wgs84, Utm13N);
        REQUIRE(geoToUtm.isValid());
        return geoToUtm.transform(cwGeoPoint(lon, lat, elev));
    }

    cwFixStation utmFix(const QString& name, double easting)
    {
        return makeFix(name, Utm13N, easting, kNorthing, kElevation);
    }

    cwCave* addCaveFixedAt(cwCavingRegion* region, const QString& name, double easting)
    {
        return addCaveWithFixes(region, {utmFix(name, easting)});
    }

    //! What γ ≈ Δlon · sin(lat) works out to for a cave \a metersEast of the
    //! frame's origin at this latitude — the whole of the convergence a local
    //! projection can accumulate, since its central meridian runs through the
    //! anchor.
    double expectedConvergenceFor(double metersEast)
    {
        constexpr double kLatitudeDegrees = 40.0;
        constexpr double kMetersPerDegreeLongitude =
            111320.0 * 0.766; // cos(40°)
        return (metersEast / kMetersPerDegreeLongitude)
               * qSin(qDegreesToRadians(kLatitudeDegrees));
    }
}

TEST_CASE("cwGridConvergence: UTM 13N near central meridian gives a small angle",
          "[cwGridConvergence]")
{
    // Boulder, CO at lon=-105.27 is ~0.27° west of UTM Z13N's central
    // meridian (-105°). PROJ's east-positive convention gives
    //   γ ≈ sin(lat) · Δlon  (lat 40°, Δlon -0.27°) → ~-0.174°
    // Tolerance kept loose to allow for the higher-order terms PROJ
    // folds in.
    const cwGeoPoint p = utm13NFor(-105.27, 40.015, 1655.0);
    auto result = cwGridConvergence::computeAt(p, Utm13N);
    REQUIRE_FALSE(result.hasError());
    CHECK_THAT(result.value(), WithinAbs(-0.174, 0.02));
}

TEST_CASE("cwGridConvergence: convergence varies measurably with location inside the same UTM zone",
          "[cwGridConvergence]")
{
    // Same projection, two points 3° apart in longitude → convergence
    // should differ by ~sin(lat)·3° ≈ 1.93°. This is the load-bearing
    // case for the per-cave readout: a single region-wide value would
    // be wrong.
    const cwGeoPoint nearMeridian = utm13NFor(-105.10, 40.015, 1655.0);
    const cwGeoPoint nearZoneEdge = utm13NFor(-108.00, 40.015, 1655.0);

    auto a = cwGridConvergence::computeAt(nearMeridian, Utm13N);
    auto b = cwGridConvergence::computeAt(nearZoneEdge, Utm13N);
    REQUIRE_FALSE(a.hasError());
    REQUIRE_FALSE(b.hasError());

    CHECK(qAbs(b.value() - a.value()) > 1.5);
}

TEST_CASE("cwGridConvergence: geographic CRS has no grid and returns 0",
          "[cwGridConvergence]")
{
    auto result = cwGridConvergence::computeAt(cwGeoPoint(-105.27, 40.015, 0.0), Wgs84);
    REQUIRE_FALSE(result.hasError());
    CHECK(result.value() == 0.0);
}

TEST_CASE("cwGridConvergence: empty source CS returns error",
          "[cwGridConvergence]")
{
    auto result = cwGridConvergence::computeAt(cwGeoPoint(0.0, 0.0, 0.0), QString());
    CHECK(result.hasError());
    CHECK(result.errorMessage().contains("coordinate", Qt::CaseInsensitive));
}

TEST_CASE("cwGridConvergence: unknown source CS returns error",
          "[cwGridConvergence]")
{
    auto result = cwGridConvergence::computeAt(cwGeoPoint(0.0, 0.0, 0.0),
                                               QStringLiteral("EPSG:999999"));
    CHECK(result.hasError());
}

TEST_CASE("cwGridConvergence: sign convention matches the central-meridian rule",
          "[cwGridConvergence]")
{
    // East of the central meridian, grid north lies east of true north;
    // PROJ reports convergence as positive in this case (factors.meridian_convergence > 0).
    const cwGeoPoint east = utm13NFor(-104.0, 40.015, 1655.0);   // east of -105°
    const cwGeoPoint west = utm13NFor(-106.0, 40.015, 1655.0);   // west of -105°

    auto eastResult = cwGridConvergence::computeAt(east, Utm13N);
    auto westResult = cwGridConvergence::computeAt(west, Utm13N);
    REQUIRE_FALSE(eastResult.hasError());
    REQUIRE_FALSE(westResult.hasError());

    CHECK(eastResult.value() > 0.0);
    CHECK(westResult.value() < 0.0);
}

TEST_CASE("cwGridConvergence: the readout converges to the project's frame, not the fix's own CS",
          "[cwGridConvergence]")
{
    // A cave 100 km east of UTM 13N's central meridian. Under UTM its grid north
    // is rotated most of a degree, but nothing is ever plotted in UTM: cavern
    // solves in the project's local projection, and this fix is what that
    // projection is centered on. Grid north there is true north.
    constexpr double kEastOfMeridian = 100000.0;

    cwCavingRegion region;
    cwCave* cave = addCaveFixedAt(&region, QStringLiteral("a1"),
                                  kCentralMeridianEasting + kEastOfMeridian);

    const auto utmAngle = cwGridConvergence::computeAt(
        cwGeoPoint(kCentralMeridianEasting + kEastOfMeridian, kNorthing, kElevation), Utm13N);
    REQUIRE_FALSE(utmAngle.hasError());
    REQUIRE(utmAngle.value() > 0.5);

    REQUIRE(region.geoReference()->state() == cwGeoReference::Anchored);
    CHECK(cave->gridConvergence()->state() == cwGridConvergence::Valid);
    CHECK_THAT(cave->gridConvergence()->angle(), WithinAbs(0.0, 1e-6));
}

TEST_CASE("cwGridConvergence: a cave away from the anchor converges by its offset",
          "[cwGridConvergence]")
{
    // The anchor cave is added first, so the frame is centered on it; the second
    // cave sits 34 km east of that origin and keeps a real, if small, rotation.
    constexpr double kOffsetEast = 34000.0;

    cwCavingRegion region;
    cwCave* anchorCave = addCaveFixedAt(&region, QStringLiteral("a1"), kCentralMeridianEasting);
    cwCave* eastCave = addCaveFixedAt(&region, QStringLiteral("b1"),
                                      kCentralMeridianEasting + kOffsetEast);

    CHECK(anchorCave->gridConvergence()->state() == cwGridConvergence::Valid);
    CHECK_THAT(anchorCave->gridConvergence()->angle(), WithinAbs(0.0, 1e-6));

    CHECK(eastCave->gridConvergence()->state() == cwGridConvergence::Valid);
    CHECK_THAT(eastCave->gridConvergence()->angle(),
               WithinAbs(expectedConvergenceFor(kOffsetEast), 0.01));
}

TEST_CASE("cwGridConvergence: moving the frame recomputes the caves that didn't move",
          "[cwGridConvergence]")
{
    // Correcting the anchor by more than the frame can absorb re-derives it, and
    // every other cave's convergence is measured from the new origin — so the
    // cave that stayed put swaps sides.
    constexpr double kOffsetEast = 34000.0;
    constexpr double kCorrectionEast = 68000.0;

    cwCavingRegion region;
    cwCave* anchorCave = addCaveFixedAt(&region, QStringLiteral("a1"), kCentralMeridianEasting);
    cwCave* eastCave = addCaveFixedAt(&region, QStringLiteral("b1"),
                                      kCentralMeridianEasting + kOffsetEast);
    REQUIRE_THAT(eastCave->gridConvergence()->angle(),
                 WithinAbs(expectedConvergenceFor(kOffsetEast), 0.01));

    // Edit the row rather than replacing the fix: the frame follows the anchor
    // by identity, and a fresh cwFixStation would read as the anchor's deletion.
    cwFixStationModel* fixes = anchorCave->fixStations();
    REQUIRE(fixes->setData(fixes->index(0),
                           kCentralMeridianEasting + kCorrectionEast,
                           cwFixStationModel::EastingRole));

    CHECK_THAT(anchorCave->gridConvergence()->angle(), WithinAbs(0.0, 1e-6));
    CHECK_THAT(eastCave->gridConvergence()->angle(),
               WithinAbs(expectedConvergenceFor(kOffsetEast - kCorrectionEast), 0.01));
}

TEST_CASE("cwGridConvergence: a cave outside a region has no grid to converge to",
          "[cwGridConvergence]")
{
    auto cave = std::make_unique<cwCave>();
    cave->fixStations()->setFixStations({utmFix(QStringLiteral("a1"),
                                                kCentralMeridianEasting + 100000.0)});

    // Georeferenced, and still nowhere: the local projection is the project's,
    // and there is no project.
    CHECK(cave->gridConvergence()->state() == cwGridConvergence::NoCoordinateSystem);
    CHECK(cave->gridConvergence()->angle() == 0.0);

    cwCavingRegion region;
    cwCave* joined = cave.release();
    region.addCave(joined);

    CHECK(joined->gridConvergence()->state() == cwGridConvergence::Valid);
}

TEST_CASE("cwGridConvergence: fix stations that say nothing about where they are",
          "[cwGridConvergence]")
{
    cwCavingRegion region;
    cwCave* anchorCave = addCaveFixedAt(&region, QStringLiteral("a1"), kCentralMeridianEasting);
    REQUIRE(anchorCave->gridConvergence()->state() == cwGridConvergence::Valid);

    // A row with no coordinate system reads as zeros. Converging there would put
    // the cave at the frame's origin, which is exactly where a cave that has
    // said nothing must not be reported to be.
    cwCave* vagueCave = addCaveWithFixes(&region, {[] {
        cwFixStation fix;
        fix.setStationName(QStringLiteral("b1"));
        fix.setCoordinate(kCentralMeridianEasting, kNorthing, kElevation);
        return fix;
    }()});

    CHECK(vagueCave->gridConvergence()->state() == cwGridConvergence::NoFixStation);
    CHECK(vagueCave->gridConvergence()->angle() == 0.0);
}
