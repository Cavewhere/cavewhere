/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Cavewhere includes
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
#include "cwTaskManagerModel.h"
#include "cwFutureManagerModel.h"
#include "cwProjectedProfileScrapViewMatrix.h"

//Our includes
#include "TestHelper.h"

//Qt includes
#include <QtGlobal>
#include "cwSignalSpy.h"
#include <algorithm>

class TestRow {
public:
    TestRow(QString filename, double rotation, double scale) :
        Filename(filename),
        Rotation(rotation),
        Scale(scale),
        RotationEpsilon(1.0e-3),
        ScaleEpsilon(1.0e-4)
    {}

    TestRow(QString filename, double rotation, double scale, double rotationEpsilon, double scaleEpsilon) :
        Filename(filename),
        Rotation(rotation),
        Scale(scale),
        RotationEpsilon(rotationEpsilon),
        ScaleEpsilon(scaleEpsilon)
    {}

    TestRow(QString filename,
            double rotation,
            double scale,
            double rotationEpsilon,
            double scaleEpsilon,
            double profileAzimuth) :
        Filename(filename),
        Rotation(rotation),
        Scale(scale),
        RotationEpsilon(rotationEpsilon),
        ScaleEpsilon(scaleEpsilon),
        ProfileAzimuth(profileAzimuth)
    {}

    TestRow() :
        Rotation(0.0),
        Scale(0.0),
        RotationEpsilon(0.0),
        ScaleEpsilon(0.0),
        ProfileAzimuth(0.0)
    {}

    QString Filename;
    double Rotation;
    double Scale;
    double RotationEpsilon;
    double ScaleEpsilon;
    double ProfileAzimuth;

    QString CaveName = QStringLiteral("Cave 1");
    QString TripName = QStringLiteral("Trip 1");
    QString NoteName;
    int ScrapIndex = 0;
};

cwScrap* findScrap(const cwProject* project,
                   const QString& caveName,
                   const QString& tripName,
                   const QString& noteName,
                   int scrapIndex) {
    INFO("Project:" << project->filename());

    REQUIRE(project);
    REQUIRE(project->cavingRegion());
    auto caves = project->cavingRegion()->caves();
    REQUIRE(!caves.isEmpty());

    auto caveIt = caveName.isEmpty()
            ? caves.begin()
            : std::find_if(caves.begin(), caves.end(), [&caveName](const cwCave* cave) {
                  return cave->name().compare(caveName, Qt::CaseInsensitive) == 0;
              });
    REQUIRE(caveIt != caves.end());
    cwCave* cave = *caveIt;

    auto trips = cave->trips();
    REQUIRE(!trips.isEmpty());
    auto tripIt = tripName.isEmpty()
            ? trips.begin()
            : std::find_if(trips.begin(), trips.end(), [&tripName](const cwTrip* trip) {
                  return trip->name().compare(tripName, Qt::CaseInsensitive) == 0;
              });
    REQUIRE(tripIt != trips.end());
    cwTrip* trip = *tripIt;
    cwSurveyNoteModel* noteModel = trip->notes();

    INFO("Trip:" << trip->name());

    const auto notes = noteModel->notes();
    REQUIRE(!notes.isEmpty());

    auto noteIt = noteName.isEmpty()
            ? notes.begin()
            : std::find_if(notes.begin(), notes.end(), [&noteName](const cwNote* note) {
                  return note->name().compare(noteName, Qt::CaseInsensitive) == 0;
              });
    REQUIRE(noteIt != notes.end());
    cwNote* note = *noteIt;

    REQUIRE(note->scraps().size() >= scrapIndex + 1);
    cwScrap* scrap = note->scrap(scrapIndex);

    return scrap;
}

cwScrap* firstScrap(const cwProject* project) {
    return findScrap(project, QString(), QString(), QString(), 0);
}

cwScrap* findScrap(const cwProject* project, const TestRow& row) {
    return findScrap(project, row.CaveName, row.TripName, row.NoteName, row.ScrapIndex);
}

void checkScrapTransform(cwScrap* scrap, const TestRow& row) {
    INFO("Row filename:" << row.Filename.toStdString());
    cwNoteTranformation* transform = scrap->noteTransformation();

    double realScale = 1.0 / transform->scale();

    INFO("Calc Scale:" << realScale << " Row Scale:" << row.Scale );
    CHECK(realScale == Catch::Approx(row.Scale).epsilon(row.ScaleEpsilon));
    INFO("Calc Up:" << transform->northUp() << " Row Up:" << row.Rotation );
    CHECK(transform->northUp() == Catch::Approx(row.Rotation).epsilon(row.RotationEpsilon));

    if(scrap->type() == cwScrap::ProjectedProfile) {
        auto viewMatrix = dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
        REQUIRE(viewMatrix);
        CHECK(viewMatrix->azimuth() == Catch::Approx(row.ProfileAzimuth).margin(0.25));
    }
}

TEST_CASE("Auto Calculate Note Transform", "[cwScrap]") {

    QList<TestRow> rows;
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileRotate.cw", 87.8004635984, 176.349));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileRotateMirror.cw", 87.5044033263, 176.696));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfile.cw", -1.8869214928, 175.592));
    rows.append(TestRow(":/datasets/scrapAutoCalculate/runningProfileMirror.cw", -2.2934958439, 176.721));
    rows.append(TestRow("://datasets/scrapAutoCalculate/runningProfileUpsideDown.cw",  87.8188708214, 1729.652));
    rows.append(TestRow("://datasets/scrapAutoCalculate/ProjectProfile-test-v3.cw",
                0.199, 255.962, 0.05, 0.05, 135.7));
    rows.append(TestRow("://datasets/scrapAutoCalculate/projectedProfile-90left.cw",
                270.08, 255.66, 0.05, 0.05, 134.2));
    rows.append(TestRow("://datasets/scrapAutoCalculate/projectedProfile-90right.cw",
                89.955, 255.667, 0.05, 0.05, 136.3));
    rows.append(TestRow("://datasets/scrapAutoCalculate/projectedProfile-180.cw",
                        180, 255.193, 0.05, 0.05, 135.6));

    foreach(TestRow row, rows) {
        auto project = fileToProject(row.Filename);
        cwScrap* currentScrap = findScrap(project.get(), row);

        auto plotManager = std::make_unique<cwLinePlotManager>();
        plotManager->setRegion(project->cavingRegion());
        plotManager->waitToFinish();

        //Force recalculation
        CHECK(currentScrap->calculateNoteTransform() == false);
        currentScrap->setCalculateNoteTransform(true);

        checkScrapTransform(currentScrap, row);
    }
}

TEST_CASE("Exact Auto Calculate Note Transform", "[cwScrap]") {

    QList<TestRow> rows;
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-0rot-0mirror.cw", -0.142, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-0rot-1mirror.cw", -0.26, 5795.0, 0.06, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-90rot-0mirror.cw", 90, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-90rot-1mirror.cw", 90, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-180rot-0mirror.cw", 180, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-180rot-1mirror.cw", 180, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-270rot-0mirror.cw", 270, 5795.0, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/profile-270rot-1mirror.cw", 270, 5795.0, 0.05, 0.005));

    foreach(TestRow row, rows) {
        auto project = fileToProject(row.Filename);
        cwScrap* currentScrap = findScrap(project.get(), row);

        auto plotManager = std::make_unique<cwLinePlotManager>();
        plotManager->setRegion(project->cavingRegion());
        plotManager->waitToFinish();

        //Force recalculation
        INFO("Filename:" << row.Filename.toStdString());
        CHECK(currentScrap->calculateNoteTransform() == false);
        currentScrap->setCalculateNoteTransform(true);

        while(currentScrap->stations().size() >= 6) {
            checkScrapTransform(currentScrap, row);
            INFO("Removing station:" << currentScrap->station(currentScrap->stations().size() - 1).name().toStdString());
            currentScrap->removeStation(currentScrap->stations().size() - 1);
        }
    }
}

TEST_CASE("Check that auto calculate work outside of trip", "[cwScrap]") {
    QList<TestRow> rows;
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/plan-seperate-trip.cw", 30.13, 1606.3, 0.05, 0.005));
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/plan-seperate-trip-badSave.cw", 30.13, 1606.3, 0.05, 0.005));

    foreach(TestRow row, rows) {
        row.TripName = QStringLiteral("Trip 2");
        auto root = std::make_unique<cwRootData>();
        root->scrapManager()->warpingSettings()->setGridResolutionMeters(10.0);
        root->scrapManager()->warpingSettings()->setUseShotInterpolationSpacing(false);
        root->scrapManager()->warpingSettings()->setUseMaxClosestStations(false);
        root->scrapManager()->warpingSettings()->setUseSmoothingRadius(false);

        fileToProject(root->project(), row.Filename);
        auto project = root->project();
        cwScrap* currentScrap = findScrap(project, row);

        auto plotManager = root->linePlotManager();
        plotManager->waitToFinish();

        CHECK(!project->cavingRegion()->cave(0)->stationPositionLookup().isEmpty());

        root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();

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

TEST_CASE("Auto calculate if survey station change position", "[cwScrap]") {
    QList<TestRow> rows;
    rows.append(TestRow("://datasets/scrapAutoCalculate/exact/plan-seperate-trip.cw", 354.13, 16063.06, 0.05, 0.005));

    foreach(TestRow row, rows) {
        row.TripName = QStringLiteral("Trip 2");
        auto root = std::make_unique<cwRootData>();
        root->scrapManager()->warpingSettings()->setGridResolutionMeters(10.0);
        root->scrapManager()->warpingSettings()->setUseShotInterpolationSpacing(false);
        root->scrapManager()->warpingSettings()->setUseMaxClosestStations(false);
        root->scrapManager()->warpingSettings()->setUseSmoothingRadius(false);

        fileToProject(root->project(), row.Filename);
        auto project = root->project();
        cwScrap* currentScrap = findScrap(project, row);

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

        root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();

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

TEST_CASE("Auto calculate should work on projected profile azimuth", "[cwScrap]") {
    QList<TestRow> rows;
    rows.append(TestRow("://datasets/scrapAutoCalculate/ProjectProfile-test-v3.cw", 0.1997, 255.967, 0.05, 0.005, 135.7));

    foreach(TestRow row, rows) {
        auto root = std::make_unique<cwRootData>();
        fileToProject(root->project(), row.Filename);
        auto project = root->project();
        cwScrap* currentScrap = findScrap(project, row);

        auto plotManager = root->linePlotManager();
        plotManager->waitToFinish();

        root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();

        //Force recalculation
        INFO("Filename:" << row.Filename.toStdString());
        CHECK(currentScrap->calculateNoteTransform() == false);
        currentScrap->setCalculateNoteTransform(true);
        CHECK(currentScrap->calculateNoteTransform() == true);
        checkScrapTransform(currentScrap, row);
    }
}

TEST_CASE("Auto calculate if the scrap type has changed", "[cwScrap]") {
    QList<TestRow> rows;
    rows.append(TestRow("://datasets/scrapAutoCalculate/ProjectProfile-test-startRunning.cw", 359.85, 257.162, 0.05, 0.005, 134.4));
    rows[0].CaveName = "My Cave";
    rows[0].TripName = "Best Trip";

    foreach(TestRow row, rows) {
        auto root = std::make_unique<cwRootData>();
        fileToProject(root->project(), row.Filename);
        auto project = root->project();
        cwScrap* currentScrap = findScrap(project, row);

        auto plotManager = root->linePlotManager();
        plotManager->waitToFinish();

        root->taskManagerModel()->waitForTasks();
        root->futureManagerModel()->waitForFinished();

        currentScrap->setCalculateNoteTransform(true);
        REQUIRE(currentScrap->type() == cwScrap::RunningProfile);

        TestRow runningProfileRow("", currentScrap->noteTransformation()->northUp(), 1.0 / currentScrap->noteTransformation()->scale(), 0.05, 0.005);
        checkScrapTransform(currentScrap, runningProfileRow);

        currentScrap->setType(cwScrap::ProjectedProfile);

        REQUIRE(dynamic_cast<cwProjectedProfileScrapViewMatrix*>(currentScrap->viewMatrix()));

        //Make sure it has change, because we've changed the type
        CHECK(runningProfileRow.Rotation != Catch::Approx(currentScrap->noteTransformation()->northUp()));
        CHECK(1.0 / runningProfileRow.Scale != Catch::Approx(currentScrap->noteTransformation()->scale()));

        //Change it back to running profile
        currentScrap->setType(cwScrap::RunningProfile);
        checkScrapTransform(currentScrap, runningProfileRow); //Shouldbe like the original

        //Change to projected profile
        currentScrap->setType(cwScrap::ProjectedProfile);
        REQUIRE(dynamic_cast<cwProjectedProfileScrapViewMatrix*>(currentScrap->viewMatrix()));
        auto projectedViewMatix2 = static_cast<cwProjectedProfileScrapViewMatrix*>(currentScrap->viewMatrix());
        CHECK(projectedViewMatix2->azimuth() == Catch::Approx(134.4).margin(0.15));
        CHECK(projectedViewMatix2->direction() == cwProjectedProfileScrapViewMatrix::LookingAt);
        checkScrapTransform(currentScrap, row);
    }
}

TEST_CASE("Guess neighbor station name", "[cwScrap]") {
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
    rows.append(TestRow("://datasets/scrapAutoCalculate/ProjectProfile-test-v3.cw"));

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
                INFO("Neighbor:" << neighbor.name().toStdString());
                auto found = std::find_if(scrapStations.begin(), scrapStations.end(), [neighbor](const cwNoteStation& station) {
                    return station.name().compare(neighbor.name(), Qt::CaseInsensitive) == 0;
                });

                REQUIRE(found != scrapStations.end());

                cwNoteStation neighborNoteStation = *found;

                QString guessedName = scrap->guessNeighborStationName(noteStation, neighborNoteStation.positionOnNote());

                CHECK(neighborNoteStation.name().toLower().toStdString() == guessedName.toStdString());
            }
        }
    }
}

TEST_CASE("Distance lead unit should return the index supported units", "[cwScrap]") {
    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
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
