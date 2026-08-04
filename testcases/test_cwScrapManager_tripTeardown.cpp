//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFutureManagerModel.h"
#include "cwJobSettings.h"
#include "cwNote.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwScrap.h"
#include "cwScrapManager.h"
#include "cwSurveyNoteModel.h"
#include "cwTrip.h"
#include "cwUpdatable.h"
#include "cwUpdateCoordinator.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QCoreApplication>
#include <QEvent>
#include <QUndoStack>

//Std includes
#include <optional>

// Reproduces issue #637.
//
// isRunnableScrap() answers a question about one scrap by walking scrap -> trip
// -> cave. A trip destroys its own members before ~QObject deletes its notes and
// their scraps, so the first scrap to die announces itself while the trip in the
// middle of that chain is already gone. The manager answers by walking the dirty
// set, and every scrap still in it reads the dead trip.
//
// The manager now caches each dirty scrap's cave when it is marked, so the
// predicate reads only the entry it is asked about and the trip in the middle is
// never touched. Before that, this failed two ways: on the state reported
// mid-teardown, and under Address Sanitizer on the read itself. Both are worth
// re-running against any change to the dirty set:
//
//   ASAN_OPTIONS=detect_container_overflow=0 \
//     ./build/<preset-Debug>/cavewhere-test "[tripTeardown637]"
TEST_CASE("Deleting a cave must not read the dying trip through a sibling scrap",
          "[cwScrapManager][tripTeardown637]")
{
    cwJobSettings::initialize();

    auto rootData = std::make_unique<cwRootData>();
    auto project = rootData->project();
    auto scrapManager = rootData->scrapManager();

    fileToProject(project, testcasesDatasetPath("test_cwScrapManager/ProjectProfile-test-v3.cw"));
    rootData->futureManagerModel()->waitForFinished();

    auto region = project->cavingRegion();

    //One cave holding one trip is what frees the cave's QPointer control block
    //partway through the teardown, which is what makes the read observable. A
    //second trip would keep that block referenced and hide the same defect.
    REQUIRE(region->caveCount() == 1);
    auto cave = region->cave(0);
    REQUIRE(cave->tripCount() == 1);
    auto trip = cave->trip(0);

    auto notes = trip->notes()->notes();
    REQUIRE(notes.size() == 1);
    auto note = notes.at(0);
    REQUIRE(note->scraps().size() == 1);

    //Two dirty scraps under the one trip is the minimum: the first to be deleted
    //has to find a sibling still in the dirty set for the manager to read it.
    note->addScrap(new cwScrap());
    REQUIRE(note->scraps().size() == 2);
    auto firstScrap = note->scraps().at(0);
    auto siblingScrap = note->scraps().at(1);

    //Automatic update off keeps the dirty set marked instead of computed, which
    //is the state the app sits in whenever a cave is closed mid-edit.
    rootData->updateCoordinator()->setAutomaticUpdate(false);
    scrapManager->waitForFinish();
    rootData->futureManagerModel()->waitForFinished();

    scrapManager->markAllScrapsDirty();
    REQUIRE(scrapManager->dirtyScraps().contains(firstScrap));
    REQUIRE(scrapManager->dirtyScraps().contains(siblingScrap));

    //destroyed() fires at the top of ~QObject, so the trip reports itself dead
    //once its members are gone and before it deletes the notes holding the
    //scraps.
    bool tripDestroyed = false;
    QObject::connect(trip, &QObject::destroyed, trip, [&tripDestroyed]() {
        tripDestroyed = true;
    });

    //Connected after markAllScrapsDirty(), so the manager's handler for this
    //scrap has already run — and already read the sibling — when this fires.
    //This samples the moment the issue's stack fires.
    bool tripWasDeadByThen = false;
    std::optional<cwUpdatable::State> stateDuringTeardown;
    QObject::connect(firstScrap, &QObject::destroyed, firstScrap, [&]() {
        tripWasDeadByThen = tripDestroyed;
        stateDuringTeardown = scrapManager->updateState();
    });

    //Removing a cave hands it to the undo command, which deleteLater()s it once
    //the stack is cleared. This is what loading a second project does.
    region->removeCave(0);
    rootData->undoStack()->clear();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    //The window really was the one #637 describes: the trip died before the
    //scraps it owns.
    REQUIRE(stateDuringTeardown.has_value());
    CHECK(tripWasDeadByThen);

    //The failing assertion. A cave on its way out has no scrap left worth
    //triangulating, so the only honest answer is Clean. Today the manager says
    //Dirty because isRunnableScrap() reaches through the dead trip and gets a
    //cave pointer that only looks alive. Either fix direction in #637 turns this
    //Clean: caching the cave gives a QPointer that nulls with the cave, and
    //purging the dirty set on removal leaves nothing to walk.
    CHECK(stateDuringTeardown.value() == cwUpdatable::State::Clean);

    CHECK(scrapManager->dirtyScraps().isEmpty());
}
