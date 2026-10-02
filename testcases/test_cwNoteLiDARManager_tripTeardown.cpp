//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
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
#include "cwStationPositionLookup.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwTrip.h"
#include "cwUpdatable.h"
#include "cwUpdateCoordinator.h"
#include "LoadProjectHelper.h"

//Qt includes
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QUndoStack>
#include <QVector3D>

//Std includes
#include <optional>

// The cwNoteLiDARManager side of issue #637, whose scrap half is in
// test_cwScrapManager_tripTeardown.cpp.
//
// isRunnableNote() answered a question about one note by walking note -> trip ->
// cave. A trip destroys its own members before ~QObject deletes the note models
// it owns, so a note that dies during a trip's teardown announces itself while
// the trip in the middle of that chain is already gone — and the manager answers
// by walking the dirty set, reading that dead trip through every note still in
// it. The manager now caches each dirty note's trip and cave when it is marked,
// so the predicate reads only the entry it is asked about.
//
// Two things keep this manager out of that window, and there is a case for each:
// the dirty set is emptied when a trip's row leaves the region, and what survives
// that (shutdown, which removes no rows) is answered from the cached ancestors.
//
// Worth re-running under Address Sanitizer against any change to the dirty set:
//
//   ASAN_OPTIONS=detect_container_overflow=0 \
//     ./build/<preset-Debug>/cavewhere-test "[tripTeardown637]"

namespace {
    //Any position works — isRunnable() only asks whether the note has stations,
    //never where they are.
    const QVector3D kStationPositionOnNote(0.19147f, -0.720703f, -2.15723f);

    cwNoteLiDAR* addNoteWithStation(cwSurveyNoteLiDARModel* model, const QString& stationName)
    {
        auto note = new cwNoteLiDAR();
        model->addNotes({note});

        cwNoteLiDARStation station;
        station.setName(stationName);
        station.setPositionOnNote(kStationPositionOnNote);
        note->addStation(station);

        return note;
    }

    //A cave with a solved centerline and one trip, holding two LiDAR notes that
    //are both marked dirty and left that way. Two is the minimum for the read the
    //issue describes: the first note to be deleted has to find a sibling still in
    //the dirty set for the manager to walk to it.
    struct DirtyLiDARFixture {
        explicit DirtyLiDARFixture(cwRootData* rootData) {
            auto project = rootData->project();

            helper.loadProjectFromZip(project,
                                      testcasesDatasetPath("lidarProjects/jaws of the beast.zip"));
            project->waitLoadToFinish();
            rootData->futureManagerModel()->waitForFinished();
            rootData->linePlotManager()->waitToFinish();

            auto region = project->cavingRegion();
            REQUIRE(region->caveCount() == 1);
            cave = region->cave(0);
            REQUIRE(cave->tripCount() == 1);
            trip = cave->trip(0);

            //A solved centerline is half of what makes a dirty note runnable;
            //stations on the note are the other half.
            REQUIRE_FALSE(cave->stationPositionLookup().positions().isEmpty());

            auto model = trip->notesLiDAR();
            REQUIRE(model->rowCount() == 0);

            //Automatic update off keeps the dirty set marked instead of computed,
            //which is the state the app sits in whenever a project is closed
            //mid-edit.
            rootData->updateCoordinator()->setAutomaticUpdate(false);
            rootData->noteLiDARManager()->waitForFinish();
            rootData->futureManagerModel()->waitForFinished();

            //Adding a note marks it dirty, since the manager watches the model.
            firstNote = addNoteWithStation(model, QStringLiteral("6"));
            siblingNote = addNoteWithStation(model, QStringLiteral("7"));
            REQUIRE(model->rowCount() == 2);
            REQUIRE(rootData->noteLiDARManager()->updateState() == cwUpdatable::State::Dirty);
        }

        TestHelper helper;
        cwCave* cave = nullptr;
        cwTrip* trip = nullptr;
        cwNoteLiDAR* firstNote = nullptr;
        cwNoteLiDAR* siblingNote = nullptr;
    };
}

// Removing a cave never reaches the teardown window at all, because the region
// tree emits the trip's row before the cave's and the manager empties the dirty
// set right then. Losing that would hand the case below every removal path in the
// app, so it is worth stating.
TEST_CASE("Removing a cave takes its LiDAR notes out of the dirty set before they die",
          "[cwNoteLiDARManager][tripTeardown637]")
{
    cwJobSettings::initialize();

    auto rootData = std::make_unique<cwRootData>();
    auto manager = rootData->noteLiDARManager();
    DirtyLiDARFixture fixture(rootData.get());

    QPointer<cwNoteLiDAR> firstNote(fixture.firstNote);
    QPointer<cwNoteLiDAR> siblingNote(fixture.siblingNote);

    //Removing a cave hands it to the undo command, which deleteLater()s it once
    //the stack is cleared. Nothing is destroyed until those are delivered below.
    rootData->project()->cavingRegion()->removeCave(0);

    //The notes are still alive, and already out of the dirty set: the manager
    //heard the trip's row leave and dropped them there, rather than waiting for
    //each note's own destroyed().
    REQUIRE_FALSE(firstNote.isNull());
    REQUIRE_FALSE(siblingNote.isNull());
    CHECK(manager->updateState() == cwUpdatable::State::Clean);

    //Clearing the stack is what destroys the removed cave, and with it the trip
    //and its notes.
    rootData->undoStack()->clear();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    CHECK(firstNote.isNull());
    CHECK(siblingNote.isNull());
    CHECK(manager->updateState() == cwUpdatable::State::Clean);
}

// Shutdown is the path that does reach the window: it removes no rows, so the
// notes are still dirty when their trip dies underneath them.
TEST_CASE("Shutting down must not read the dying trip through a sibling LiDAR note",
          "[cwNoteLiDARManager][tripTeardown637]")
{
    cwJobSettings::initialize();

    auto rootData = std::make_unique<cwRootData>();
    QPointer<cwNoteLiDARManager> manager = rootData->noteLiDARManager();
    DirtyLiDARFixture fixture(rootData.get());

    //destroyed() fires at the top of ~QObject, so the trip reports itself dead
    //once its members are gone and before it deletes the models holding the
    //notes.
    bool tripDestroyed = false;
    QObject::connect(fixture.trip, &QObject::destroyed, fixture.trip, [&tripDestroyed]() {
        tripDestroyed = true;
    });

    bool siblingDestroyed = false;
    QObject::connect(fixture.siblingNote, &QObject::destroyed, fixture.siblingNote,
                     [&siblingDestroyed]() {
        siblingDestroyed = true;
    });

    //The manager connects each note's destroyed() when the note is inserted, so
    //its handler for this note has already run — and already walked the dirty set
    //to the sibling — when this fires. This samples the moment the issue's stack
    //fires.
    bool tripWasDeadByThen = false;
    bool siblingWasAliveByThen = false;
    bool managerWasAliveByThen = false;
    std::optional<cwUpdatable::State> stateDuringTeardown;
    QObject::connect(fixture.firstNote, &QObject::destroyed, fixture.firstNote, [&]() {
        tripWasDeadByThen = tripDestroyed;
        siblingWasAliveByThen = !siblingDestroyed;
        managerWasAliveByThen = !manager.isNull();
        if(managerWasAliveByThen) {
            stateDuringTeardown = manager->updateState();
        }
    });

    //Closing the app destroys the region without removing a single row, so the
    //notes reach their own destructors still in the dirty set.
    rootData.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    //The window really was the one #637 describes: the trip died before the notes
    //it owns, and the sibling was still there to be read. Both are what give the
    //state below its meaning — an empty dirty set reports Clean on its own.
    REQUIRE(managerWasAliveByThen);
    REQUIRE(stateDuringTeardown.has_value());
    REQUIRE(siblingWasAliveByThen);
    CHECK(tripWasDeadByThen);

    //A note whose trip is gone has nothing left to triangulate, so the only
    //honest answer is Clean. The manager reaches it from the cached ancestors —
    //the QPointer to the trip nulls with the trip — rather than by asking the
    //dead trip for its cave.
    CHECK(stateDuringTeardown.value() == cwUpdatable::State::Clean);

    CHECK(siblingDestroyed);
}
