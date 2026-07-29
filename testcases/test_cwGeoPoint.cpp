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
#include "cwGeoPoint.h"

//Qt includes
#include <QVector3D>

using Catch::Matchers::WithinAbs;

TEST_CASE("cwGeoPoint converts to QVector3D with worldOrigin offset", "[cwGeoPoint]")
{
    SECTION("A zero worldOrigin just narrows to float")
    {
        cwGeoPoint p(1.5, 2.5, 3.5);
        QVector3D v = p.toVector3D(cwGeoPoint());
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

TEST_CASE("cwGeoPoint::fromSceneLocal adds the worldOrigin offset", "[cwGeoPoint]")
{
    const cwGeoPoint origin(400000.0, 4500000.0, 1600.0);
    const QVector3D sceneLocal(-30.0f, 12.0f, -5.0f);

    const cwGeoPoint global = cwGeoPoint::fromSceneLocal(sceneLocal, origin);

    CHECK_THAT(global.x, WithinAbs(origin.x + double(sceneLocal.x()), 1e-6));
    CHECK_THAT(global.y, WithinAbs(origin.y + double(sceneLocal.y()), 1e-6));
    CHECK_THAT(global.z, WithinAbs(origin.z + double(sceneLocal.z()), 1e-6));
}

TEST_CASE("cwGeoPoint::fromSceneLocal is the inverse of toVector3D(worldOrigin)",
          "[cwGeoPoint]")
{
    const cwGeoPoint origin(400000.0, 4500000.0, 1600.0);
    const cwGeoPoint global(400123.5, 4500087.25, 1655.0);

    // global -> scene-local -> global round-trips within float precision.
    const QVector3D sceneLocal = global.toVector3D(origin);
    const cwGeoPoint recovered = cwGeoPoint::fromSceneLocal(sceneLocal, origin);

    CHECK_THAT(recovered.x, WithinAbs(global.x, 1e-2));
    CHECK_THAT(recovered.y, WithinAbs(global.y, 1e-2));
    CHECK_THAT(recovered.z, WithinAbs(global.z, 1e-2));
}

TEST_CASE("cwGeoPoint::fromSceneLocal with a zero origin equals the raw vector",
          "[cwGeoPoint]")
{
    const QVector3D sceneLocal(7.0f, -3.0f, 2.0f);
    const cwGeoPoint global = cwGeoPoint::fromSceneLocal(sceneLocal, cwGeoPoint());

    CHECK_THAT(global.x, WithinAbs(7.0, 1e-6));
    CHECK_THAT(global.y, WithinAbs(-3.0, 1e-6));
    CHECK_THAT(global.z, WithinAbs(2.0, 1e-6));
}
