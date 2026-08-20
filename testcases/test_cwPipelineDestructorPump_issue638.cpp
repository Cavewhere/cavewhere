// Regression test for issue #638 — the derived-data pipeline destructors used
// to pump the event loop.
//
// cwLinePlotManager, cwScrapManager and cwNoteLiDARManager each used to end
// their destructor with waitToFinish()/waitForFinish(), which calls
// AsyncFuture::waitForFinished() on the restarter's future. For a future still
// in flight that helper spins a nested QEventLoop, so the destructor body ran
// the event queue: queued signals, deferred deletes and timers were delivered
// to half-destroyed objects, and cwUpdatable::beginTeardown() was load-bearing
// solely because of that pump.
//
// The wait is gone. Every worker already ran on value snapshots — the line plot
// solves a cwLinePlotTask::Input copy, scraps a cwTriangulateInData copy, LiDAR
// a cwTriangulateLiDARInData copy — so a worker that outlives its manager reads
// nothing that died with it, and each destructor is now beginTeardown() +
// future.cancel(). The line plot worker also polls isCanceled() between its
// phases, so a canceled run stops instead of paying for cavern.
//
// Each case below kicks a run, posts a zero-timer probe, and destroys the
// manager without returning to the event loop first — so the restarter future
// is guaranteed unfinished, which is exactly the shape that used to pump. The
// probe records whether it was delivered before the delete expression returned;
// these tests hold the destructors to delivering nothing.
//
// One diagnostic note: a destructor that blocks the main thread without pumping
// (say a plain QFuture::waitForFinished()) hangs this test instead of failing
// it — the restarter future completes only through main-thread observation, so
// nothing can finish it while the main thread is parked.

#include <catch2/catch_test_macros.hpp>

#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFutureManagerModel.h"
#include "cwJobSettings.h"
#include "cwLinePlotManager.h"
#include "cwNoteLiDAR.h"
#include "cwNoteLiDARManager.h"
#include "cwNoteLiDARStation.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwScrapManager.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwTrip.h"
#include "LoadProjectHelper.h"
#include "TestHelper.h"

#include <QCoreApplication>
#include <QList>
#include <QTimer>
#include <QUrl>
#include <QVector3D>
#include <QtTest/QSignalSpy>

#include <memory>
#include <utility>

namespace {

constexpr int kRowsInsertedWaitMs = 1000;

struct ProbeState {
    bool fired = false;
    bool deliveredDuringDestructor = false;
    bool destroyReturned = false;
};

//Deletes manager with a zero-timer probe queued and checks that the probe was
//still queued when the delete expression returned. A destructor that pumps the
//event loop delivers the probe early, inside itself.
template <typename Manager>
void checkDestructorDeliversNothing(Manager* manager)
{
    {
        INFO("the run must be in flight, otherwise the destructor has nothing "
             "to wait on and the test proves nothing");
        REQUIRE_FALSE(manager->currentRun().isFinished());
    }

    //Shared with the probe so a probe that outlives this frame — the failing
    //path where delivery is skipped — writes to live memory instead of a dead
    //stack.
    auto state = std::make_shared<ProbeState>();

    QTimer::singleShot(0, [state]() {
        state->fired = true;
        state->deliveredDuringDestructor = !state->destroyReturned;
    });

    delete manager;
    state->destroyReturned = true;

    //Run the probe when the destructor left it queued, so `fired` separates
    //"delivered after the destructor" from "never delivered at all".
    QCoreApplication::processEvents();

    CHECK(state->fired);
    CHECK_FALSE(state->deliveredDuringDestructor);
}

} // namespace

TEST_CASE("issue #638: ~cwLinePlotManager delivers no queued events",
          "[cwLinePlotManager][Issue638]")
{
    cwJobSettings::initialize();

    auto project = fileToProject(testcasesDatasetPath("network.cw"));
    REQUIRE(project->cavingRegion()->caveCount() == 1);

    auto* plotManager = new cwLinePlotManager();
    plotManager->setRegion(project->cavingRegion());

    checkDestructorDeliversNothing(plotManager);
}

TEST_CASE("issue #638: ~cwScrapManager delivers no queued events",
          "[cwScrapManager][Issue638]")
{
    cwJobSettings::initialize();

    auto rootData = std::make_unique<cwRootData>();
    fileToProject(rootData->project(),
                  testcasesDatasetPath("test_cwScrapManager/scrapGuessNeigborPlan.cw"));
    rootData->futureManagerModel()->waitForFinished();
    rootData->linePlotManager()->waitToFinish();
    rootData->scrapManager()->waitForFinish();

    //A manager of its own, so only its destructor is under test.
    auto* scrapManager = new cwScrapManager();
    scrapManager->setProject(rootData->project());
    scrapManager->setRegionTreeModel(rootData->regionTreeModel());
    scrapManager->setLinePlotManager(rootData->linePlotManager());

    scrapManager->markAllScrapsDirty();
    REQUIRE_FALSE(scrapManager->dirtyScraps().isEmpty());
    scrapManager->runIfNeeded();

    checkDestructorDeliversNothing(scrapManager);
}

TEST_CASE("issue #638: ~cwNoteLiDARManager delivers no queued events",
          "[cwNoteLiDARManager][Issue638]")
{
    cwJobSettings::initialize();

    auto rootData = std::make_unique<cwRootData>();

    TestHelper helper;
    helper.loadProjectFromZip(rootData->project(),
                              testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
    rootData->project()->waitLoadToFinish();
    rootData->futureManagerModel()->waitForFinished();
    rootData->linePlotManager()->waitToFinish();

    auto* cave = rootData->region()->cave(0);
    REQUIRE(cave != nullptr);
    auto* trip = cave->trip(0);
    REQUIRE(trip != nullptr);
    auto* lidarModel = trip->notesLiDAR();
    REQUIRE(lidarModel != nullptr);

    const QString lidarFile =
        helper.copyToTempDir(testcasesDatasetPath("lidarProjects/9_15_2025 3.glb"));
    REQUIRE_FALSE(lidarFile.isEmpty());

    QSignalSpy rowsInsertedSpy(lidarModel, &QAbstractItemModel::rowsInserted);
    lidarModel->addFromFiles({ QUrl::fromLocalFile(lidarFile) });
    rootData->futureManagerModel()->waitForFinished();
    if (rowsInsertedSpy.isEmpty()) {
        rowsInsertedSpy.wait(kRowsInsertedWaitMs);
    }
    REQUIRE(lidarModel->rowCount() == 1);

    auto* note = qobject_cast<cwNoteLiDAR*>(
        lidarModel->data(lidarModel->index(0, 0), cwSurveyNoteModelBase::NoteObjectRole)
            .value<QObject*>());
    REQUIRE(note != nullptr);

    const QList<std::pair<QString, QVector3D>> stations = {
        {QStringLiteral("6"), QVector3D(0.19147f, -0.720703f, -2.15723f)},
        {QStringLiteral("7"), QVector3D(3.51028f, -0.0917969f, 5.39945f)},
        {QStringLiteral("5"), QVector3D(-3.48475f, -1.92188f, -3.38263f)}
    };
    for (const auto& [name, position] : stations) {
        cwNoteLiDARStation station;
        station.setName(name);
        station.setPositionOnNote(position);
        note->addStation(station);
    }

    rootData->noteLiDARManager()->waitForFinish();
    rootData->futureManagerModel()->waitForFinished();
    QCoreApplication::processEvents();

    //A manager of its own, so only its destructor is under test.
    auto* liDARManager = new cwNoteLiDARManager();
    liDARManager->setProject(rootData->project());
    liDARManager->setRegionTreeModel(rootData->regionTreeModel());
    liDARManager->setLinePlotManager(rootData->linePlotManager());

    liDARManager->updateLiDARForTrip(trip);

    checkDestructorDeliversNothing(liDARManager);
}
