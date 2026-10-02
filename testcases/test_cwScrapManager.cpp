//Catch includes
#include <catch2/catch_test_macros.hpp>
#include "LoadProjectHelper.h"
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwScrapManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "TestHelper.h"
#include "asyncfuture.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwNote.h"
#include "cwSurveyNoteModel.h"
#include "cwLinePlotManager.h"
#include "cwUpdateCoordinator.h"
#include "cwJobSettings.h"
// #include "cwScrapsEntity.h"
#include "cwProjectedProfileScrapViewMatrix.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordItem.h"
#include "cwRenderTexturedItemVisibility.h"
#include "cwRenderTexturedItems.h"
#include "cwRegionSceneManager.h"
#include "cwScene.h"
#include "cwSceneVisibility.h"
#include "cwRunningProfileScrapViewMatrix.h"
#include "cwImageUtils.h"
#include "cwCavingRegion.h"
#include "cwScrap.h"
#include "cwNoteStation.h"
#include "cwNoteTranformation.h"
#include "cwStationPositionLookup.h"
#include "cwExternalCenterlineManager.h"
#include "ExternalCenterlineTestHelpers.h"
#include "cwLead.h"
#include "cwFutureManagerModel.h"
#include "SurveyTreeTestHelper.h"

//Qt includes
#include <QFile>
#include <QImageReader>
#include <QElapsedTimer>
#include <QThread>
#include <QCoreApplication>
#include <QEvent>
#include <QThreadPool>
#include <QEventLoop>
#include "cwSignalSpy.h"

//Std includes
#include <algorithm>
#include <random>

//Async includes
#include "asyncfuture.h"

namespace {
    //What the "Compute Scraps" menu item does. The manager has no single call for
    //it on purpose: marking is what makes it every scrap rather than the dirty
    //ones, and the drive that follows is the ordinary reading of the state.
    void computeAllScraps(cwScrapManager* scrapManager)
    {
        scrapManager->markAllScrapsDirty();
        scrapManager->runIfNeeded();
    }

    //Render ids are handed out from 1 and every dataset here has a handful of
    //items, so scanning this far covers them all.
    constexpr uint32_t kMaxScannedRenderId = 64;

    //How many render items carry a streamed-texture descriptor. cwScrapManager
    //sets one when it delivers a scrap, so this counts the scraps the renderer
    //has been given.
    int deliveredScrapCount(const cwRenderTexturedItems* renderItems)
    {
        int count = 0;
        for(uint32_t id = 1; id <= kMaxScannedRenderId; id++) {
            if(renderItems->hasItem(id) && !renderItems->item(id).streamedTexture.isNull()) {
                count++;
            }
        }
        return count;
    }
}

static void requireAutomaticUpdatesEnabled()
{
    cwJobSettings::initialize();
    auto* settings = cwJobSettings::instance();
    REQUIRE(settings->automaticUpdate());
}

TEST_CASE("cwScrapManager should make the file size grow when re-calculaing scraps", "[cwScrapManager]") {
    requireAutomaticUpdatesEnabled();
    QFile file;
    qint64 firstSize;

    {
        auto rootData = std::make_unique<cwRootData>();
        auto project = rootData->project();
        fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));

        file.setFileName(project->filename());
        REQUIRE(file.open(QFile::ReadOnly));
        firstSize = file.size();

        auto scrapManager = rootData->scrapManager();

        int numberOfRuns = 5;
        QList<qint64> runTime;
        QElapsedTimer timer;
        for(int i = 0; i < numberOfRuns; i++) {
            timer.restart();
            computeAllScraps(scrapManager);
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

            computeAllScraps(scrapManager);

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
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));

    rootData->futureManagerModel()->waitForFinished();

    cwSignalSpy addRowSpy(rootData->futureManagerModel(), &cwFutureManagerModel::rowsInserted);

    auto scrapManager = rootData->scrapManager();
    auto coordinator = rootData->updateCoordinator();
    CHECK(coordinator->automaticUpdate() == true);

    coordinator->setAutomaticUpdate(false);
    CHECK(coordinator->automaticUpdate() == false);

    auto cave = rootData->region()->cave(0);
    auto trip = cave->trip(0);
    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() > 0);
    auto note = notes.first();
    REQUIRE(note->scraps().size() > 0);
    auto scraps = note->scraps();
    auto scrap = scraps.first();

    //Dirty the scraps while auto update is off. The single coordinator gates
    //the whole pipeline, so a warping change (which marks scraps dirty on its
    //own) is used rather than a station move, whose line-plot solve would also
    //be deferred.
    auto warping = scrapManager->warpingSettings();
    warping->setGridResolutionMeters(warping->gridResolutionMeters() + 1.0);

    rootData->futureManagerModel()->waitForFinished();
    CHECK(scrapManager->dirtyScraps().contains(scrap));

    QEventLoop loop;

    QTimer::singleShot(1, qApp, [&](){
        rootData->futureManagerModel()->waitForFinished();

        auto pendingScraps = scrapManager->dirtyScraps();
        CHECK(pendingScraps.contains(scrap));

        coordinator->setAutomaticUpdate(true);

        rootData->futureManagerModel()->waitForFinished();
        scrapManager->waitForFinish();

        CHECK_FALSE(scrapManager->dirtyScraps().contains(scrap));

        loop.quit();
    });

    loop.exec();
}

TEST_CASE("A single Run resolves the line-plot to scraps cascade with automatic update off", "[cwScrapManager]") {
    // Regression for the F1 "press Run twice" trap: with automatic update off, a
    // survey edit defers the line-plot solve. One Run (updateNow) must solve the
    // line plot AND carry the station-position change through to the scraps it
    // dirties on completion, leaving nothing stale — the user never has to press
    // Run a second time.
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
    rootData->futureManagerModel()->waitForFinished();

    auto scrapManager = rootData->scrapManager();
    auto linePlotManager = rootData->linePlotManager();
    auto coordinator = rootData->updateCoordinator();
    REQUIRE(scrapManager);
    REQUIRE(linePlotManager);

    coordinator->setAutomaticUpdate(false);
    REQUIRE(coordinator->automaticUpdate() == false);

    auto cave = rootData->region()->cave(0);
    auto trip = cave->trip(0);
    REQUIRE(trip->chunkCount() > 0);
    auto chunk = trip->chunk(0);
    auto note = trip->notes()->notes().first();
    REQUIRE(note->scraps().size() > 0);
    auto scrap = note->scraps().first();

    // Everything is clean and settled to start.
    rootData->futureManagerModel()->waitForFinished();
    REQUIRE(coordinator->needsUpdate() == false);
    REQUIRE(scrapManager->dirtyScraps().contains(scrap) == false);

    // Edit a survey shot: this dirties the line plot only. With automatic update
    // off nothing solves, so the scrap stays clean (it is dirtied later, by the
    // line-plot solve moving station positions).
    cwSignalSpy scrapsCascadeSpy(linePlotManager,
                                 &cwLinePlotManager::stationPositionInScrapsChanged);
    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "25.0");
    rootData->futureManagerModel()->waitForFinished();

    CHECK(coordinator->needsUpdate() == true);          // line plot is dirty
    CHECK(scrapManager->dirtyScraps().contains(scrap) == false); // scrap not yet
    CHECK(scrapsCascadeSpy.count() == 0);               // nothing solved

    // One Run.
    coordinator->updateNow();

    // Drain the whole cascade: the line-plot solve completes, dirties the
    // scraps, and the still-open forced update runs them too.
    for(int i = 0; i < 20 && (coordinator->needsUpdate()
                              || rootData->futureManagerModel()->rowCount() > 0
                              || linePlotManager->updateState() == cwUpdatable::State::Working
                              || scrapManager->updateState() == cwUpdatable::State::Working); ++i) {
        rootData->futureManagerModel()->waitForFinished();
        linePlotManager->waitToFinish();
        scrapManager->waitForFinish();
        QCoreApplication::processEvents();
    }

    // The cascade actually fired for THIS scrap — assert the scrap itself
    // appears in a stationPositionInScrapsChanged payload, not merely that some
    // solve emitted (that signal fires per successful solve regardless of which
    // scraps moved). Guards against dataset drift silently making the test pass.
    const auto scrapCascaded = [&]() {
        for(const auto& emission : scrapsCascadeSpy) {
            if(emission.at(0).value<QList<cwScrap*>>().contains(scrap)) {
                return true;
            }
        }
        return false;
    };
    CHECK(scrapCascaded());
    // ...and one Run resolved it: nothing stale, no second press needed.
    CHECK(coordinator->needsUpdate() == false);
    CHECK(scrapManager->dirtyScraps().contains(scrap) == false);
}

TEST_CASE("Scrap pipeline reports Working while a triangulation task is in flight",
          "[cwScrapManager]")
{
    // updateState() must distinguish "dirty and idle" (Dirty) from "dirty and
    // computing" (Working): the restarter queues its start, so the task is in
    // flight — but not yet finished — the moment the forced update() returns.
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
    rootData->futureManagerModel()->waitForFinished();

    auto scrapManager = rootData->scrapManager();
    REQUIRE(scrapManager);

    // Settle to Clean.
    scrapManager->waitForFinish();
    rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    REQUIRE(scrapManager->updateState() == cwUpdatable::State::Clean);

    // Recompute every scrap. Marking makes the pipeline Dirty, so the drive
    // dispatches a task; the restarter's start is queued, so the pipeline is
    // Working (task dispatched, not yet finished) synchronously here.
    computeAllScraps(scrapManager);
    CHECK(scrapManager->updateState() == cwUpdatable::State::Working);

    // Drain to completion — back to Clean, never stuck Working.
    for(int i = 0; i < 20 && scrapManager->updateState() != cwUpdatable::State::Clean; ++i) {
        rootData->futureManagerModel()->waitForFinished();
        scrapManager->waitForFinish();
        QCoreApplication::processEvents();
    }
    CHECK(scrapManager->updateState() == cwUpdatable::State::Clean);
}


TEST_CASE("Each scrap reaches the render items as it finishes", "[cwScrapManager]") {
    // Scraps are handed to the renderer one at a time, as each triangulation
    // finishes, so the 3d view fills in scrap by scrap. Delivering the whole
    // batch at the end left the view empty for the length of the run and then
    // synchronized everything in one frame.
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto scrapManager = rootData->scrapManager();
    REQUIRE(scrapManager != nullptr);
    auto renderItems = rootData->regionSceneManager()->items();
    REQUIRE(renderItems != nullptr);

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
    rootData->futureManagerModel()->waitForFinished();
    scrapManager->waitForFinish();
    QCoreApplication::processEvents();

    auto note = project->cavingRegion()->cave(0)->trip(0)->notes()->notes().first();
    REQUIRE(note->scraps().size() == 1);
    auto firstScrap = note->scraps().first();

    // The dataset carries a single scrap, and one scrap can't show the
    // difference between per-scrap and end-of-batch delivery. Copy it, nudged
    // off the original, so the batch below has several scraps to triangulate.
    constexpr int kExtraScrapCount = 2;
    constexpr double kScrapOffset = 0.02;
    QList<cwScrap*> extraScraps;
    for(int i = 1; i <= kExtraScrapCount; i++) {
        auto* scrap = new cwScrap();
        const QPointF offset(kScrapOffset * i, 0.0);

        const QList<QPointF> points = firstScrap->points();
        for(int pointIndex = 0; pointIndex < points.size(); pointIndex++) {
            scrap->insertPoint(pointIndex, points.at(pointIndex) + offset);
        }

        const QList<cwNoteStation> stations = firstScrap->stations();
        for(const cwNoteStation& station : stations) {
            cwNoteStation copy = station;
            copy.setPositionOnNote(station.positionOnNote() + offset);
            scrap->addStation(copy);
        }

        // Delivering a scrap writes its lead positions, so a lead makes each
        // delivery observable at the instant it happens.
        cwLead lead;
        lead.setDescription(QStringLiteral("Lead %1").arg(i));
        lead.setPositionOnNote(stations.first().positionOnNote() + offset);
        scrap->addLead(lead);

        note->addScrap(scrap);
        extraScraps.append(scrap);
    }

    const int totalScraps = scrapManager->renderScrapCount();
    REQUIRE(totalScraps == kExtraScrapCount + 1);

    // Count the deliveries, and the pipeline state each one landed in. A scrap
    // delivered while the pipeline is still Working got there ahead of the
    // batch's completion handler, which is what per-scrap delivery buys; the
    // end-of-batch push ran after that handler had left Working.
    QHash<cwScrap*, int> deliveryCount;
    int deliveriesWhileWorking = 0;
    for(cwScrap* scrap : std::as_const(extraScraps)) {
        QObject::connect(scrap, &cwScrap::leadsDataChanged, scrapManager,
                         [&, scrap](int, int, QList<int>)
        {
            deliveryCount[scrap]++;
            if(scrapManager->updateState() == cwUpdatable::State::Working) {
                deliveriesWhileWorking++;
            }
        });
    }

    // A delivered scrap stays hidden until the intersecter publishes a BVH that
    // contains it, so per-scrap delivery only fills the 3d view in if the pick
    // gate opens mid-run too (issue #671). attachScrap mints a render id that
    // reads visible before the scrap's geometry arrives, so a scrap counts as
    // drawable only once it has been delivered.
    auto* scene = rootData->regionSceneManager()->scene();
    REQUIRE(scene != nullptr);
    const auto renderObjectId = renderItems->renderObjectId();

    constexpr int kRunTimeoutMs = 120000;
    constexpr int kPollWaitMs = 2;
    bool sawVisibleWhileWorking = false;
    QElapsedTimer runTimer;
    runTimer.start();
    while(scrapManager->updateState() == cwUpdatable::State::Working
          && runTimer.elapsed() < kRunTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents,
                                        kPollWaitMs);

        const auto snapshot = scene->visibility()->snapshot();
        bool anyDeliveredVisible = false;
        bool anyPending = false;
        for(cwScrap* scrap : std::as_const(extraScraps)) {
            if(deliveryCount.value(scrap) > 0) {
                if(snapshot.subVisible(renderObjectId, scrapManager->renderId(scrap))) {
                    anyDeliveredVisible = true;
                }
            } else {
                anyPending = true;
            }
        }

        if(anyDeliveredVisible && anyPending) {
            sawVisibleWhileWorking = true;
        }
    }

    scrapManager->waitForFinish();
    rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    // Every scrap ended up delivered, exactly one texture descriptor each.
    CHECK(deliveredScrapCount(renderItems) == totalScraps);

    // A scrap was drawable while the rest of the batch was still running.
    CHECK(sawVisibleWhileWorking);

    // Each scrap was delivered exactly once, mid-run.
    for(cwScrap* scrap : std::as_const(extraScraps)) {
        CHECK(deliveryCount.value(scrap) == 1);
    }
    CHECK(deliveriesWhileWorking == extraScraps.size());
}

TEST_CASE("cwScrapManager shouldn't update scraps that are invalid", "[cwScrapManager]") {
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/ignoreInvalidScrap.cw"));
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

TEST_CASE("Deleting the last dirty scrap announces the pipeline is clean",
          "[cwScrapManager]") {
    // Removing a scrap drops it from the dirty set, which can take the pipeline
    // from Dirty straight to Clean. That transition has to be announced like any
    // other, or the coordinator's aggregate goes stale: the footer keeps offering
    // Run for work that no longer exists.
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"));
    rootData->futureManagerModel()->waitForFinished();

    auto scrapManager = rootData->scrapManager();
    auto coordinator = rootData->updateCoordinator();
    REQUIRE(scrapManager != nullptr);

    auto notes = project->cavingRegion()->cave(0)->trip(0)->notes()->notes();
    REQUIRE(notes.size() == 1);
    auto note = notes.at(0);
    REQUIRE(note->scraps().size() == 1);

    coordinator->setAutomaticUpdate(false);

    // Settle to Clean so the dirty set is exactly what this test puts in it.
    scrapManager->waitForFinish();
    rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();
    REQUIRE(scrapManager->updateState() == cwUpdatable::State::Clean);

    // Dirty the one scrap without running it: with automatic update off, moving a
    // station marks the scrap and leaves the run to the coordinator. This is the
    // state the footer's Run offer is built on.
    auto scrap = note->scraps().at(0);
    const QPointF stationPos = scrap->stationData(cwScrap::StationPosition, 0).toPointF();
    scrap->setStationData(cwScrap::StationPosition, 0, stationPos + QPointF(0.01, 0.0));

    REQUIRE(scrapManager->dirtyScraps().contains(scrap));
    REQUIRE(scrapManager->updateState() == cwUpdatable::State::Dirty);
    REQUIRE(coordinator->needsUpdate());

    cwSignalSpy pipelineSpy(scrapManager, &cwScrapManager::updateStateChanged);
    cwSignalSpy aggregateSpy(coordinator, &cwUpdateCoordinator::needsUpdateChanged);

    // Delete the only dirty scrap. Nothing is left to compute. removeScraps uses
    // deleteLater, and plain processEvents() doesn't flush deferred deletes, so
    // ask for them explicitly — the manager reacts to the scrap's destroyed().
    note->removeScraps(0, 0);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    CHECK(scrapManager->updateState() == cwUpdatable::State::Clean);
    CHECK(pipelineSpy.count() > 0);
    CHECK_FALSE(coordinator->needsUpdate());
    CHECK(aggregateSpy.count() == 1);
}

TEST_CASE("cwScrapManager should update on viewMatrix change", "[cwScrapManager]") {
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"));
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
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
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

TEST_CASE("cwScrapManager scrap render items should remain visible after loading a bundle (.cw) file", "[cwScrapManager][regression]") {
    // Regression test for bug #329: "Save As makes carpeting go away"
    //
    // Root cause: cwProject::filenameChanged fired after bundle load (which
    // completes after scraps are already inserted and registered with the
    // keyword item model).  cwRootData's filenameChanged handler was calling
    // m_keywordItemModel->clear(), which triggered cwKeywordVisibility to call
    // setVisible(false) on every cwRenderTexturedItemVisibility.  The guard in
    // addKeywordItemForScrap() then prevented re-registration, leaving all
    // carpeted scraps permanently invisible in the renderer.
    requireAutomaticUpdatesEnabled();

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
    rootData->futureManagerModel()->waitForFinished();

    auto* keywordModel = rootData->keywordItemModel();
    REQUIRE(keywordModel != nullptr);

    // Every keyword item whose object is a cwRenderTexturedItemVisibility must
    // report visible == true.  Before the fix the filenameChanged handler
    // cleared the model mid-load, driving all visibility objects to false.
    int visibilityItemCount = 0;
    for (int i = 0; i < keywordModel->rowCount(); ++i) {
        auto* kwItem = keywordModel->item(i);
        REQUIRE(kwItem != nullptr);
        auto* vis = qobject_cast<cwRenderTexturedItemVisibility*>(kwItem->object());
        if (vis) {
            ++visibilityItemCount;
            INFO("cwRenderTexturedItemVisibility at keyword model row " << i << " is not visible");
            CHECK(vis->isVisible());
        }
    }

    // Sanity-check: the dataset has scraps, so we must have found at least one
    // visibility item.
    CHECK(visibilityItemCount > 0);
}

TEST_CASE("Scrap keyword items are cleaned up across a project reload", "[cwScrapManager][regression]") {
    // cwRootData's filenameChanged handler deliberately does NOT clear the
    // keyword item model on reload, relying on scrap keyword items being torn
    // down when their scraps are destroyed.  This guards that contract: reloading
    // a project into the same cwRootData must not accumulate stale scrap keyword
    // items (each reload destroys the previous region's scraps).
    requireAutomaticUpdatesEnabled();

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();

    auto* keywordModel = rootData->keywordItemModel();
    REQUIRE(keywordModel != nullptr);

    const QString dataset =
        testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw");

    auto scrapVisibilityCount = [&]() {
        int count = 0;
        for (int i = 0; i < keywordModel->rowCount(); ++i) {
            auto* kwItem = keywordModel->item(i);
            REQUIRE(kwItem != nullptr);
            if (qobject_cast<cwRenderTexturedItemVisibility*>(kwItem->object())) {
                ++count;
            }
        }
        return count;
    };

    auto loadAndSettle = [&]() {
        fileToProject(project, dataset);
        rootData->futureManagerModel()->waitForFinished();
        rootData->scrapManager()->waitForFinish();
        // Drain deferred deletions so removed keyword items leave the model.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    };

    loadAndSettle();
    const int afterFirstLoad = scrapVisibilityCount();
    const int rowsAfterFirstLoad = keywordModel->rowCount();
    REQUIRE(afterFirstLoad > 0);

    // Each reload must return the keyword item model to its baseline, never
    // accumulating leftovers from the previous region. scrapVisibilityCount()
    // guards the scrap items specifically; rowCount() guards every per-entity
    // registry (scrap, line plot, ...) against a reload leak.
    for (int reload = 0; reload < 3; ++reload) {
        loadAndSettle();
        INFO("reload iteration " << reload);
        CHECK(scrapVisibilityCount() == afterFirstLoad);
        CHECK(keywordModel->rowCount() == rowsAfterFirstLoad);
    }
}

// Helper: load a v6 .cw file and return the first note
static cwNote* loadV6FirstNote(cwRootData* rootData, const QString& resourcePath) {
    requireAutomaticUpdatesEnabled();
    auto project = rootData->project();
    fileToProject(project, resourcePath);
    rootData->futureManagerModel()->waitForFinished();

    REQUIRE(project->cavingRegion()->caveCount() == 1);
    auto cave = project->cavingRegion()->cave(0);
    REQUIRE(cave->tripCount() == 1);
    auto trip = cave->trip(0);
    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() == 1);
    return notes.at(0);
}

TEST_CASE("V6 conversion with landscape EXIF coords applies rotation", "[cwScrapManager]") {
    // backgroundRotation-0.08.cw is proto v1 with EXIF Rotate 90 CW.
    // Raw image is 4032x3024 (landscape). Scrap coords are in landscape space.
    // After conversion: originalSize should be post-rotation (3024x4032)
    // and coords should be rotated: (x,y) -> (y, 1-x).
    auto rootData = std::make_unique<cwRootData>();
    auto note = loadV6FirstNote(rootData.get(),
        testcasesDatasetPath("test_cwScrapManager/backgroundRotation-0.08.cw"));
    REQUIRE(note->scraps().size() >= 1);

    // originalSize should be post-rotation
    const QSize storedSize = note->image().originalSize();
    CHECK(storedSize.width() == 3024);
    CHECK(storedSize.height() == 4032);

    // DPI should be set from image
    CHECK(note->image().originalDotsPerMeter() > 0);

    // imageWithAutoTransform should match stored originalSize
    const QString absPath = rootData->project()->absolutePath(note, note->image().path());
    REQUIRE(QFileInfo::exists(absPath));
    QFile file(absPath);
    REQUIRE(file.open(QIODevice::ReadOnly));
    QByteArray data = file.readAll();
    QImage autoImage = cwImageUtils::imageWithAutoTransform(
        data, QFileInfo(absPath).suffix().toLatin1());
    CHECK(autoImage.size() == storedSize);

    // Station coords should be rotated from landscape (~0.660, 0.620)
    // to portrait (~0.620, 0.340)
    auto scrap = note->scrap(0);
    REQUIRE(scrap->stations().size() >= 1);
    const QPointF station0 = scrap->stations().at(0).positionOnNote();
    CHECK(station0.x() == Catch::Approx(0.620).margin(0.01));
    CHECK(station0.y() == Catch::Approx(0.340).margin(0.02));

    // Triangulate and verify it completes
    auto results = rootData->scrapManager()->triangulateScraps({scrap});
    rootData->futureManagerModel()->waitForFinished();
    REQUIRE(!results.isEmpty());
    REQUIRE(AsyncFuture::waitForFinished(results.first().data));
    REQUIRE(results.first().data.resultCount() == 1);
    CHECK(!results.first().data.result().isNull());
}

TEST_CASE("V6 conversion with v5 coords in v6 file applies reNormalization", "[cwScrapManager]") {
    // backgroundRotation-2025.2 to 2025.2-101.cw is proto v6 with v5 landscape coords.
    // V6 always applies reNormalization (not rotation) because real v6 users
    // would have redrawn scraps on the auto-rotated display.
    auto rootData = std::make_unique<cwRootData>();
    auto note = loadV6FirstNote(rootData.get(),
        testcasesDatasetPath("test_cwScrapManager/backgroundRotation-2025.2 to 2025.2-101.cw"));
    REQUIRE(note->scraps().size() >= 1);

    // originalSize should be post-rotation
    CHECK(note->image().originalSize().width() == 3024);
    CHECK(note->image().originalSize().height() == 4032);

    // Station coords are re-normalized from landscape to portrait dims.
    // Original v5 station0: (0.6485, 0.5875)
    // reNorm: x' = x * W/H = 0.6485 * 1.333 ≈ 0.865
    //         y' = y * H/W + (1 - H/W) = 0.5875 * 0.75 + 0.25 ≈ 0.691
    auto scrap = note->scrap(0);
    REQUIRE(scrap->stations().size() >= 1);
    const QPointF station0 = scrap->stations().at(0).positionOnNote();
    CHECK(station0.x() == Catch::Approx(0.865).margin(0.01));
    CHECK(station0.y() == Catch::Approx(0.691).margin(0.02));
}

TEST_CASE("V6 conversion with correct EXIF coords does not modify", "[cwScrapManager]") {
    // backgroundRotation-2025.2-101.cw is proto v6 with post-rotation originalSize (3024x4032).
    // Coords are already correct — no transformation should be applied.
    auto rootData = std::make_unique<cwRootData>();
    auto note = loadV6FirstNote(rootData.get(),
        testcasesDatasetPath("test_cwScrapManager/backgroundRotation-2025.2-101.cw"));
    REQUIRE(note->scraps().size() >= 1);

    // originalSize should remain post-rotation (already correct)
    CHECK(note->image().originalSize().width() == 3024);
    CHECK(note->image().originalSize().height() == 4032);

    // Station coords should be unchanged (~0.621, 0.339)
    auto scrap = note->scrap(0);
    REQUIRE(scrap->stations().size() >= 1);
    const QPointF station0 = scrap->stations().at(0).positionOnNote();
    CHECK(station0.x() == Catch::Approx(0.621).margin(0.005));
    CHECK(station0.y() == Catch::Approx(0.339).margin(0.005));
}

namespace {

// A page one thousand pixels square at one hundred pixels per meter, so a
// scrap station's note position turns into paper meters.
constexpr int kAttachedNotePixels = 1000;
constexpr int kAttachedNoteDotsPerMeter = 100;

// Where the scrap marks a1, a2, and a3 on the page (note y runs up): a2 due
// north of a1 and a3 due east of a2, the way the survey below runs.
const QPointF kA1OnNote(0.2, 0.3);
const QPointF kA2OnNote(0.2, 0.8);
const QPointF kA3OnNote(0.7, 0.8);

constexpr double kScaleTolerance = 1e-3;
constexpr float kPositionToleranceMeters = 0.01f;

//! One survey block, a1 fixed at the origin, a1-a2 due north, a2-a3 due east.
QByteArray threeStationSurvey(const QByteArray& a1a2Tape, bool withA3)
{
    QByteArray survey(
        "*begin s\n"
        "*fix a1 0 0 0\n"
        "*data normal from to tape compass clino\n");
    survey += "a1 a2 " + a1a2Tape + " 0 0\n";
    if (withA3) {
        survey += "a2 a3 10.0 90 0\n";
    }
    survey += "*end s\n";
    return survey;
}

//! A note sized like a real page (so note positions map to paper meters)
//! holding one empty scrap, added to \a trip the way the note editor adds one. A saved note
//! names its image by path, so the project can write it.
cwScrap* addEmptySizedScrap(cwTrip* trip)
{
    cwNote* note = new cwNote();
    note->setName(QStringLiteral("page.png"));
    cwImage image;
    image.setPath(QStringLiteral("page.png"));
    image.setOriginalSize(QSize(kAttachedNotePixels, kAttachedNotePixels));
    image.setOriginalDotsPerMeter(kAttachedNoteDotsPerMeter);
    note->setImage(image);
    trip->notes()->addNotes({note});

    cwScrap* scrap = new cwScrap();
    note->addScrap(scrap);
    return scrap;
}

struct AttachedScrapSetup {
    std::unique_ptr<SavedProjectFixture> fixture;
    QString source;
    cwScrap* scrap = nullptr;
};

//! A saved project whose trip is attached to a three-station source kept
//! outside the project, carrying one note with a scrap anchored at every
//! station.
AttachedScrapSetup makeAttachedScrap(const QString& projectFileBase)
{
    requireAutomaticUpdatesEnabled();
    AttachedScrapSetup setup;
    setup.fixture = makeSavedProject(projectFileBase,
                                     QStringLiteral("ScrapCave"),
                                     QStringLiteral("ScrapTrip"));
    SavedProjectFixture* fixture = setup.fixture.get();

    setup.source = QDir(fixture->tempDir.path()).filePath(QStringLiteral("source/s.svx"));
    REQUIRE(QDir().mkpath(QFileInfo(setup.source).absolutePath()));
    overwriteFile(setup.source, threeStationSurvey("10.0", true));
    attachThroughManager(fixture, fixture->trip, setup.source);
    drainPipelines(fixture);

    setup.scrap = addEmptySizedScrap(fixture->trip);
    const QList<QPair<QString, QPointF>> anchors {
        {QStringLiteral("s.a1"), kA1OnNote},
        {QStringLiteral("s.a2"), kA2OnNote},
        {QStringLiteral("s.a3"), kA3OnNote},
    };
    for (const auto& anchor : anchors) {
        cwNoteStation noteStation;
        noteStation.setName(anchor.first);
        noteStation.setPositionOnNote(anchor.second);
        setup.scrap->addStation(noteStation);
    }
    drainPipelines(fixture);
    fixture->rootData->scrapManager()->waitForFinish();
    return setup;
}

void reloadAndSettle(SavedProjectFixture* fixture)
{
    auto future = managerOf(fixture)->reloadFromSource(fixture->trip);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture);
    fixture->rootData->scrapManager()->waitForFinish();
}

bool nearPosition(const QVector3D& actual, const QVector3D& expected)
{
    return (actual - expected).length() < kPositionToleranceMeters;
}

} // namespace

TEST_CASE("a scrap on an attached trip re-morphs after Reload moves its stations",
          "[cwScrapManager][Attach]")
{
    auto setup = makeAttachedScrap(QStringLiteral("scrap-reload-moves"));
    SavedProjectFixture* fixture = setup.fixture.get();
    cwTrip* trip = fixture->trip;

    REQUIRE(nearPosition(trip->solvedStationPositions().position(QStringLiteral("s.a2")),
                         QVector3D(0.0f, 10.0f, 0.0f)));
    const double scaleBefore = setup.scrap->noteTransformation()->scale();
    REQUIRE(scaleBefore > 0.0);

    // a2 moves ten meters further north and a3 follows it.
    overwriteFile(setup.source, threeStationSurvey("20.0", true));
    reloadAndSettle(fixture);

    const cwStationPositionLookup lookup = trip->solvedStationPositions();
    CHECK(nearPosition(lookup.position(QStringLiteral("s.a2")), QVector3D(0.0f, 20.0f, 0.0f)));
    CHECK(nearPosition(lookup.position(QStringLiteral("s.a3")), QVector3D(10.0f, 20.0f, 0.0f)));

    // The page did not change, so a survey that grew recomputes a smaller
    // paper-to-cave scale: the scrap's anchors followed the reload.
    const double scaleAfter = setup.scrap->noteTransformation()->scale();
    CHECK(scaleAfter > 0.0);
    CHECK(scaleAfter < scaleBefore * (1.0 - kScaleTolerance));
    CHECK(setup.scrap->numberOfStations() == 3);
}

TEST_CASE("a scrap whose station left the source keeps its other anchors and reports the missing one",
          "[cwScrapManager][Attach]")
{
    auto setup = makeAttachedScrap(QStringLiteral("scrap-station-left"));
    SavedProjectFixture* fixture = setup.fixture.get();
    cwTrip* trip = fixture->trip;

    REQUIRE(trip->solvedStationPositions().hasPosition(QStringLiteral("s.a3")));
    const double scaleBefore = setup.scrap->noteTransformation()->scale();
    REQUIRE(scaleBefore > 0.0);

    overwriteFile(setup.source, threeStationSurvey("10.0", false));
    reloadAndSettle(fixture);

    // The scrap still names all three stations; a3 is the one the solve
    // has no position for, and a1-a2 alone carry the note transform.
    REQUIRE(setup.scrap->numberOfStations() == 3);
    CHECK(setup.scrap->station(2).name() == QStringLiteral("s.a3"));
    const cwStationPositionLookup lookup = trip->solvedStationPositions();
    CHECK(lookup.hasPosition(QStringLiteral("s.a1")));
    CHECK(lookup.hasPosition(QStringLiteral("s.a2")));
    CHECK_FALSE(lookup.hasPosition(QStringLiteral("s.a3")));

    // The page draws a1-a2 and a2-a3 to the same scale, so the transform the
    // remaining anchors compute matches the one all three computed.
    CHECK(setup.scrap->noteTransformation()->scale()
          == Catch::Approx(scaleBefore).epsilon(kScaleTolerance));
}

namespace {

// The scale a two-station scrap computes when its stations sit half as far
// apart on the page as they do in the cave.
constexpr double kHalfScale = 0.5;
constexpr double kQuarterScale = 0.25;
constexpr double kMetersPerFoot = 0.3048;

// Where a scrap marks the Walls fixture's P1 and P2 (P2 due east of P1).
const QPointF kP1OnNote(0.2, 0.3);
const QPointF kP2OnNote(0.8, 0.3);

//! Adds a station the way cwBaseNoteStationInteraction::addPoint does (the
//! neighbor guess, else the trip's first known station), then types
//! \a typedName into it the way NoteStation.qml commits an edit.
void addStationLikeTheEditor(cwScrap* scrap, QPointF notePosition, const QString& typedName)
{
    cwNoteStation selected;
    if (scrap->numberOfStations() > 0) {
        selected = scrap->station(scrap->numberOfStations() - 1);
    }

    QString name = scrap->guessNeighborStationName(selected, notePosition);
    if (name.isEmpty()) {
        const QList<cwTrip::KnownStation> known = scrap->parentTrip()->knownStations();
        name = known.isEmpty() ? QStringLiteral("Station Name") : known.first().name;
    }

    cwNoteStation added;
    added.setName(name);
    added.setPositionOnNote(notePosition);
    scrap->addStation(added);
    scrap->setStationData(cwScrap::StationName, scrap->numberOfStations() - 1, typedName);
}

QStringList knownNames(const cwTrip* trip)
{
    QStringList names;
    for (const cwTrip::KnownStation& known : trip->knownStations()) {
        names.append(known.name);
    }
    return names;
}

void settleScraps(SavedProjectFixture* fixture)
{
    drainPipelines(fixture);
    fixture->rootData->scrapManager()->waitForFinish();
    drainPipelines(fixture);
}

std::unique_ptr<SavedProjectFixture> makeProjectWithEmptyCave(const QString& projectFileBase,
                                                             cwCave** cave)
{
    auto fixture = makeSavedProject(projectFileBase,
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    *cave = addEmptyCave(*fixture->project->cavingRegion(), QStringLiteral("AttachedCave"));
    fixture->project->waitSaveToFinish();
    QCoreApplication::processEvents();
    return fixture;
}

cwTrip* wholeCaveWindowOf(const cwCave* cave)
{
    for (cwTrip* trip : cave->trips()) {
        if (trip->windowsWholeCave()) {
            return trip;
        }
    }
    return nullptr;
}

} // namespace

// The trip-level attach path, which keeps computing alongside the cave-level
// attaches below.
TEST_CASE("auto-calculate computes the note transform for a scrap made before the attach's first solve lands",
          "[cwScrapManager][Attach][NoteTransform]")
{
    requireAutomaticUpdatesEnabled();
    auto fixture = makeSavedProject(QStringLiteral("note-auto-trip-before"),
                                    QStringLiteral("ScrapCave"),
                                    QStringLiteral("ScrapTrip"));
    const QString source = QDir(fixture->tempDir.path()).filePath(QStringLiteral("source/s.svx"));
    REQUIRE(QDir().mkpath(QFileInfo(source).absolutePath()));
    overwriteFile(source, threeStationSurvey("10.0", true));
    attachThroughManager(fixture.get(), fixture->trip, source);

    cwScrap* scrap = addEmptySizedScrap(fixture->trip);
    addStationLikeTheEditor(scrap, kA1OnNote, QStringLiteral("s.a1"));
    addStationLikeTheEditor(scrap, kA2OnNote, QStringLiteral("s.a2"));
    settleScraps(fixture.get());

    REQUIRE(fixture->trip->solvedStationPositions().hasPosition(QStringLiteral("s.a2")));
    CHECK(scrap->noteTransformation()->scale()
          == Catch::Approx(kHalfScale).epsilon(kScaleTolerance));
}

TEST_CASE("auto-calculate computes the note transform on the whole-cave window of a Compass cave attach",
          "[cwScrapManager][Attach][NoteTransform]")
{
    requireAutomaticUpdatesEnabled();
    cwCave* cave = nullptr;
    auto fixture = makeProjectWithEmptyCave(QStringLiteral("note-auto-compass-cave"), &cave);
    attachThroughManager(fixture.get(), cave, fixturePath(QStringLiteral("compass_solvable.dat")));
    settleScraps(fixture.get());

    cwTrip* window = wholeCaveWindowOf(cave);
    REQUIRE(window != nullptr);
    INFO("known stations: " << knownNames(window).join(QStringLiteral(", ")).toStdString());
    REQUIRE(window->solvedStationPositions().hasPosition(QStringLiteral("S1")));
    REQUIRE(window->solvedStationPositions().hasPosition(QStringLiteral("S2")));
    CHECK(window->solvedNetwork().neighbors(QStringLiteral("S1")).contains(QStringLiteral("s2"),
                                                                         Qt::CaseInsensitive));

    // S1-S2 runs ten feet north (Compass lengths are feet); the page draws it
    // five meters long.
    cwScrap* scrap = addEmptySizedScrap(window);
    addStationLikeTheEditor(scrap, kA1OnNote, QStringLiteral("S1"));
    addStationLikeTheEditor(scrap, kA2OnNote, QStringLiteral("S2"));
    settleScraps(fixture.get());

    CHECK(scrap->noteTransformation()->scale()
          == Catch::Approx(kHalfScale / kMetersPerFoot).epsilon(kScaleTolerance));
}

TEST_CASE("auto-calculate computes the note transform on a Scope trip of a Walls cave attach",
          "[cwScrapManager][Attach][NoteTransform]")
{
    requireAutomaticUpdatesEnabled();
    cwCave* cave = nullptr;
    auto fixture = makeProjectWithEmptyCave(QStringLiteral("note-auto-walls-cave"), &cave);
    attachThroughManager(fixture.get(), cave,
                         fixturePath(QStringLiteral("walls_prefixed/walls_prefixed.wpj")));
    settleScraps(fixture.get());

    cwTrip* xy = tripForPrefix(cave, QStringLiteral("XY"));
    REQUIRE(xy != nullptr);
    INFO("known stations: " << knownNames(xy).join(QStringLiteral(", ")).toStdString());
    REQUIRE(xy->solvedStationPositions().hasPosition(QStringLiteral("P1")));
    REQUIRE(xy->solvedStationPositions().hasPosition(QStringLiteral("P2")));
    CHECK(xy->solvedNetwork().neighbors(QStringLiteral("P1")).contains(QStringLiteral("p2"),
                                                                      Qt::CaseInsensitive));

    // P1-P2 runs twelve meters east; the page draws it six meters long.
    cwScrap* scrap = addEmptySizedScrap(xy);
    addStationLikeTheEditor(scrap, kP1OnNote, QStringLiteral("P1"));
    addStationLikeTheEditor(scrap, kP2OnNote, QStringLiteral("P2"));
    settleScraps(fixture.get());

    CHECK(scrap->noteTransformation()->scale()
          == Catch::Approx(kHalfScale).epsilon(kScaleTolerance));
}

TEST_CASE("auto-calculate computes the note transform on a Scope trip of a Survex cave attach",
          "[cwScrapManager][Attach][NoteTransform]")
{
    requireAutomaticUpdatesEnabled();
    cwCave* cave = nullptr;
    auto fixture = makeProjectWithEmptyCave(QStringLiteral("note-auto-survex-cave"), &cave);
    attachThroughManager(fixture.get(), cave, fixturePath(QStringLiteral("survex_blocks.svx")));
    settleScraps(fixture.get());

    cwTrip* east = tripForPrefix(cave, QStringLiteral("doghill.big-passage.east"));
    REQUIRE(east != nullptr);
    INFO("known stations: " << knownNames(east).join(QStringLiteral(", ")).toStdString());
    REQUIRE(east->solvedStationPositions().hasPosition(QStringLiteral("e1")));
    REQUIRE(east->solvedStationPositions().hasPosition(QStringLiteral("e2")));
    CHECK(east->solvedNetwork().neighbors(QStringLiteral("e1")).contains(QStringLiteral("e2"),
                                                                        Qt::CaseInsensitive));

    // e1-e2 runs three meters; the page draws it one and a half meters long.
    const QPointF e1OnNote(0.2, 0.3);
    const QPointF e2OnNote(0.2, 0.45);
    cwScrap* scrap = addEmptySizedScrap(east);
    addStationLikeTheEditor(scrap, e1OnNote, QStringLiteral("e1"));
    addStationLikeTheEditor(scrap, e2OnNote, QStringLiteral("e2"));
    settleScraps(fixture.get());

    CHECK(scrap->noteTransformation()->scale()
          == Catch::Approx(kHalfScale).epsilon(kScaleTolerance));
}

TEST_CASE("a scrap on a Scope trip of a Walls cave attach re-morphs after Reload moves its stations",
          "[cwScrapManager][Attach][NoteTransform]")
{
    requireAutomaticUpdatesEnabled();
    cwCave* cave = nullptr;
    auto fixture = makeProjectWithEmptyCave(QStringLiteral("note-auto-walls-reload"), &cave);
    const QString sourceDir = tempSubdir(fixture->tempDir, QStringLiteral("walls-source"));
    const QString project = seedAttachment(sourceDir,
                                           fixturePath(QStringLiteral("walls_prefixed/walls_prefixed.wpj")));
    seedAttachment(sourceDir, fixturePath(QStringLiteral("walls_prefixed/ROOT.SRV")));
    const QString prefixed = seedAttachment(sourceDir,
                                            fixturePath(QStringLiteral("walls_prefixed/PREFIXED.SRV")));
    attachThroughManager(fixture.get(), cave, project);
    settleScraps(fixture.get());

    cwTrip* xy = tripForPrefix(cave, QStringLiteral("XY"));
    REQUIRE(xy != nullptr);
    cwScrap* scrap = addEmptySizedScrap(xy);
    addStationLikeTheEditor(scrap, kP1OnNote, QStringLiteral("P1"));
    addStationLikeTheEditor(scrap, kP2OnNote, QStringLiteral("P2"));
    settleScraps(fixture.get());
    REQUIRE(scrap->noteTransformation()->scale()
            == Catch::Approx(kHalfScale).epsilon(kScaleTolerance));

    // P1-P2 doubles to twenty-four meters while the page stays the same.
    overwriteFile(prefixed,
                  "#PREFIX XY\n"
                  "#DATE 2025-01-02\n"
                  "#UNITS D=Meters A=Degrees V=Degrees DECL=0\n"
                  "P1\tP2\t24.0\t90\t0\n");
    auto future = managerOf(fixture.get())->reloadFromSource(cave);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    REQUIRE_FALSE(future.result().hasError());
    settleScraps(fixture.get());

    CHECK(scrap->noteTransformation()->scale()
          == Catch::Approx(kQuarterScale).epsilon(kScaleTolerance));
}

namespace {
    //What one "Updating Scraps" row said while it was alive, read through the
    //roles a person sees in the task list.
    struct ScrapRunObservation {
        QList<int> progress;
        QList<int> steps;
        QSet<QString> detailNames;

        int distinctProgressCount() const { return QSet<int>(progress.begin(), progress.end()).size(); }
    };

    //Records every scrap run the model announces, one entry per row. Nothing
    //test-only lives on the manager: this is the same data the task list draws.
    class ScrapRunRecorder
    {
    public:
        ScrapRunRecorder(cwFutureManagerModel* model) :
            m_model(model)
        {
            QObject::connect(model, &QAbstractItemModel::rowsInserted, &m_context,
                             [this](const QModelIndex&, int first, int last)
            {
                for(int row = first; row <= last; row++) {
                    if(isScrapRow(row)) {
                        //One observation per row, so two runs that overlap in
                        //the model stay two runs here
                        m_open.append({QPersistentModelIndex(m_model->index(row)),
                                       ScrapRunObservation()});
                    }
                }
            });

            QObject::connect(model, &QAbstractItemModel::dataChanged, &m_context,
                             [this](const QModelIndex& topLeft,
                                    const QModelIndex& bottomRight,
                                    const QList<int>&)
            {
                for(int row = topLeft.row(); row <= bottomRight.row(); row++) {
                    sample(row);
                }
            });

            QObject::connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, &m_context,
                             [this](const QModelIndex&, int first, int last)
            {
                for(int row = first; row <= last; row++) {
                    //The last thing the row says before it goes
                    sample(row);
                    close(m_model->index(row));
                }
            });
        }

        const QList<ScrapRunObservation>& runs() const { return m_runs; }

        //The run that had the most to say — the cold one, when a test does a
        //cold run and a warm one.
        ScrapRunObservation richestRun() const
        {
            ScrapRunObservation richest;
            for(const auto& run : m_runs) {
                if(run.progress.size() > richest.progress.size()) {
                    richest = run;
                }
            }
            return richest;
        }

    private:
        bool isScrapRow(int row) const
        {
            return m_model->data(m_model->index(row), cwFutureManagerModel::NameRole).toString()
                   == QStringLiteral("Updating Scraps");
        }

        ScrapRunObservation* openRun(const QModelIndex& modelIndex)
        {
            for(auto& entry : m_open) {
                if(entry.first == modelIndex) {
                    return &entry.second;
                }
            }
            return nullptr;
        }

        void sample(int row)
        {
            const QModelIndex modelIndex = m_model->index(row);
            ScrapRunObservation* run = openRun(modelIndex);
            if(run == nullptr) {
                return;
            }

            run->progress.append(m_model->data(modelIndex, cwFutureManagerModel::ProgressRole).toInt());
            run->steps.append(m_model->data(modelIndex, cwFutureManagerModel::NumberOfStepRole).toInt());

            const QString detail = m_model->data(modelIndex, cwFutureManagerModel::DetailNameRole).toString();
            if(!detail.isEmpty()) {
                run->detailNames.insert(detail);
            }
        }

        void close(const QModelIndex& modelIndex)
        {
            for(int i = 0; i < m_open.size(); i++) {
                if(m_open.at(i).first == modelIndex) {
                    m_runs.append(m_open.at(i).second);
                    m_open.removeAt(i);
                    return;
                }
            }
        }

        cwFutureManagerModel* m_model;
        QList<QPair<QPersistentModelIndex, ScrapRunObservation>> m_open;
        QList<ScrapRunObservation> m_runs;

        //Owns the model connections: they go when the recorder does
        QObject m_context;
    };

    //Pumps the event loop until the pipeline is done and its row has gone. The
    //detail line is polled on a timer, so a run has to be watched, never waited
    //out in a nested loop.
    void pumpUntilScrapsSettle(cwRootData* rootData, cwScrapManager* scrapManager)
    {
        constexpr int kRunTimeoutMs = 120000;
        constexpr int kPollWaitMs = 2;

        QElapsedTimer timer;
        timer.start();
        while(timer.elapsed() < kRunTimeoutMs
              && (scrapManager->updateState() != cwUpdatable::State::Clean
                  || rootData->futureManagerModel()->rowCount() > 0)) {
            QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents,
                                            kPollWaitMs);
        }
    }
}

TEST_CASE("A scrap run's progress moves in steps finer than one per scrap",
          "[cwScrapManager][Issue671]")
{
    // The row for a scrap run used to hold still until the run was over, which
    // reads as a hang. The run now grows a progress tree as it works, so the bar
    // moves through the crop, the encode, the mesh and the morph of each scrap.
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto model = rootData->futureManagerModel();
    auto scrapManager = rootData->scrapManager();
    REQUIRE(scrapManager != nullptr);

    // The dataset is copied to a fresh temp folder, so the run that loading
    // starts is a cold one: nothing is in the texture cache yet.
    ScrapRunRecorder recorder(model);
    fileToProject(rootData->project(), testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
    pumpUntilScrapsSettle(rootData.get(), scrapManager);

    const int scrapCount = scrapManager->renderScrapCount();
    REQUIRE(scrapCount > 0);
    REQUIRE_FALSE(recorder.runs().isEmpty());

    for(const auto& run : recorder.runs()) {
        INFO("Progress: " << run.progress.size() << " observations");
        REQUIRE_FALSE(run.steps.isEmpty());

        // The range is fixed for the life of the row: the bar's denominator
        // never moves under it.
        const int steps = run.steps.first();
        CHECK(steps > 0);
        CHECK(std::count(run.steps.begin(), run.steps.end(), steps) == run.steps.size());

        // Monotone: the bar stalls when an unhinted parent grows, it never
        // steps backward.
        CHECK(std::is_sorted(run.progress.begin(), run.progress.end()));
    }

    const ScrapRunObservation coldRun = recorder.richestRun();

    // Reaches full: the last thing the row said before it went was its maximum.
    CHECK(coldRun.progress.last() == coldRun.steps.last());

    // Finer than one step per scrap.
    CHECK(coldRun.distinctProgressCount() > scrapCount);

    // ...and the row named what it was working on while it worked.
    CHECK_FALSE(coldRun.detailNames.isEmpty());
}

TEST_CASE("A rerun off a warm texture cache still reaches full",
          "[cwScrapManager][Issue671]")
{
    // The encode is the longest step of a cold run and the one the cache skips.
    // A rerun that finds its textures already compressed grows no
    // "Compressing texture" node at all — no caller declared that step, so
    // nothing has to be kept in sync with the cache.
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto model = rootData->futureManagerModel();
    auto scrapManager = rootData->scrapManager();
    REQUIRE(scrapManager != nullptr);

    fileToProject(rootData->project(), testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
    pumpUntilScrapsSettle(rootData.get(), scrapManager);

    ScrapRunRecorder recorder(model);
    computeAllScraps(scrapManager);
    pumpUntilScrapsSettle(rootData.get(), scrapManager);

    REQUIRE_FALSE(recorder.runs().isEmpty());

    const ScrapRunObservation warmRun = recorder.richestRun();
    CHECK(warmRun.progress.last() == warmRun.steps.last());
    CHECK_FALSE(warmRun.detailNames.contains(QStringLiteral("Compressing texture")));
}

TEST_CASE("Renaming a cave re-marks the scraps of trips in its nested nodes",
          "[cwScrapManager][objectPathReady]")
{
    // A cave rename moves its whole directory, sub/ included, so the note images
    // of every descendant trip move too and their scraps need fresh geometry.
    requireAutomaticUpdatesEnabled();
    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
    rootData->futureManagerModel()->waitForFinished();

    auto* region = rootData->region();
    auto* scrapManager = rootData->scrapManager();
    auto* coordinator = rootData->updateCoordinator();

    cwCave* nested = region->cave(0);
    REQUIRE(nested != nullptr);
    const QList<cwNote*> notes = nested->trip(0)->notes()->notes();
    REQUIRE_FALSE(notes.isEmpty());
    REQUIRE_FALSE(notes.first()->scraps().isEmpty());
    cwScrap* scrap = notes.first()->scraps().first();

    cwCave* outer = SurveyTreeTestHelper::addNode(region, nullptr, cwSurveyNode::Kind::Cave,
                                                  QStringLiteral("Outer"));
    region->moveNode(nested, outer, 0);
    REQUIRE(nested->parentNode() == outer);
    REQUIRE(outer->trips().isEmpty());

    SurveyTreeTestHelper::flushSaves(rootData.get());
    scrapManager->waitForFinish();
    rootData->futureManagerModel()->waitForFinished();

    // Holding the coordinator keeps the re-mark observable in dirtyScraps()
    coordinator->setAutomaticUpdate(false);
    REQUIRE_FALSE(scrapManager->dirtyScraps().contains(scrap));

    outer->setName(QStringLiteral("Outer renamed"));
    SurveyTreeTestHelper::flushSaves(rootData.get());

    CHECK(scrapManager->dirtyScraps().contains(scrap));

    coordinator->setAutomaticUpdate(true);
    rootData->futureManagerModel()->waitForFinished();
    scrapManager->waitForFinish();
}
