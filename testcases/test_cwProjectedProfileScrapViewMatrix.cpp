//Catch includes
#include "catch.hpp"

//Our includes
#include "cwProjectedProfileScrapViewMatrix.h"
#include "TestHelper.h"

TEST_CASE("cwProjectedProfileScrapViewMatrix should create the matrix correctly", "[cwProjectedProfileScrapViewMatrix]")
{
    auto fuzzyCompareVector = [](QVector3D v1, QVector3D v2) {
        INFO(v1 << "==" << v2);
        double delta = 0.000001;
        CHECK(v1.x() == Approx(v2.x()).margin(delta));
        CHECK(v1.y() == Approx(v2.y()).margin(delta));
        CHECK(v1.z() == Approx(v2.z()).margin(delta));
    };

    cwProjectedProfileScrapViewMatrix view;

    REQUIRE(dynamic_cast<const cwProjectedProfileScrapViewMatrix::Data*>(view.data()) != nullptr);

    //Make sure cloning works correctly
    auto matrixData = std::unique_ptr<cwProjectedProfileScrapViewMatrix::Data>(view.data()->clone());
    CHECK(matrixData->matrix() == view.matrix());
    CHECK(matrixData->azimuth() == view.azimuth());

    INFO("North");
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 0.0, 1.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(1.0, 0.0, 0.0));

    INFO("East");
    view.setAzimuth(90); //East
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(1.0, 0.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, -1.0));

    INFO("South");
    view.setAzimuth(180); //South
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 0.0, -1.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(-1.0, 0.0, 0.0));

    INFO("West");
    view.setAzimuth(270); //West
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(-1.0, 0.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, 1.0));
}
