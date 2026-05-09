/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

//Our includes
#include "cwCoordinateTransform.h"
#include "cwGeoPoint.h"

//Qt includes
#include <QVector3D>

#include <cmath>

using Catch::Matchers::WithinAbs;

TEST_CASE("cwGeoPoint converts to QVector3D with worldOrigin offset", "[cwGeoPoint]")
{
    SECTION("toVector3D with no offset narrows to float")
    {
        cwGeoPoint p(1.5, 2.5, 3.5);
        QVector3D v = p.toVector3D();
        CHECK(v.x() == 1.5f);
        CHECK(v.y() == 2.5f);
        CHECK(v.z() == 3.5f);
    }

    SECTION("toVector3D(worldOrigin) preserves precision past float-only narrowing")
    {
        // UTM-scale eastings: subtracting worldOrigin in doubles, then
        // narrowing keeps mm precision; plain float subtraction would lose it.
        cwGeoPoint origin(500123.456789, 4400987.654321, 1234.5);
        cwGeoPoint p(500123.466789, 4400987.664321, 1234.6); // 1cm east, 1cm north, 10cm up

        QVector3D v = p.toVector3D(origin);
        CHECK_THAT(v.x(), WithinAbs(0.01f, 1e-4f));
        CHECK_THAT(v.y(), WithinAbs(0.01f, 1e-4f));
        CHECK_THAT(v.z(), WithinAbs(0.10f, 1e-4f));
    }

    SECTION("equality")
    {
        cwGeoPoint a(1.0, 2.0, 3.0);
        cwGeoPoint b(1.0, 2.0, 3.0);
        cwGeoPoint c(1.0, 2.0, 3.1);
        CHECK(a == b);
        CHECK(a != c);
    }
}

TEST_CASE("cwCoordinateTransform short-circuits identical CS", "[cwCoordinateTransform][identity]")
{
    SECTION("Same string is identity")
    {
        cwCoordinateTransform t("EPSG:4326", "EPSG:4326");
        CHECK(t.isValid());
        CHECK(t.isIdentity());

        cwGeoPoint p(-105.27, 40.01, 1655.0);
        cwGeoPoint q = t.transform(p);
        CHECK(q == p);
    }

    SECTION("Whitespace and case differences are still identity")
    {
        cwCoordinateTransform t(" epsg:32612 ", "EPSG:32612");
        CHECK(t.isValid());
        CHECK(t.isIdentity());
    }

    SECTION("transformInPlace is a no-op when identity")
    {
        cwCoordinateTransform t("EPSG:32612", "EPSG:32612");
        cwGeoPoint pts[3] = {
            cwGeoPoint(500000.0, 4400000.0, 1000.0),
            cwGeoPoint(501000.0, 4401000.0, 1100.0),
            cwGeoPoint(502000.0, 4402000.0, 1200.0)
        };
        t.transformInPlace(pts, 3);
        CHECK(pts[0].x == 500000.0);
        CHECK(pts[1].y == 4401000.0);
        CHECK(pts[2].z == 1200.0);
    }
}

TEST_CASE("cwCoordinateTransform reports invalid CS", "[cwCoordinateTransform][invalid]")
{
    SECTION("Garbage CS string")
    {
        cwCoordinateTransform t("EPSG:4326", "NOT_A_REAL_CRS_xxx");
        CHECK_FALSE(t.isValid());
        CHECK_FALSE(t.isIdentity());
        CHECK_FALSE(t.errorMessage().isEmpty());
    }

    SECTION("Empty CS string")
    {
        cwCoordinateTransform t("", "EPSG:4326");
        CHECK_FALSE(t.isValid());
        CHECK_FALSE(t.errorMessage().isEmpty());
    }
}

TEST_CASE("cwCoordinateTransform::isValidCS validates EPSG codes", "[cwCoordinateTransform][isValidCS]")
{
    CHECK(cwCoordinateTransform::isValidCS("EPSG:4326"));
    CHECK(cwCoordinateTransform::isValidCS("EPSG:32612"));
    CHECK(cwCoordinateTransform::isValidCS("EPSG:27700"));

    CHECK_FALSE(cwCoordinateTransform::isValidCS(""));
    CHECK_FALSE(cwCoordinateTransform::isValidCS("   "));
    CHECK_FALSE(cwCoordinateTransform::isValidCS("NOT_A_CRS"));
    CHECK_FALSE(cwCoordinateTransform::isValidCS("EPSG:99999999"));
}

TEST_CASE("cwCoordinateTransform reprojects WGS84 to UTM 12N", "[cwCoordinateTransform][reproject]")
{
    cwCoordinateTransform t("EPSG:4326", "EPSG:32612");
    REQUIRE(t.isValid());
    REQUIRE_FALSE(t.isIdentity());

    // -110 lon, 40 lat is well inside UTM zone 12N. Expected easting/northing
    // verified against an independent proj invocation:
    //   echo "-110 40" | cs2cs +init=epsg:4326 +to +init=epsg:32612
    // gives roughly (500000, 4427757) for -111 +40, and ~585360 / 4428236 for
    // -110 +40. We test round-trip precision rather than hard-coding numbers.
    cwGeoPoint lonLat(-110.0, 40.0, 1500.0);
    cwGeoPoint utm = t.transform(lonLat);

    // Easting must be inside the canonical UTM range (zone-N falsifies to 500k).
    CHECK(utm.x > 100000.0);
    CHECK(utm.x < 900000.0);
    // Northing for 40N should be roughly 4.4M.
    CHECK(utm.y > 4'000'000.0);
    CHECK(utm.y < 5'000'000.0);
    // Elevation passes through unchanged for these horizontal-only EPSG codes.
    CHECK_THAT(utm.z, WithinAbs(1500.0, 1e-3));
}

TEST_CASE("cwCoordinateTransform round-trip preserves mm precision", "[cwCoordinateTransform][precision]")
{
    cwCoordinateTransform forward("EPSG:4326", "EPSG:32612");
    cwCoordinateTransform inverse("EPSG:32612", "EPSG:4326");
    REQUIRE(forward.isValid());
    REQUIRE(inverse.isValid());

    const cwGeoPoint origin(-110.123456, 40.987654, 1500.0);
    const cwGeoPoint utm = forward.transform(origin);
    const cwGeoPoint back = inverse.transform(utm);

    // Round-trip residual: converting 1e-7 degrees to meters at 40N ≈ 1cm.
    // Allow up to 1mm in degrees (~10cm at 40N) to be safe across PROJ versions.
    CHECK_THAT(back.x, WithinAbs(origin.x, 1e-8));
    CHECK_THAT(back.y, WithinAbs(origin.y, 1e-8));
    CHECK_THAT(back.z, WithinAbs(origin.z, 1e-3));
}

TEST_CASE("cwCoordinateTransform::transformInPlace reprojects an array", "[cwCoordinateTransform][batch]")
{
    cwCoordinateTransform t("EPSG:4326", "EPSG:32612");
    REQUIRE(t.isValid());

    cwGeoPoint pts[3] = {
        cwGeoPoint(-110.0, 40.0, 1500.0),
        cwGeoPoint(-110.5, 40.5, 1600.0),
        cwGeoPoint(-111.0, 41.0, 1700.0)
    };

    cwCoordinateTransform single("EPSG:4326", "EPSG:32612");
    const cwGeoPoint expected0 = single.transform(pts[0]);
    const cwGeoPoint expected1 = single.transform(pts[1]);
    const cwGeoPoint expected2 = single.transform(pts[2]);

    t.transformInPlace(pts, 3);

    CHECK_THAT(pts[0].x, WithinAbs(expected0.x, 1e-6));
    CHECK_THAT(pts[0].y, WithinAbs(expected0.y, 1e-6));
    CHECK_THAT(pts[1].x, WithinAbs(expected1.x, 1e-6));
    CHECK_THAT(pts[1].y, WithinAbs(expected1.y, 1e-6));
    CHECK_THAT(pts[2].x, WithinAbs(expected2.x, 1e-6));
    CHECK_THAT(pts[2].y, WithinAbs(expected2.y, 1e-6));
}

TEST_CASE("cwCoordinateTransform::commonProjectedCSList lists valid EPSG codes",
          "[cwCoordinateTransform]")
{
    const QStringList list = cwCoordinateTransform::commonProjectedCSList();
    REQUIRE_FALSE(list.isEmpty());
    for (const QString& cs : list) {
        INFO("CS: " << cs.toStdString());
        CHECK(cwCoordinateTransform::isValidCS(cs));
    }
}
