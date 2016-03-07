/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/



//Catch includes
#include "catch.hpp"

//Cavewhere includes
#include "cwTriangulateTask.h"

//Qt includes
#include <QVector3D>
#include <QMatrix4x4>

//Our includes
#include "TestHelper.h"

/**
 * This test the profileViewRotation() function in cwTriangleTask
 */
TEST_CASE("Test profile view rotation") {

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
        QMatrix4x4 rotate = cwTriangulateTask::toProfileRotation(row.Origin, row.ToStation);

        QMatrix4x4 tranlate;
        tranlate.translate(-row.Origin);

        QMatrix4x4 matrix = rotate * tranlate;

        QVector3D calculatedResult = matrix * row.ToStation;

        checkQVector3D(calculatedResult, row.Result, 5);
    }
}
