//Catch includes
#include "catch.hpp"

//Our includes
#include "cwProjectedProfileScrapViewMatrix.h"
#include "TestHelper.h"

TEST_CASE("cwProjectedProfileScrapViewMatrix should create the matrix correctly", "[cwProjectedProfileScrapViewMatrix]")
{
    cwProjectedProfileScrapViewMatrix view;
    CHECK(view.type() == cwScrap::ProjectedProfile);
    CHECK(view.data()->type() == cwScrap::ProjectedProfile);

    REQUIRE(dynamic_cast<const cwProjectedProfileScrapViewMatrix::Data*>(view.data()) != nullptr);

    //Make sure cloning works correctly
    auto matrixData = std::unique_ptr<cwProjectedProfileScrapViewMatrix::Data>(view.data()->clone());
    CHECK(matrixData->matrix() == view.matrix());
    CHECK(matrixData->azimuth() == view.azimuth());

    INFO("North"); //Looking North
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(1.0, 0.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, -1.0));

    INFO("East");
    view.setAzimuth(90); //Looking East
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 0.0, -1.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(-1.0, 0.0, 0.0));

    INFO("South");
    view.setAzimuth(180); //Looking South
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(-1.0, 0.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, 1.0));

    INFO("West");
    view.setAzimuth(270); //Looking West
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 0.0, 1.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(1.0, 0.0, 0.0));

}
