//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCamera.h"

//Qt includes
#include <QVector3D>

TEST_CASE("cwCamera normalized device coordinate clipping", "[cwCamera]") {
    CHECK(cwCamera::isNormalizedDeviceCoordinateClipped(QVector3D(-1.1f, 0.0f, 0.0f)));
    CHECK(cwCamera::isNormalizedDeviceCoordinateClipped(QVector3D(1.1f, 0.0f, 0.0f)));
    CHECK(cwCamera::isNormalizedDeviceCoordinateClipped(QVector3D(0.0f, -1.1f, 0.0f)));
    CHECK(cwCamera::isNormalizedDeviceCoordinateClipped(QVector3D(0.0f, 1.1f, 0.0f)));
    CHECK(cwCamera::isNormalizedDeviceCoordinateClipped(QVector3D(0.0f, 0.0f, -1.1f)));
    CHECK(cwCamera::isNormalizedDeviceCoordinateClipped(QVector3D(0.0f, 0.0f, 1.1f)));

    CHECK_FALSE(cwCamera::isNormalizedDeviceCoordinateClipped(QVector3D(0.0f, 0.0f, 0.0f)));
    CHECK_FALSE(cwCamera::isNormalizedDeviceCoordinateClipped(QVector3D(-1.0f, -1.0f, -1.0f)));
    CHECK_FALSE(cwCamera::isNormalizedDeviceCoordinateClipped(QVector3D(1.0f, 1.0f, 1.0f)));
}

TEST_CASE("cwCamera qtViewportMatrix maps normalized device coordinates", "[cwCamera]") {
    cwCamera camera;
    camera.setViewport(QRect(0, 0, 100, 200));

    QMatrix4x4 matrix = camera.qtViewportMatrix();

    QVector3D topLeft = matrix.map(QVector3D(-1.0f, 1.0f, 0.0f));
    CHECK(topLeft == QVector3D(0.0f, 0.0f, 0.0f));

    QVector3D bottomRight = matrix.map(QVector3D(1.0f, -1.0f, 0.0f));
    CHECK(bottomRight == QVector3D(100.0f, 200.0f, 0.0f));

    QVector3D center = matrix.map(QVector3D(0.0f, 0.0f, 0.0f));
    CHECK(center == QVector3D(50.0f, 100.0f, 0.0f));
}

TEST_CASE("cwCamera qt viewport clipping", "[cwCamera]") {
    cwCamera camera;
    camera.setViewport(QRect(0, 0, 100, 200));

    CHECK_FALSE(camera.isQtViewportCoordinateClipped(QVector3D(50.0f, 100.0f, 0.0f)));
    CHECK(camera.isQtViewportCoordinateClipped(QVector3D(-1.0f, 100.0f, 0.0f)));
    CHECK(camera.isQtViewportCoordinateClipped(QVector3D(101.0f, 100.0f, 0.0f)));
    CHECK(camera.isQtViewportCoordinateClipped(QVector3D(50.0f, -1.0f, 0.0f)));
    CHECK(camera.isQtViewportCoordinateClipped(QVector3D(50.0f, 201.0f, 0.0f)));
    CHECK(camera.isQtViewportCoordinateClipped(QVector3D(50.0f, 100.0f, -1.1f)));
    CHECK(camera.isQtViewportCoordinateClipped(QVector3D(50.0f, 100.0f, 1.1f)));
}
