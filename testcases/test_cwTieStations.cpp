/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Commit 6c of plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html: the creation path.
// Until now an equate could only be hand-authored in C++ or loaded from disk,
// so these pin the two things a caller must not have to decide — which of the
// two homes a tie travels with, and whether the pair can be tied at all — plus
// the one thing that makes a tie visible: the plot re-solves for it, the same
// way it does for a shot.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwLinePlotManager.h"
#include "cwStationHandle.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QSignalSpy>
#include <QTemporaryDir>

namespace {

//! The one station of \a trip named \a tail, as the identity a tie stores.
cwStationHandle handleOf(const cwTrip* trip, const QString& tail)
{
    const cwStationHandle handle = trip->stationHandle(tail);
    REQUIRE(handle.isValid());
    return handle;
}

} // namespace

TEST_CASE("A tie inside one cave travels with that cave", "[TieStations]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    const cwTrip* first = addNativeTripWithShot(cave, QStringLiteral("First"),
                                                QStringLiteral("A1"), QStringLiteral("A2"));
    const cwTrip* second = addNativeTripWithShot(cave, QStringLiteral("Second"),
                                                 QStringLiteral("B1"), QStringLiteral("B2"));

    REQUIRE(region.tieStations(handleOf(first, QStringLiteral("A2")),
                               handleOf(second, QStringLiteral("B1"))));

    // Which home a tie goes in is not the caller's choice and not a preference:
    // a within-cave tie is part of the cave's definition and has to travel with
    // it, so a cave copied or exported on its own still says what it is joined
    // to.
    REQUIRE(cave->equates()->count() == 1);
    CHECK(region.equates()->count() == 0);

    const cwEquate equate = cave->equates()->equateAt(0);
    REQUIRE(equate.stations().size() == 2);
    CHECK(equate.stations().at(0).tail() == QStringLiteral("A2"));
    CHECK(equate.stations().at(1).tail() == QStringLiteral("B1"));
}

TEST_CASE("A tie between two caves travels with the region", "[TieStations]")
{
    cwCavingRegion region;
    cwCave* alpha = addEmptyCave(region, QStringLiteral("Alpha"));
    cwCave* bravo = addEmptyCave(region, QStringLiteral("Bravo"));
    const cwTrip* alphaTrip = addNativeTripWithShot(alpha, QStringLiteral("Alpha trip"),
                                                    QStringLiteral("A1"), QStringLiteral("A2"));
    const cwTrip* bravoTrip = addNativeTripWithShot(bravo, QStringLiteral("Bravo trip"),
                                                    QStringLiteral("B1"), QStringLiteral("B2"));

    REQUIRE(region.tieStations(handleOf(alphaTrip, QStringLiteral("A2")),
                               handleOf(bravoTrip, QStringLiteral("B1"))));

    // Neither cave can hold it: the tie is the fact that the two connect, so it
    // belongs to whatever holds both.
    CHECK(region.equates()->count() == 1);
    CHECK(alpha->equates()->count() == 0);
    CHECK(bravo->equates()->count() == 0);
}

TEST_CASE("A pair that cannot be one point is not tied", "[TieStations]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    const cwTrip* trip = addNativeTripWithShot(cave, QStringLiteral("Trip"),
                                               QStringLiteral("A1"), QStringLiteral("A2"));

    const cwStationHandle a1 = handleOf(trip, QStringLiteral("A1"));

    // A station is already itself. Recording that as an equate would emit a
    // survex line joining a name to itself, which is why the check is here and
    // not left to the caller.
    CHECK_FALSE(region.tieStations(a1, a1));

    // A handle naming nothing this region holds — a trip from a project that has
    // since been closed, a uuid off a stale row. No home can accept it, so
    // there is no home to put it in.
    const cwStationHandle stray(cwStationHandle::Trip, QUuid::createUuid(),
                                QStringLiteral("A1"));
    CHECK_FALSE(region.tieStations(a1, stray));
    CHECK_FALSE(region.tieStations(stray, a1));

    // An unnamed station names no station.
    CHECK_FALSE(region.tieStations(a1, cwStationHandle()));

    CHECK(cave->equates()->count() == 0);
    CHECK(region.equates()->count() == 0);
}

TEST_CASE("Declaring a tie that already stands changes nothing", "[TieStations]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    const cwTrip* first = addNativeTripWithShot(cave, QStringLiteral("First"),
                                                QStringLiteral("A1"), QStringLiteral("A2"));
    const cwTrip* second = addNativeTripWithShot(cave, QStringLiteral("Second"),
                                                 QStringLiteral("B1"), QStringLiteral("B2"));

    const cwStationHandle a2 = handleOf(first, QStringLiteral("A2"));
    const cwStationHandle b1 = handleOf(second, QStringLiteral("B1"));

    REQUIRE(region.tieStations(a2, b1));

    // A suggester offers the same pair for as long as it stands, and clicking it
    // twice must not stack duplicate equates that then all have to be removed
    // one by one. True either way: the caller asked for the tie to stand, and it
    // does.
    CHECK(region.tieStations(a2, b1));
    CHECK(region.tieStations(b1, a2));
    CHECK(cave->equates()->count() == 1);
}

TEST_CASE("A tie the region already carries is still one tie when a third joins",
          "[TieStations]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    const cwTrip* first = addNativeTripWithShot(cave, QStringLiteral("First"),
                                                QStringLiteral("A1"), QStringLiteral("A2"));
    const cwTrip* second = addNativeTripWithShot(cave, QStringLiteral("Second"),
                                                 QStringLiteral("B1"), QStringLiteral("B2"));

    const cwStationHandle a2 = handleOf(first, QStringLiteral("A2"));
    const cwStationHandle b1 = handleOf(second, QStringLiteral("B1"));
    const cwStationHandle b2 = handleOf(second, QStringLiteral("B2"));

    // An equate is an N-way set, so one that already names both stations says
    // what the caller is asking for even though it was never created as this
    // pair. Matching on the whole equate instead would add a second one saying
    // the same thing.
    cave->equates()->appendEquate(cwEquate(QList<cwStationHandle>({a2, b2, b1})));
    CHECK(region.tieStations(a2, b1));
    CHECK(cave->equates()->count() == 1);
}

TEST_CASE("Tying two stations re-solves the plot", "[TieStations]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);
    REQUIRE(manager.floatingSurveyModel()->isFloating(setup.attached->id()));

    // An equate is a survey input like a shot or a fix — it is the whole reason
    // cavern puts two scopes in one frame — so declaring one has to reach the
    // plot the way entering a shot does. Without this the user makes a tie and
    // the banner that asked for it stays up until something unrelated happens to
    // trigger a run.
    const cwTrip* native = setup.cave->trip(0);
    REQUIRE(region.tieStations(handleOf(setup.attached, QStringLiteral("simple.a1")),
                               handleOf(native, QStringLiteral("A1"))));

    manager.waitToFinish();
    REQUIRE_FALSE(manager.hasSolveError());
    CHECK_FALSE(manager.floatingSurveyModel()->isFloating(setup.attached->id()));
}
