/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Commit 6c of plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html: one trip's slice
// of the floating answer, as properties instead of questions.
//
// The point of the type is that a surface never re-asks. So these pin the
// arriving, not the answer: bind first, let the run land, and the properties
// have to have moved on their own — and having moved, they must carry both
// spellings of the same stations, the one a label shows and the one a tie
// stores.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFindFloatingSurveys.h"
#include "cwFloatingSurveyModel.h"
#include "cwFloatingSurveyStatus.h"
#include "cwLinePlotManager.h"
#include "cwStationHandle.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>

TEST_CASE("A trip's floating answer arrives without the surface asking again",
          "[FloatingSurveyStatus]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;

    // Bound before the solve on purpose: this is the panel the user is already
    // looking at, so the answer has to reach it by arriving, not by being
    // fetched when something else happens to change.
    cwFloatingSurveyStatus status;
    status.setModel(manager.floatingSurveyModel());
    status.setTrip(setup.attached);
    REQUIRE_FALSE(status.floating());

    QSignalSpy statusSpy(&status, &cwFloatingSurveyStatus::statusChanged);
    solveRegion(manager, region, setup.tripDirs);

    REQUIRE_FALSE(manager.hasSolveError());
    CHECK(statusSpy.size() == 1);
    CHECK(status.floating());

    const QStringList tripLocal({QStringLiteral("simple.a1"),
                                 QStringLiteral("simple.a2"),
                                 QStringLiteral("simple.a3")});
    CHECK(status.stations() == tripLocal);

    // Both spellings of the same stations, in the same order — the suggester
    // proposes a tie from the handles and shows the strings, so a surface that
    // pairs them by index has to be right to do so.
    const QList<cwStationHandle> handles = status.stationHandles();
    REQUIRE(handles.size() == tripLocal.size());
    for (int i = 0; i < handles.size(); ++i) {
        INFO("station " << i);
        CHECK(handles.at(i).scope() == cwStationHandle::Trip);
        CHECK(handles.at(i).containerId() == setup.attached->id());
        CHECK(handles.at(i).tail() == tripLocal.at(i));
    }
}

TEST_CASE("The trip that anchors its cave is answered for too",
          "[FloatingSurveyStatus]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);

    // A trip with no row is not an absent answer, it is a negative one — the
    // banner it drives has to stay hidden rather than never be evaluated.
    cwFloatingSurveyStatus status;
    status.setModel(manager.floatingSurveyModel());
    status.setTrip(setup.cave->trip(0));

    CHECK_FALSE(status.floating());
    CHECK(status.stations().isEmpty());
    CHECK(status.stationHandles().isEmpty());
}

TEST_CASE("A survey cavern dropped outright still says it floats",
          "[FloatingSurveyStatus]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_hanging.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);

    // Nothing fixes it and nothing ties it in, so none of its stations are
    // placed — and the surface still has names to show, because the scan reads
    // the attachment on its own. floating stays the thing that decides whether
    // the banner appears: a file cavern cannot read at all leaves this list
    // empty while floating just as badly.
    cwFloatingSurveyStatus status;
    status.setModel(manager.floatingSurveyModel());
    status.setTrip(setup.attached);

    REQUIRE_FALSE(manager.hasSolveError());
    REQUIRE(setup.attached->solvedStations().isEmpty());
    CHECK(status.floating());
    CHECK(status.stations() == QStringList({QStringLiteral("hanging.h1"),
                                            QStringLiteral("hanging.h2"),
                                            QStringLiteral("hanging.h3")}));
    CHECK(status.stationHandles().size() == 3);
}

TEST_CASE("Rebinding to another trip re-answers for that one",
          "[FloatingSurveyStatus]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);

    cwFloatingSurveyStatus status;
    status.setModel(manager.floatingSurveyModel());
    status.setTrip(setup.attached);
    REQUIRE(status.floating());

    // The surface is about the trip it is bound to, not about the cave holding
    // a floating one — the page moves between trips without being rebuilt.
    status.setTrip(setup.cave->trip(0));
    CHECK_FALSE(status.floating());
    CHECK(status.stations().isEmpty());

    status.setTrip(nullptr);
    CHECK_FALSE(status.floating());
}

TEST_CASE("A trip the region dropped stops floating",
          "[FloatingSurveyStatus]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);

    cwFloatingSurveyStatus status;
    status.setModel(manager.floatingSurveyModel());
    status.setTrip(setup.attached);
    REQUIRE(status.floating());

    QSignalSpy statusSpy(&status, &cwFloatingSurveyStatus::statusChanged);

    // The run's records outlive the trip they name by a run, so the model drops
    // the row against the live region. Asserting before the re-solve is the
    // point: the answer must not wait on a whole cavern run to stop standing.
    setup.cave->removeTrip(setup.cave->tripCount() - 1);
    CHECK_FALSE(status.floating());
    CHECK(status.stations().isEmpty());
    CHECK(statusSpy.size() == 1);

    manager.waitToFinish();
    CHECK_FALSE(status.floating());
}

TEST_CASE("An answer that did not move is not re-announced",
          "[FloatingSurveyStatus]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);

    cwFloatingSurveyStatus status;
    status.setModel(manager.floatingSurveyModel());
    status.setTrip(setup.attached);
    REQUIRE(status.floating());

    QSignalSpy statusSpy(&status, &cwFloatingSurveyStatus::statusChanged);

    // Renaming the cave rebuilds every row, and the answer for this trip is
    // unchanged by it. A surface binds these properties, so re-announcing an
    // answer that did not move is churn a whole view tree pays for.
    setup.cave->setName(QStringLiteral("Bravo"));
    CHECK(statusSpy.size() == 0);
    CHECK(status.floating());

    manager.waitToFinish();
    CHECK(status.floating());
}

TEST_CASE("The answer does not outlive the model that gave it",
          "[FloatingSurveyStatus]")
{
    cwFloatingSurveyStatus status;
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    const cwTrip* stray = addNativeTripWithShot(cave, QStringLiteral("Stray"),
                                                QStringLiteral("Z1"),
                                                QStringLiteral("Z2"));

    {
        cwFloatingSurveyModel model;
        model.setRegion(&region);
        model.setResults({cwFindFloatingSurveys::Result{
            cave->id(),
            stray->id(),
            cwFindFloatingSurveys::Result::Trigger::UnconnectedChunks,
            QStringList({QStringLiteral("z1"), QStringLiteral("z2")})}});

        status.setModel(&model);
        status.setTrip(cave->trip(0));
        REQUIRE(status.floating());
    }

    // A QPointer going null is not enough on its own: these are cached
    // properties, and nothing else would ever tell a bound surface that the
    // thing which answered is gone.
    CHECK_FALSE(status.floating());
    CHECK(status.stations().isEmpty());
    CHECK(status.stationHandles().isEmpty());
}
