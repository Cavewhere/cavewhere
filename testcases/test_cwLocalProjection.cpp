/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

//Our includes
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"
#include "cwLocalProjection.h"

//Qt includes
#include <QString>

#include <cmath>
#include <limits>

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {
    // A cave area in eastern Kentucky, inside NAD83 UTM zone 16N.
    constexpr double kAnchorLatitude = 37.1832;
    constexpr double kAnchorLongitude = -84.0947;

    constexpr const char* kNad83Utm16N = "EPSG:26916";
}

TEST_CASE("cwLocalProjection derives a transverse Mercator on the anchor",
          "[cwLocalProjection]")
{
    const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                  cwCoordinateTransform::Wgs84);
    REQUIRE_FALSE(ldp.isEmpty());

    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+proj=tmerc"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+lat_0=37.1832"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+lon_0=-84.0947"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+k=1"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+x_0=0"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+y_0=0"));

    // Without this PROJ reads the string as a transformation pipeline rather
    // than as a CRS, and every transform built from it fails.
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+type=crs"));

    // The result has to be usable as a CS everywhere a CS is taken.
    CHECK(cwCoordinateTransform::isValidCS(ldp));
    CHECK_FALSE(cwCoordinateTransform::isGeographic(ldp));
}

TEST_CASE("cwLocalProjection puts the anchor at the scene origin", "[cwLocalProjection]")
{
    const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                  cwCoordinateTransform::Wgs84);
    REQUIRE_FALSE(ldp.isEmpty());

    cwCoordinateTransform toLocal(cwCoordinateTransform::Wgs84, ldp);
    REQUIRE(toLocal.isValid());

    SECTION("the anchor itself lands on (0, 0)")
    {
        const cwGeoPoint local = toLocal.transform(
            cwGeoPoint(kAnchorLongitude, kAnchorLatitude, 0.0));
        CHECK_THAT(local.x, WithinAbs(0.0, 1e-6));
        CHECK_THAT(local.y, WithinAbs(0.0, 1e-6));
    }

    SECTION("a kilometer north is a kilometer of northing")
    {
        // A degree of latitude, roughly — good to a couple of meters, which is
        // all this needs to tell true distance from a scaled one.
        constexpr double kMetersPerDegreeLatitude = 111132.0;
        constexpr double kOneKilometer = 1000.0;
        const cwGeoPoint local = toLocal.transform(
            cwGeoPoint(kAnchorLongitude,
                       kAnchorLatitude + kOneKilometer / kMetersPerDegreeLatitude,
                       0.0));

        CHECK_THAT(local.x, WithinAbs(0.0, 1e-6));
        // k_0 = 1 on the central meridian, so this is true distance to within
        // the crudeness of the degrees-to-meters figure above.
        CHECK_THAT(local.y, WithinAbs(kOneKilometer, 2.0));
    }

    SECTION("elevation passes through untouched")
    {
        constexpr double kElevation = 312.5;
        const cwGeoPoint local = toLocal.transform(
            cwGeoPoint(kAnchorLongitude, kAnchorLatitude, kElevation));
        CHECK_THAT(local.z, WithinAbs(kElevation, 1e-6));
    }
}

TEST_CASE("cwLocalProjection round-trips back to latitude and longitude",
          "[cwLocalProjection]")
{
    // The single load-bearing property for the future globe morph: any point in
    // the frame is recoverable as a geographic location from the string alone.
    const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                  cwCoordinateTransform::Wgs84);
    REQUIRE_FALSE(ldp.isEmpty());

    cwCoordinateTransform toLocal(cwCoordinateTransform::Wgs84, ldp);
    cwCoordinateTransform toGeographic(ldp, cwCoordinateTransform::Wgs84);
    REQUIRE(toLocal.isValid());
    REQUIRE(toGeographic.isValid());

    // ~30 km east and ~20 km north of the anchor — past where a cave area ends
    // and the LDP's guarantees start to matter.
    const cwGeoPoint source(kAnchorLongitude + 0.35, kAnchorLatitude + 0.18, 285.0);
    const cwGeoPoint roundTripped = toGeographic.transform(toLocal.transform(source));

    CHECK_THAT(roundTripped.x, WithinAbs(source.x, 1e-9));
    CHECK_THAT(roundTripped.y, WithinAbs(source.y, 1e-9));
    CHECK_THAT(roundTripped.z, WithinAbs(source.z, 1e-9));
}

TEST_CASE("cwLocalProjection pins the datum from the anchor's own system",
          "[cwLocalProjection]")
{
    SECTION("a NAD83 anchor gives a NAD83 frame")
    {
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QString::fromLatin1(kNad83Utm16N));
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(ldp.toStdString(), ContainsSubstring("+datum=NAD83"));
    }

    SECTION("a WGS84 anchor gives a WGS84 frame")
    {
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      cwCoordinateTransform::Wgs84);
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK_THAT(ldp.toStdString(), ContainsSubstring("WGS"));
    }

    SECTION("nothing said means WGS84 — what a typed coordinate means")
    {
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QString());
        const QString wgs84Ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                           cwCoordinateTransform::Wgs84);
        REQUIRE_FALSE(ldp.isEmpty());
        CHECK(ldp == wgs84Ldp);
    }

    SECTION("an unreadable system falls back rather than failing")
    {
        const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                                      QStringLiteral("not a coordinate system"));
        CHECK_FALSE(ldp.isEmpty());
    }
}

TEST_CASE("cwLocalProjection takes only the horizontal half of a compound system",
          "[cwLocalProjection]")
{
    // What lidar declares: a horizontal CRS plus a vertical one. The vertical
    // half must not reach the frame — a transform that carries it applies the
    // geoid model and quietly returns ellipsoidal heights on whichever machine
    // has the grids installed.
    const QString compound = QStringLiteral("EPSG:26916+EPSG:5703"); // NAD83 UTM 16N + NAVD88
    const QString ldp = cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude, compound);
    REQUIRE_FALSE(ldp.isEmpty());

    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+proj=tmerc"));
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+datum=NAD83"));
    CHECK_THAT(ldp.toStdString(), !ContainsSubstring("+geoidgrids"));
    CHECK_THAT(ldp.toStdString(), !ContainsSubstring("+vunits"));

    // Same frame as the horizontal half alone would have produced.
    CHECK(ldp == cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                           QString::fromLatin1(kNad83Utm16N)));
}

TEST_CASE("cwLocalProjection refuses a location that isn't one", "[cwLocalProjection]")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    CHECK(cwLocalProjection::derive(91.0, 0.0, cwCoordinateTransform::Wgs84).isEmpty());
    CHECK(cwLocalProjection::derive(-90.5, 0.0, cwCoordinateTransform::Wgs84).isEmpty());
    CHECK(cwLocalProjection::derive(0.0, 181.0, cwCoordinateTransform::Wgs84).isEmpty());
    CHECK(cwLocalProjection::derive(nan, 0.0, cwCoordinateTransform::Wgs84).isEmpty());
    CHECK(cwLocalProjection::derive(0.0, nan, cwCoordinateTransform::Wgs84).isEmpty());

    // The poles and the antimeridian are locations, however unlikely.
    CHECK_FALSE(cwLocalProjection::derive(90.0, 180.0, cwCoordinateTransform::Wgs84).isEmpty());
}

TEST_CASE("cwLocalProjection::deriveFrom centers on a projected anchor",
          "[cwLocalProjection]")
{
    // The anchor as a fix station holds it: numbers in its own system.
    cwCoordinateTransform toUtm(cwCoordinateTransform::Wgs84,
                                QString::fromLatin1(kNad83Utm16N));
    REQUIRE(toUtm.isValid());
    const cwGeoPoint utmAnchor = toUtm.transform(
        cwGeoPoint(kAnchorLongitude, kAnchorLatitude, 250.0));

    const QString ldp = cwLocalProjection::deriveFrom(QString::fromLatin1(kNad83Utm16N),
                                                      utmAnchor);
    REQUIRE_FALSE(ldp.isEmpty());

    cwCoordinateTransform utmToLocal(QString::fromLatin1(kNad83Utm16N), ldp);
    REQUIRE(utmToLocal.isValid());
    const cwGeoPoint local = utmToLocal.transform(utmAnchor);

    CHECK_THAT(local.x, WithinAbs(0.0, 1e-6));
    CHECK_THAT(local.y, WithinAbs(0.0, 1e-6));

    // Derived from the anchor's own datum, so it never travels through WGS84.
    CHECK_THAT(ldp.toStdString(), ContainsSubstring("+datum=NAD83"));
}

TEST_CASE("cwLocalProjection::deriveFrom reads a geographic anchor longitude-first",
          "[cwLocalProjection]")
{
    // Same convention cwCoordinateTransform hands its callers: x is the
    // easting or the longitude, whatever the CRS's own axis order says.
    const QString ldp = cwLocalProjection::deriveFrom(
        cwCoordinateTransform::Wgs84,
        cwGeoPoint(kAnchorLongitude, kAnchorLatitude, 0.0));

    REQUIRE_FALSE(ldp.isEmpty());
    CHECK(ldp == cwLocalProjection::derive(kAnchorLatitude, kAnchorLongitude,
                                           cwCoordinateTransform::Wgs84));
}

TEST_CASE("cwLocalProjection::deriveFrom refuses an anchor with no numbers",
          "[cwLocalProjection]")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(cwLocalProjection::deriveFrom(cwCoordinateTransform::Wgs84,
                                        cwGeoPoint(nan, kAnchorLatitude, 0.0)).isEmpty());
    CHECK(cwLocalProjection::deriveFrom(QStringLiteral("not a coordinate system"),
                                        cwGeoPoint(kAnchorLongitude, kAnchorLatitude, 0.0))
              .isEmpty());
}
