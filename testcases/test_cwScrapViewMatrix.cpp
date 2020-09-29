//Catch includes
#include "catch.hpp"

//Our includes
#include "cwScrapViewMatrix.h"
#include "TestHelper.h"

TEST_CASE("cwScrapViewMatrix create it's matrix correctly", "[cwScrapViewMatrix]") {

    cwScrapViewMatrix view;
    CHECK(view.type() == cwScrapViewMatrix::Plan);

    QMatrix4x4 defaultMatrix;
    defaultMatrix.setToIdentity();
    CHECK(view.matrix() == defaultMatrix);

    view.setType(cwScrapViewMatrix::RunningProfile);
    CHECK(view.matrix() == defaultMatrix);

    auto fuzzyCompareVector = [](QVector3D v1, QVector3D v2) {
        INFO(v1 << "==" << v2);
        double delta = 0.000001;
        CHECK(v1.x() == Approx(v2.x()).margin(delta));
        CHECK(v1.y() == Approx(v2.y()).margin(delta));
        CHECK(v1.z() == Approx(v2.z()).margin(delta));
    };

    view.setType(cwScrapViewMatrix::ProjectedProfile);
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, 1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(1.0, 0.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, 1.0));

    view.setAzimuth(90); //East
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(1.0, 0.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, 1.0));

    view.setAzimuth(180); //South
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(-1.0, 0.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, 1.0));

    view.setAzimuth(270); //West
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(-1.0, 0.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 1.0, 0.0));
    fuzzyCompareVector(view.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, 1.0));
}

TEST_CASE("cwScrapViewMatrix operator == and != works", "[cwScrapViewMatrix]") {
    cwScrapViewMatrix view;
    CHECK(view == view);

    auto view1 = view;
    CHECK(view == view1);

    SECTION("Change azimuth") {
       view1.setAzimuth(100);
       CHECK(view != view1);

       view1.setAzimuth(0);
       CHECK(view == view1);
    }

    SECTION("Change type") {
        view1.setType(cwScrapViewMatrix::ProjectedProfile);
        CHECK(view != view1);

        view1.setType(cwScrapViewMatrix::Plan);
        CHECK(view == view1);
    }
}
