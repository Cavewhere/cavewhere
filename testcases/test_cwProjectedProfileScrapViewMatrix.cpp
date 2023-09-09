//Catch includes
#include <catch2/catch.hpp>

//Our includes
#include "cwProjectedProfileScrapViewMatrix.h"
#include "TestHelper.h"
#include "SpyChecker.h"

//Qt includes
#include <QSignalSpy>

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

    QSignalSpy azimuthChangedSpy(&view, &cwProjectedProfileScrapViewMatrix::azimuthChanged);
    QSignalSpy matrixChangedSpy(&view, &cwProjectedProfileScrapViewMatrix::matrixChanged);
    QSignalSpy directionChangedSpy(&view, &cwProjectedProfileScrapViewMatrix::directionChanged);

    azimuthChangedSpy.setObjectName("azimuthChangedSpy");
    matrixChangedSpy.setObjectName("matrixChangedSpy");
    directionChangedSpy.setObjectName("directionChangedSpy");


    SpyChecker spyChecker({
                              {&azimuthChangedSpy, 0},
                              {&matrixChangedSpy, 0},
                              {&directionChangedSpy, 0},
                          });

    INFO("North"); //Looking North
    CHECK(view.data()->absoluteAzimuth() == 0.0);
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(1.0, 0.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, -1.0));
    spyChecker.checkSpies();

    INFO("East");
    view.setAzimuth(90); //Looking East
    CHECK(view.data()->absoluteAzimuth() == 90.0);
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 0.0, -1.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(-1.0, 0.0, 0.0));
    spyChecker[&matrixChangedSpy]++;
    spyChecker[&azimuthChangedSpy]++;
    spyChecker.checkSpies();

    INFO("South");
    view.setAzimuth(180); //Looking South
    CHECK(view.data()->absoluteAzimuth() == 180.0);
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(-1.0, 0.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, 1.0));
    spyChecker[&matrixChangedSpy]++;
    spyChecker[&azimuthChangedSpy]++;
    spyChecker.checkSpies();

    INFO("West");
    view.setAzimuth(270); //Looking West
    CHECK(view.data()->absoluteAzimuth() == 270.0);
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 0.0, 1.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(1.0, 0.0, 0.0));
    spyChecker[&matrixChangedSpy]++;
    spyChecker[&azimuthChangedSpy]++;
    spyChecker.checkSpies();

    SECTION("Make sure direction works correctly") {
        view.setAzimuth(0);
        spyChecker[&matrixChangedSpy]++;
        spyChecker[&azimuthChangedSpy]++;
        spyChecker.checkSpies();

        CHECK(view.direction() == cwProjectedProfileScrapViewMatrix::LookingAt);

        view.setDirection(cwProjectedProfileScrapViewMatrix::RightToLeft);
        CHECK(view.azimuth() == 0.0);
        CHECK(view.data()->absoluteAzimuth() == 90.0);
        fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
        fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 0.0, -1.0));
        fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(-1.0, 0.0, 0.0));
        spyChecker[&matrixChangedSpy]++;
        spyChecker[&directionChangedSpy]++;
        spyChecker.checkSpies();

        view.setDirection(cwProjectedProfileScrapViewMatrix::LeftToRight);
        CHECK(view.azimuth() == 0.0);
        CHECK(view.data()->absoluteAzimuth() == -90.0);
        fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
        fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 0.0, 1.0));
        fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(1.0, 0.0, 0.0));
        spyChecker[&matrixChangedSpy]++;
        spyChecker[&directionChangedSpy]++;
        spyChecker.checkSpies();
    }
}
