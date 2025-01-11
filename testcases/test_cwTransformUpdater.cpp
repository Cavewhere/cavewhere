//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTransformUpdater.h"
#include "cwCamera.h"
// #include "cwBasePositioner.h"

//Qt includes
#include "cwSignalSpy.h"

TEST_CASE("Transform Updater should hide QQuickItem if behind the camera", "[cwTransformUpdater]") {

    cwTransformUpdater updater;
    cwCamera camera;

    cwProjection projection;
    projection.setPerspective(55, 1.0, 1.0, 10000);

    camera.setProjection(projection);
    camera.setViewport(QRect(0, 0, 1000, 1000));
    camera.setViewMatrix(QMatrix4x4()); //Default view looking down the -z axis

    // cwBasePositioner positionItem;
    // positionItem.setPosition3D(QVector3D(0, 0, 10)); //Behind the camera

    // cwSignalSpy visibleSpy(&positionItem, &cwBasePositioner::visibleChanged);

    // updater.setCamera(&camera);
    // updater.addPointItem(&positionItem);

    // CHECK(visibleSpy.size() == 1);
    // CHECK(positionItem.isVisible() == false);

    // // positionItem.setPosition3D(QVector3D(0, 0, -10)); //In front of camera

    // CHECK(visibleSpy.size() == 2);
    // CHECK(positionItem.isVisible() == true);


}
