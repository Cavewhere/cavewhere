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
#include "cwLead.h"
#include "cwFutureManagerModel.h"

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
            if(renderItems->hasItem(id) && !renderItems->item(id).texture.streamed().isNull()) {
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
