//Catch includes
#include "catch.hpp"

//Our includes
#include "cwRunningProfileScrapViewMatrix.h"
#include "TestHelper.h"

TEST_CASE("Running profile scrap view matrix should produce the correct ViewMatrix", "[cwRunningProfileScrapViewMatrix]") {

    cwRunningProfileScrapViewMatrix matrix;
    CHECK(matrix.type() == cwScrap::RunningProfile);
    CHECK(matrix.data()->type() == cwScrap::RunningProfile);
    CHECK(matrix.matrix() == QMatrix4x4());

    QVector3D origin;

    INFO("Invalid")
    cwRunningProfileScrapViewMatrix::Data invalid(QVector3D(1.0, 2.0, 3.0), QVector3D(1.0, 2.0, 3.0));
    fuzzyCompareVector(invalid.from(), QVector3D(1.0, 2.0, 3.0));
    fuzzyCompareVector(invalid.to(), QVector3D(1.0, 2.0, 3.0));
    CHECK(invalid.matrix() == QMatrix4x4());

    //Make sure that matrix works correctly
    INFO("North");
    cwRunningProfileScrapViewMatrix::Data north(origin, QVector3D(0.0, 1.0, 0.0));
    fuzzyCompareVector(north.from(), QVector3D(0.0, 0.0, 0.0));
    fuzzyCompareVector(north.to(), QVector3D(0.0, 1.0, 0.0));
    fuzzyCompareVector(north.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(north.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 0.0, 1.0));
    fuzzyCompareVector(north.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(1.0, 0.0, 0.0));

    INFO("East");
    cwRunningProfileScrapViewMatrix::Data east(origin, QVector3D(1.0, 0.0, 0.0));
    fuzzyCompareVector(east.from(), QVector3D(0.0, 0.0, 0.0));
    fuzzyCompareVector(east.to(), QVector3D(1.0, 0.0, 0.0));
    fuzzyCompareVector(east.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(east.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(1.0, 0.0, 0.0));
    fuzzyCompareVector(east.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, -1.0));

    INFO("South");
    cwRunningProfileScrapViewMatrix::Data south(origin, QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(south.from(), QVector3D(0.0, 0.0, 0.0));
    fuzzyCompareVector(south.to(), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(south.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(south.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 0.0, -1.0));
    fuzzyCompareVector(south.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(-1.0, 0.0, 0.0));

    INFO("West");
    cwRunningProfileScrapViewMatrix::Data west(origin, QVector3D(-1.0, 0.0, 0.0));
    fuzzyCompareVector(west.from(), QVector3D(0.0, 0.0, 0.0));
    fuzzyCompareVector(west.to(), QVector3D(-1.0, 0.0, 0.0));
    fuzzyCompareVector(west.matrix() * QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0));
    fuzzyCompareVector(west.matrix() * QVector3D(1.0, 0.0, 0.0), QVector3D(-1.0, 0.0, 0.0));
    fuzzyCompareVector(west.matrix() * QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, 1.0));

}

/**
 * This test the profileViewRotation() function in cwScrap
 */
TEST_CASE("Test profile view rotation", "[cwRunningProfileScrapViewMatrix]") {

    class TestRow {
    public:
        TestRow() {}
        TestRow(QVector3D origin, QVector3D toStation, QVector3D result) :
            Origin(origin),
            ToStation(toStation),
            Result(result)
        {}

        QVector3D Origin;
        QVector3D ToStation;
        QVector3D Result;
    };

    QVector3D origin(0.0, 0.0, 0.0);
    QVector3D origin1(5.2, 4.2, 1.2);

    QList<TestRow> rows;
    rows.append(TestRow(origin, QVector3D(0.0, 0.0, 0.0), QVector3D(0.0, 0.0, 0.0)));
    rows.append(TestRow(origin, QVector3D(1.0, 0.0, 0.0), QVector3D(1.0, 0.0, 0.0)));
    rows.append(TestRow(origin, QVector3D(-1.0, 0.0, 0.0), QVector3D(1.0, 0.0, 0.0)));
    rows.append(TestRow(origin, QVector3D(0.0, 1.0, 0.0), QVector3D(1.0, 0.0, 0.0)));
    rows.append(TestRow(origin, QVector3D(0.0, -1.0, 0.0), QVector3D(1.0, 0.0, 0.0)));
    rows.append(TestRow(origin, QVector3D(0.0, 0.0, 1.0), QVector3D(0.0, 1.0, 0.0)));
    rows.append(TestRow(origin, QVector3D(0.0, 0.0, -1.0), QVector3D(0.0, -1.0, 0.0)));
    rows.append(TestRow(origin, QVector3D(1.0, 1.0, -1.0), QVector3D(1.4142135623730951, -1.0, 0.0)));
    rows.append(TestRow(origin1, origin1 + QVector3D(1.0, 1.0, -1.0), QVector3D(1.4142135623730951, -1.0, 0.0)));

    for(int i = 0; i < rows.size(); i++) {
        const TestRow& row = rows.at(i);
        QMatrix4x4 rotate = cwRunningProfileScrapViewMatrix::Data(row.Origin, row.ToStation).matrix();

        QMatrix4x4 tranlate;
        tranlate.translate(-row.Origin);

        QMatrix4x4 matrix = rotate * tranlate;

        QVector3D calculatedResult = matrix * row.ToStation;

        checkQVector3D(calculatedResult, row.Result, 5);
    }
}

