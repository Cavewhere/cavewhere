//Catch includes
#include <catch2/catch.hpp>

//Our includes
#include "cwTransformUpdater.h"
#include "cwCamera.h"
#include "cwPositioner3D.h"

//Qt includes
#include <QSignalSpy>

TEST_CASE("Transform Updater should hide QQuickItem if behind the camera", "[cwTransformUpdater]") {

    cwTransformUpdater updater;
    cwCamera camera;

    cwProjection projection;
    projection.setPerspective(55, 1.0, 1.0, 10000);

    camera.setProjection(projection);
    camera.setViewport(QRect(0, 0, 1000, 1000));
    camera.setViewMatrix(QMatrix4x4()); //Default view looking down the -z axis

    cwPositioner3D positionItem;
    positionItem.setPosition3D(QVector3D(0, 0, 10)); //Behind the camera

    QSignalSpy visibleSpy(&positionItem, &cwPositioner3D::visibleChanged);

    updater.setCamera(&camera);
    updater.addPointItem(&positionItem);

    CHECK(visibleSpy.size() == 1);
    CHECK(positionItem.isVisible() == false);

    positionItem.setPosition3D(QVector3D(0, 0, -10)); //In front of camera

    CHECK(visibleSpy.size() == 2);
    CHECK(positionItem.isVisible() == true);


}
