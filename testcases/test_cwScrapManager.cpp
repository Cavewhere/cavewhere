//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwScrapManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "TestHelper.h"
#include "asyncfuture.h"
#include "cwTaskManagerModel.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwLinePlotManager.h"
// #include "cwScrapsEntity.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwRunningProfileScrapViewMatrix.h"

//Qt includes
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <QThreadPool>
#include "cwSignalSpy.h"

//Std includes
#include <random>

//Async includes
#include "asyncfuture.h"

TEST_CASE("cwScrapManager should make the file size grow when re-calculaing scraps", "[cwScrapManager]") {
    QFile file;
    qint64 firstSize;

    {
        auto rootData = std::make_unique<cwRootData>();
        auto project = rootData->project();
        fileToProject(project, "://datasets/test_cwScrapManager/scrapGuessNeigborPlan.cw");

        file.setFileName(project->filename());
        file.open(QFile::ReadOnly);
        firstSize = file.size();

        auto scrapManager = rootData->scrapManager();

        int numberOfRuns = 5;
        QList<qint64> runTime;
        QElapsedTimer timer;
        for(int i = 0; i < numberOfRuns; i++) {
            timer.restart();
            scrapManager->updateAllScraps();
            scrapManager->waitForFinish();
            runTime.append(timer.nsecsElapsed());
        }

        double runFraction = 1 / static_cast<double>(runTime.size());
        double averageTime = std::accumulate(runTime.begin(), runTime.end(), 0.0,
                                             [runFraction](double average, int runTime)
        {
            return average + runFraction * runTime;
        });

        std::default_random_engine generator;
        std::uniform_real_distribution<double> distribution(0.0,averageTime * 1.5);

        for(int i = 0; i < 20; i++) {
            double waitTime = distribution(generator);

            QTimer timer;
            timer.setInterval(waitTime * 1e-6);
            timer.start();

            scrapManager->updateAllScraps();

            QEventLoop eventLoop;
            QObject::connect(&timer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
            eventLoop.exec();
        }

        scrapManager->waitForFinish();
        QThreadPool::globalInstance()->waitForDone();
        INFO("Filename:" << project->filename());
    }

    auto nextSize = file.size();
    double ratio = nextSize / static_cast<double>(firstSize);
    CHECK(ratio <= Catch::Approx(1.01));
}

TEST_CASE("cwScrapManager auto update should work propertly", "[cwScrapManager]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    fileToProject(project, "://datasets/test_cwScrapManager/scrapGuessNeigborPlan.cw");

    rootData->futureManagerModel()->waitForFinished();

    cwSignalSpy addRowSpy(rootData->futureManagerModel(), &cwFutureManagerModel::rowsInserted);

    auto scrapManager = rootData->scrapManager();
    CHECK(scrapManager->automaticUpdate() == true);

    scrapManager->setAutomaticUpdate(false);
    CHECK(scrapManager->automaticUpdate() == false);

    //Change the station position
    auto cave = rootData->region()->cave(0);
    auto trip = cave->trip(0);
    auto chunk = trip->chunk(0);
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "10.0");

    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() > 0);
    auto note = notes.first();
    REQUIRE(note->scraps().size() > 0);
    auto scraps = note->scraps();
    auto scrap = scraps.first();

    rootData->futureManagerModel()->waitForFinished();
    CHECK(scrapManager->dirtyScraps().contains(scrap));

    QEventLoop loop;

    QTimer::singleShot(1, qApp, [&](){
        rootData->futureManagerModel()->waitForFinished();

        auto pendingScraps = scrapManager->dirtyScraps();
        CHECK(pendingScraps.contains(scrap));

        scrapManager->setAutomaticUpdate(true);

        rootData->futureManagerModel()->waitForFinished();
        scrapManager->waitForFinish();

        CHECK_FALSE(scrapManager->dirtyScraps().contains(scrap));

        loop.quit();
    });

    loop.exec();
}


TEST_CASE("cwScrapManager shouldn't update scraps that are invalid", "[cwScrapManager]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, "://datasets/test_cwScrapManager/ignoreInvalidScrap.cw");
    REQUIRE(project->cavingRegion()->caveCount() == 1);
    auto cave = project->cavingRegion()->cave(0);
    REQUIRE(project->cavingRegion()->cave(0)->tripCount() == 1);
    auto trip = cave->trip(0);
    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() == 1);
    auto note = notes.at(0);
    auto scrap = new cwScrap();

    rootData->futureManagerModel()->waitForFinished();

    note->addScrap(scrap);
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    rootData->futureManagerModel()->waitForFinished();

    scrap->addPoint(QPointF(0.0, 0.0));
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    scrap->addPoint(QPointF(0.0, 1.0));
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    scrap->addPoint(QPointF(1.0, 1.0));
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    scrap->addPoint(QPointF(1.0, 0.0));
    CHECK(rootData->futureManagerModel()->rowCount() == 0);

    cwNoteStation noteStation;
    noteStation.setPositionOnNote(QPointF(0.5, 0.5));
    noteStation.setName("a1");

    scrap->addStation(noteStation);
    CHECK(rootData->futureManagerModel()->rowCount() == 1);
    rootData->futureManagerModel()->waitForFinished();

    scrap->removeStation(0);
    CHECK(rootData->futureManagerModel()->rowCount() == 1);
    rootData->futureManagerModel()->waitForFinished();
}

TEST_CASE("cwScrapManager should update on viewMatrix change", "[cwScrapManager]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, "://datasets/test_cwScrapManager/ProjectProfile-test-v3.cw");
    if(rootData->futureManagerModel()->rowCount() >= 0) {
        rootData->futureManagerModel()->waitForFinished();
    }

    REQUIRE(project->cavingRegion()->caveCount() == 1);
    auto cave = project->cavingRegion()->cave(0);
    REQUIRE(project->cavingRegion()->cave(0)->tripCount() == 1);
    auto trip = cave->trip(0);
    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() == 1);
    auto note = notes.at(0);
    REQUIRE(note->scraps().size() == 1);
    auto scrap = note->scraps().at(0);
    auto scrapManager = rootData->scrapManager();
    REQUIRE(scrapManager != nullptr);

    CHECK(scrap->type() == cwScrap::ProjectedProfile);
    auto profile = dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
    REQUIRE(profile);
    CHECK(profile->azimuth() == 135.0);

    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    CHECK(scrapManager->dirtyScraps().contains(scrap) == false);
    profile->setAzimuth(136.0);


    CHECK(rootData->futureManagerModel()->rowCount() == 1);
    CHECK(scrapManager->dirtyScraps().contains(scrap));

    rootData->futureManagerModel()->waitForFinished();
    scrapManager->waitForFinish();

    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    CHECK(scrapManager->dirtyScraps().contains(scrap) == false);

    SECTION("Switch to running profile") {
        scrap->setType(cwScrap::RunningProfile);
        CHECK(dynamic_cast<cwRunningProfileScrapViewMatrix*>(scrap->viewMatrix()));

        CHECK(rootData->futureManagerModel()->rowCount() == 1);
        CHECK(scrapManager->dirtyScraps().contains(scrap));

        rootData->futureManagerModel()->waitForFinished();
        scrapManager->waitForFinish();

        CHECK(rootData->futureManagerModel()->rowCount() == 0);
        CHECK(scrapManager->dirtyScraps().contains(scrap) == false);
    }
}

TEST_CASE("cwScrapManager should defer updates while editing", "[cwScrapManager]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, "://datasets/test_cwScrapManager/scrapGuessNeigborPlan.cw");
    rootData->futureManagerModel()->waitForFinished();

    auto scrapManager = rootData->scrapManager();
    REQUIRE(scrapManager);

    auto cave = project->cavingRegion()->cave(0);
    auto trip = cave->trip(0);
    auto note = trip->notes()->notes().first();
    auto scrap = note->scraps().first();
    REQUIRE(scrap);

    rootData->futureManagerModel()->waitForFinished();
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    CHECK(scrapManager->dirtyScraps().contains(scrap) == false);

    cwSignalSpy rowsInsertedSpy(rootData->futureManagerModel(), &cwFutureManagerModel::rowsInserted);

    scrap->beginEditing();
    QPointF currentPos = scrap->stationData(cwScrap::StationPosition, 0).toPointF();
    for(int i = 0; i < 20; ++i) {
        auto offset = 0.01 * (i + 1);
        scrap->setStationData(cwScrap::StationPosition, 0, currentPos + QPointF(offset, 0.0));
    }

    CHECK(rowsInsertedSpy.count() == 0);
    CHECK(scrapManager->dirtyScraps().contains(scrap));

    scrap->endEditing();
    CHECK(rowsInsertedSpy.count() == 1);
    rootData->futureManagerModel()->waitForFinished();
    scrapManager->waitForFinish();


    CHECK(scrapManager->dirtyScraps().contains(scrap) == false);
}
