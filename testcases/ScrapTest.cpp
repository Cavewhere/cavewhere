/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include "catch.hpp"

//Cavewhere includes
#include "cwScrap.h"
#include "cwProject.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwScrap.h"
#include "cwNoteTranformation.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwCavingRegion.h"

//Our includes
#include "TestHelper.h"

//Qt includes
#include <QtGlobal>

cwScrap* firstScrap(const cwProject* project) {
    REQUIRE(project->cavingRegion()->caveCount() == 1);
    cwCave* cave = project->cavingRegion()->cave(0);

    REQUIRE(cave->tripCount() == 1);
    cwTrip* trip = cave->trip(0);
    cwSurveyNoteModel* noteModel = trip->notes();

    REQUIRE(noteModel->rowCount() == 1);
    cwNote* note = noteModel->notes().first();

    REQUIRE(note->scraps().size() == 1);
    cwScrap* scrap = note->scrap(0);

    return scrap;
}

TEST_CASE("Auto Calculate Note Transform", "[ScrapTest]") {

    class TestRow {
    public:
        TestRow(QString filename, double rotation, double scale) :
            Filename(filename),
            Rotation(rotation),
            Scale(scale) {}
        TestRow() {}

        QString Filename;
        double Rotation;
        double Scale;
    };

    QList<TestRow> rows;
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileRotate.cw", 89.7529184495, 176.349));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileRotateMirror.cw", 89.5297778044, 176.696));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfile.cw", -0.1062471752, 175.592));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileMirror.cw", 0.0034680627, 176.721));

    foreach(TestRow row, rows) {
        cwProject* project = fileToProject(row.Filename);
        cwScrap* scrap = firstScrap(project);

        //Force recalculation
        CHECK(scrap->calculateNoteTransform() == false);
        scrap->setCalculateNoteTransform(true);

        cwNoteTranformation* transform = scrap->noteTransformation();

        double realScale = 1.0 / transform->scale();

        INFO("Calc Scale:" << realScale << " Row Scale:" << row.Scale );
        CHECK(realScale == Approx(row.Scale));
        INFO("Calc Up:" << transform->northUp() << " Row Up:" << row.Rotation );
        CHECK(transform->northUp() == Approx(row.Rotation));

        delete project;
    }
}

TEST_CASE("Guess neighbor station name", "[ScrapTest]") {
    class TestRow {
    public:
        TestRow(QString filename) :
            Filename(filename)
        {}

        QString Filename;
    };

    QList<TestRow> rows;
    rows.append(TestRow("://datasets/scrapGuessNeighbor/scrapGuessNeigborPlan.cw"));
    rows.append(TestRow("://datasets/scrapGuessNeighbor/scrapGuessNeigborProfile.cw"));
    rows.append(TestRow("://datasets/scrapGuessNeighbor/scrapGuessNeigborProfileRotate90.cw"));

    foreach(TestRow row, rows) {
        INFO("Testing:" << row.Filename.toStdString());

        cwProject* planProject = fileToProject(row.Filename);
        cwScrap* scrap = firstScrap(planProject);

        REQUIRE(scrap->stations().size() >= 1);
        cwNoteStation centerStation = scrap->stations().first();

        for(int i = 1; i < scrap->stations().size(); i++) {
            cwNoteStation station = scrap->station(i);

//            qDebug() << "Matching station:" << station.name();
            QString guessedName = scrap->guessNeighborStationName(centerStation, station.positionOnNote());

            CHECK(station.name().toStdString() == guessedName.toStdString());
        }

        delete planProject;
    }
}

/**
 * This test the profileViewRotation() function in cwScrap
 */
TEST_CASE("Test profile view rotation", "[ScrapTest]") {

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
        QMatrix4x4 rotate = cwScrap::toProfileRotation(row.Origin, row.ToStation);

        QMatrix4x4 tranlate;
        tranlate.translate(-row.Origin);

        QMatrix4x4 matrix = rotate * tranlate;

        QVector3D calculatedResult = matrix * row.ToStation;

        checkQVector3D(calculatedResult, row.Result, 5);
    }
}
