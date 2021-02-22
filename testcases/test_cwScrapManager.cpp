//Catch includes
#include <catch2/catch.hpp>

//Our includes
#include "cwScrapManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "TestHelper.h"
#include "cwAsyncFuture.h"
#include "cwTaskManagerModel.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwLinePlotManager.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwRunningProfileScrapViewMatrix.h"

//Qt includes
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <QThreadPool>
#include <QSignalSpy>

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
    CHECK(ratio <= Approx(1.01));
}

TEST_CASE("cwScrapManager auto update should work propertly", "[cwScrapManager]") {
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    fileToProject(project, "://datasets/test_cwScrapManager/scrapGuessNeigborPlan.cw");

    rootData->futureManagerModel()->waitForFinished();

    QSignalSpy addRowSpy(rootData->futureManagerModel(), &cwFutureManagerModel::rowsInserted);

    auto scrapManager = rootData->scrapManager();
    CHECK(scrapManager->automaticUpdate() == true);

    scrapManager->setAutomaticUpdate(false);
    CHECK(scrapManager->automaticUpdate() == false);

    //Change the station position
    auto cave = rootData->region()->cave(0);
    auto trip = cave->trip(0);
    auto chunk = trip->chunk(0);
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "10.0");

    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    CHECK(addRowSpy.count() == 0);

    QEventLoop loop;

    QTimer::singleShot(1, [&](){
        rootData->linePlotManager()->waitToFinish();
        CHECK(rootData->futureManagerModel()->rowCount() == 0);
        CHECK(addRowSpy.count() == 0);

        scrapManager->setAutomaticUpdate(true);
        CHECK(trip->notes()->notes().first()->scraps().first()->triangulationData().isStale());

        rootData->futureManagerModel()->waitForFinished();

        CHECK(!trip->notes()->notes().first()->scraps().first()->triangulationData().isStale());

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
    CHECK(rootData->futureManagerModel()->rowCount() == 0);
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

    REQUIRE(project->cavingRegion()->caveCount() == 1);
    auto cave = project->cavingRegion()->cave(0);
    REQUIRE(project->cavingRegion()->cave(0)->tripCount() == 1);
    auto trip = cave->trip(0);
    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() == 1);
    auto note = notes.at(0);
    REQUIRE(note->scraps().size() == 1);
    auto scrap = note->scraps().at(0);

    CHECK(scrap->type() == cwScrap::ProjectedProfile);
    auto profile = dynamic_cast<cwProjectedProfileScrapViewMatrix*>(scrap->viewMatrix());
    REQUIRE(profile);
    CHECK(profile->azimuth() == 135.0);

    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    CHECK(scrap->triangulationData().isStale() == false);
    profile->setAzimuth(136.0);


    CHECK(rootData->futureManagerModel()->rowCount() == 1);
    CHECK(scrap->triangulationData().isStale() == true);

    rootData->futureManagerModel()->waitForFinished();

    CHECK(rootData->futureManagerModel()->rowCount() == 0);
    CHECK(scrap->triangulationData().isStale() == false);

    SECTION("Switch to running profile") {
        scrap->setType(cwScrap::RunningProfile);
        CHECK(dynamic_cast<cwRunningProfileScrapViewMatrix*>(scrap->viewMatrix()));

        CHECK(rootData->futureManagerModel()->rowCount() == 1);
        CHECK(scrap->triangulationData().isStale() == true);

        rootData->futureManagerModel()->waitForFinished();

        CHECK(rootData->futureManagerModel()->rowCount() == 0);
        CHECK(scrap->triangulationData().isStale() == false);
    }
}
