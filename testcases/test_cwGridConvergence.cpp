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
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"
#include "cwGridConvergence.h"

//Qt includes
#include <QtMath>

using Catch::Matchers::WithinAbs;

namespace {
    const QString Wgs84 = QStringLiteral("EPSG:4326");
    const QString Utm13N = QStringLiteral("EPSG:32613"); // central meridian -105°

    cwGeoPoint utm13NFor(double lon, double lat, double elev)
    {
        cwCoordinateTransform geoToUtm(Wgs84, Utm13N);
        REQUIRE(geoToUtm.isValid());
        return geoToUtm.transform(cwGeoPoint(lon, lat, elev));
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
