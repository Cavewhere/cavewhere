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
