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
#include "cwLinePlotManager.h"
#include "cwRootData.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"

//Our includes
#include "TestHelper.h"

//Qt includes
#include <QtGlobal>

class TestRow {
public:
    TestRow(QString filename, double rotation, double scale) :
        Filename(filename),
        Rotation(rotation),
        Scale(scale),
        RotationEpsilon(1.0e-4),
        ScaleEpsilon(1.0e-4)
    {}

    TestRow(QString filename, double rotation, double scale, double rotationEpsilon, double scaleEpsilon) :
        Filename(filename),
        Rotation(rotation),
        Scale(scale),
        RotationEpsilon(rotationEpsilon),
        ScaleEpsilon(scaleEpsilon)
    {}

    TestRow() :
        Rotation(0.0),
        Scale(0.0),
        RotationEpsilon(0.0),
        ScaleEpsilon(0.0)
    {}

    QString Filename;
    double Rotation;
    double Scale;
    double RotationEpsilon;
    double ScaleEpsilon;
};

cwScrap* scrap(const cwProject* project, int caveIndex, int tripIndex, int noteIndex, int scrapIndex) {
    REQUIRE(project->cavingRegion()->caveCount() >= caveIndex + 1);
    cwCave* cave = project->cavingRegion()->cave(caveIndex);

    REQUIRE(cave->tripCount() >= tripIndex + 1);
    cwTrip* trip = cave->trip(tripIndex);
    cwSurveyNoteModel* noteModel = trip->notes();

    REQUIRE(noteModel->rowCount() >= noteIndex + 1);
    cwNote* note = noteModel->notes().at(noteIndex);

    REQUIRE(note->scraps().size() >= scrapIndex + 1);
    cwScrap* scrap = note->scrap(scrapIndex);

    return scrap;
}

cwScrap* firstScrap(const cwProject* project) {
    return scrap(project, 0, 0, 0, 0);
}

void checkScrapTransform(cwScrap* scrap, const TestRow& row) {
    INFO("Row filename:" << row.Filename.toStdString());
    cwNoteTranformation* transform = scrap->noteTransformation();

    double realScale = 1.0 / transform->scale();

    INFO("Calc Scale:" << realScale << " Row Scale:" << row.Scale );
    CHECK(realScale == Approx(row.Scale).epsilon(row.ScaleEpsilon));
    INFO("Calc Up:" << transform->northUp() << " Row Up:" << row.Rotation );
    CHECK(transform->northUp() == Approx(row.Rotation).epsilon(row.RotationEpsilon));
}

TEST_CASE("Auto Calculate Note Transform", "[Scrap]") {

    QList<TestRow> rows;
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileRotate.cw", 87.8004635984, 176.349));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileRotateMirror.cw", 87.5044033263, 176.696));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfile.cw", -1.8869214928, 175.592));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileMirror.cw", -2.2934958439, 176.721));

    foreach(TestRow row, rows) {
        auto project = fileToProject(row.Filename);
        cwScrap* scrap = firstScrap(project.get());

        auto plotManager = std::make_unique<cwLinePlotManager>();
        plotManager->setRegion(project->cavingRegion());
        plotManager->waitToFinish();

        //Force recalculation
        CHECK(scrap->calculateNoteTransform() == false);
        scrap->setCalculateNoteTransform(true);

        checkScrapTransform(scrap, row);
    }
}

TEST_CASE("Exact Auto Calculate Note Transform", "[Scrap]") {

    QList<TestRow> rows;
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-0rot-0mirror.cw", -0.155, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-0rot-1mirror.cw", -0.26, 5795.0, 0.06, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-90rot-0mirror.cw", 90, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-90rot-1mirror.cw", 90, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-180rot-0mirror.cw", 180, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-180rot-1mirror.cw", 180, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-270rot-0mirror.cw", 270, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-270rot-1mirror.cw", 270, 5795.0, 0.05, 0.005));

    foreach(TestRow row, rows) {
        auto project = fileToProject(row.Filename);
        cwScrap* scrap = firstScrap(project.get());

        auto plotManager = std::make_unique<cwLinePlotManager>();
        plotManager->setRegion(project->cavingRegion());
        plotManager->waitToFinish();

        //Force recalculation
        INFO("Filename:" << row.Filename.toStdString());
        CHECK(scrap->calculateNoteTransform() == false);
        scrap->setCalculateNoteTransform(true);

        while(scrap->stations().size() >= 6) {
            checkScrapTransform(scrap, row);
            INFO("Removing station:" << scrap->station(scrap->stations().size() - 1).name().toStdString());
            scrap->removeStation(scrap->stations().size() - 1);
        }
    }
}

TEST_CASE("Check that auto calculate work outside of trip", "[Scrap]") {
    QList<TestRow> rows;
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/plan-seperate-trip.cw", 30.13, 1606.3, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/plan-seperate-trip-badSave.cw", 30.13, 1606.3, 0.05, 0.005));

    foreach(TestRow row, rows) {
        auto root = std::make_unique<cwRootData>();
        fileToProject(root->project(), row.Filename);
        auto project = root->project();
        cwScrap* currentScrap = scrap(project, 0, 1, 0, 0);

        auto plotManager = root->linePlotManager();
        plotManager->waitToFinish();

        INFO("Finished after plotManager!");
        checkScrapTransform(currentScrap, row);

        currentScrap->setCalculateNoteTransform(false);

        //Force recalculation
        INFO("Filename:" << row.Filename.toStdString());
        CHECK(currentScrap->calculateNoteTransform() == false);
        currentScrap->setCalculateNoteTransform(true);
        CHECK(currentScrap->calculateNoteTransform() == true);
        checkScrapTransform(currentScrap, row);
    }
}

TEST_CASE("Auto calculate if survey station change position", "[Scrap]") {
    QList<TestRow> rows;
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/plan-seperate-trip.cw", 354.13, 16063.06, 0.05, 0.005));

    foreach(TestRow row, rows) {
        auto root = std::make_unique<cwRootData>();
        fileToProject(root->project(), row.Filename);
        auto project = root->project();
        cwScrap* currentScrap = scrap(project, 0, 1, 0, 0);

        REQUIRE(project->cavingRegion()->caves().size() > 0);
        REQUIRE(project->cavingRegion()->caves().first()->trips().size() > 0);
        REQUIRE(project->cavingRegion()->caves().first()->trips().first()->chunks().size() > 0);

        auto chunk = project->cavingRegion()->cave(0)->trip(0)->chunk(0);
        REQUIRE(chunk->shotCount() > 0);
        CHECK(chunk->data(cwSurveyChunk::ShotDistanceRole, 0).toDouble() == 100.0);
        chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, 1000);
        chunk->setData(cwSurveyChunk::ShotCompassRole, 0, 45);

        auto plotManager = root->linePlotManager();
        plotManager->waitToFinish();

        INFO("Finished after plotManager!");
        checkScrapTransform(currentScrap, row);

        currentScrap->setCalculateNoteTransform(false);

        //Force recalculation
        INFO("Filename:" << row.Filename.toStdString());
        CHECK(currentScrap->calculateNoteTransform() == false);
        currentScrap->setCalculateNoteTransform(true);
        CHECK(currentScrap->calculateNoteTransform() == true);
        checkScrapTransform(currentScrap, row);
    }
}


TEST_CASE("Guess neighbor station name", "[Scrap]") {
    class TestRow {
    public:
        TestRow(QString filename) :
            Filename(filename)
        {}

        QString Filename;
    };

    QList<TestRow> rows;
    rows.append(TestRow("://datasets/scrapGuessNeighbor/scrapGuessNeigborPlan.cw"));
    rows.append(TestRow("://datasets/scrapGuessNeighbor/scrapGuessNeigborPlanContinuous.cw"));
    rows.append(TestRow("://datasets/scrapGuessNeighbor/scrapGuessNeigborProfile.cw"));
    rows.append(TestRow("://datasets/scrapGuessNeighbor/scrapGuessNeigborProfileRotate90.cw"));
    rows.append(TestRow("://datasets/scrapGuessNeighbor/scrapGuessNeigborProfileContinuous.cw"));

    foreach(TestRow row, rows) {
        INFO("Testing:" << row.Filename.toStdString());

        auto planProject = fileToProject(row.Filename);
        cwScrap* scrap = firstScrap(planProject.get());

        auto plotManager = std::make_unique<cwLinePlotManager>();
        plotManager->setRegion(planProject->cavingRegion());
        plotManager->waitToFinish();

        REQUIRE(scrap->stations().size() >= 1);
        cwNoteStation centerStation = scrap->stations().first();

        QList<cwNoteStation> scrapStations = scrap->stations();

        foreach(cwNoteStation noteStation, scrapStations) {
            QSet<cwStation> neighbors = scrap->parentNote()->parentTrip()->neighboringStations(noteStation.name());

            INFO("Center station:" << noteStation.name().toStdString());

            foreach(cwStation neighbor, neighbors) {
                auto found = std::find_if(scrapStations.begin(), scrapStations.end(), [neighbor](const cwNoteStation& station) {
                    return station.name() == neighbor.name();
                });

                CHECK(found != scrapStations.end());

                cwNoteStation neighborNoteStation = *found;

                QString guessedName = scrap->guessNeighborStationName(noteStation, neighborNoteStation.positionOnNote());

                CHECK(neighborNoteStation.name().toUpper().toStdString() == guessedName.toStdString());
            }
        }
    }
}

/**
 * This test the profileViewRotation() function in cwScrap
 */
TEST_CASE("Test profile view rotation", "[Scrap]") {

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

TEST_CASE("Distance lead unit should return the index supported units", "[cwScrap]") {
    auto project = std::unique_ptr<cwProject>(new cwProject);
    fileToProject(project.get(), "://datasets/test_cwScrap/leadLengthCheck.cw");

    auto trip = project->cavingRegion()->cave(0)->trip(0);
    auto scrap = trip->notes()->notes().at(0)->scrap(0);
    REQUIRE(scrap->leads().size() == 1);

    auto checkUnit = [=](QString unit) {

        bool okay;
        int supportedUnitIndex = scrap->leadData(cwScrap::LeadUnits, 0).toInt(&okay);
        CHECK(okay);
        REQUIRE(supportedUnitIndex >= 0);
        auto supportedUnits = scrap->leadData(cwScrap::LeadSupportedUnits, 0).toStringList();
        REQUIRE(supportedUnitIndex < supportedUnits.size());
        INFO("Supported Units:" << supportedUnits << "unit:" << unit);
        CHECK(supportedUnits.at(supportedUnitIndex).toStdString() == unit.toStdString());
    };

    trip->calibrations()->setDistanceUnit(cwUnits::Meters);
    checkUnit("m");

    trip->calibrations()->setDistanceUnit(cwUnits::Feet);
    checkUnit("ft");
}

