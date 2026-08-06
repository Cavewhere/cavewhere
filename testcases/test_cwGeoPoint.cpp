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

TEST_CASE("cwGeoPoint::toVector3D narrows to float", "[cwGeoPoint]")
{
    SECTION("A small coordinate narrows exactly")
    {
        const cwGeoPoint p(1.5, 2.5, 3.5);

        const QVector3D v = p.toVector3D();
        CHECK(v.x() == 1.5f);
        CHECK(v.y() == 2.5f);
        CHECK(v.z() == 3.5f);
    }

    SECTION("local-projection magnitudes keep mm precision through the narrowing")
    {
        // The project's frame is centered on its anchor, so scene coordinates
        // stay small — which is what leaves float room for millimeters. The
        // same values as UTM eastings would not survive the narrowing.
        const cwGeoPoint p(123.456789, 987.654321, 1234.5);

        const QVector3D v = p.toVector3D();
        CHECK_THAT(v.x(), WithinAbs(123.456789f, 1e-3f));
        CHECK_THAT(v.y(), WithinAbs(987.654321f, 1e-3f));
        CHECK_THAT(v.z(), WithinAbs(1234.5f, 1e-3f));
    }
}

TEST_CASE("cwGeoPoint compares componentwise", "[cwGeoPoint]")
{
    const cwGeoPoint a(1.0, 2.0, 3.0);
    const cwGeoPoint b(1.0, 2.0, 3.0);
    const cwGeoPoint c(1.0, 2.0, 3.1);

    CHECK(a == b);
    CHECK(a != c);
}

TEST_CASE("cwGeoPoint::fromSceneLocal widens a scene point unchanged", "[cwGeoPoint]")
{
    // The project's local projection is centered on its anchor, so a scene
    // coordinate is already a coordinate in that projection — there is no
    // offset in either direction.
    const QVector3D sceneLocal(7.0f, -3.0f, 2.0f);
    const cwGeoPoint global = cwGeoPoint::fromSceneLocal(sceneLocal);

    CHECK_THAT(global.x, WithinAbs(7.0, 1e-6));
    CHECK_THAT(global.y, WithinAbs(-3.0, 1e-6));
    CHECK_THAT(global.z, WithinAbs(2.0, 1e-6));
}

TEST_CASE("cwGeoPoint::fromSceneLocal is the inverse of toVector3D", "[cwGeoPoint]")
{
    const cwGeoPoint global(123.5, 87.25, 1655.0);

    const QVector3D sceneLocal = global.toVector3D();
    const cwGeoPoint recovered = cwGeoPoint::fromSceneLocal(sceneLocal);

    CHECK_THAT(recovered.x, WithinAbs(global.x, 1e-2));
    CHECK_THAT(recovered.y, WithinAbs(global.y, 1e-2));
    CHECK_THAT(recovered.z, WithinAbs(global.z, 1e-2));
}
