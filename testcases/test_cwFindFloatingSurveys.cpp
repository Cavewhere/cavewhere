/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Commit 6a of plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html: the one
// "trip is floating" signal, fed by both of the passes that can find one.
//
// The two are genuinely unrelated. A native chunk no station name reaches is
// caught before the solve, and stops it. An attached centerline owns no chunk,
// so that pass cannot see it at all, and when it fails to join its cave nothing
// stops anything: cavern either solves it happily in a corner of its own or
// warns once and exits zero. That silent case is the one the suggester exists
// for, and the one these tests pin.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwFindFloatingSurveys.h"
#include "cwLinePlotManager.h"
#include "cwScopeLabels.h"
#include "cwStationHandle.h"
#include "cwSurveyNetwork.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QDir>
#include <QHash>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QUuid>

namespace {

using Result = cwFindFloatingSurveys::Result;

// Solves `region` with the given trip attachments and returns the manager's
// floating-survey answer. The manager outlives the call through the reference
// the caller holds, so a caller can go on to assert on its solve state.
QList<Result> solveAndFindFloating(cwLinePlotManager& manager,
                                   cwCavingRegion& region,
                                   const QHash<QUuid, QString>& tripDirs)
{
    solveRegion(manager, region, tripDirs);
    return manager.floatingSurveys();
}

// The single record for `trip`, failing the test when the answer holds no
// record for it or more than one.
Result recordFor(const QList<Result>& floating, const cwTrip* trip)
{
    QList<Result> matches;
    for (const Result& result : floating) {
        if (result.tripId == trip->id()) {
            matches.append(result);
        }
    }
    REQUIRE(matches.size() == 1);
    return matches.first();
}

} // namespace

TEST_CASE("A native chunk no station name reaches floats before the solve",
          "[FloatingSurveys]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));

    // The first valid chunk seeds the connected set, so Main anchors the cave
    // and Stray — sharing no station name with it — is what floats.
    addNativeTripWithShot(cave, QStringLiteral("Main"),
                          QStringLiteral("A1"), QStringLiteral("A2"));
    cwTrip* stray = addNativeTripWithShot(cave, QStringLiteral("Stray"),
                                          QStringLiteral("Z1"), QStringLiteral("Z2"));

    cwLinePlotManager manager;
    const QList<Result> floating = solveAndFindFloating(manager, region, {});

    // This pass runs instead of the solve, not alongside it.
    CHECK(manager.hasSolveError());

    REQUIRE(floating.size() == 1);
    const Result result = recordFor(floating, stray);
    CHECK(result.caveId == cave->id());
    CHECK(result.trigger == Result::Trigger::UnconnectedChunks);
    CHECK(result.stations == QStringList({QStringLiteral("z1"), QStringLiteral("z2")}));
}

TEST_CASE("An attached centerline nothing ties in floats even when cavern is happy",
          "[FloatingSurveys]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    const QList<Result> floating = solveAndFindFloating(manager, region, setup.tripDirs);

    // The whole point of this trigger: survex_simple.svx fixes its own A1, so
    // cavern solves it in a corner of its own and reports nothing at all.
    //
    // It is also why the answer is read from the region's declared equates and
    // not from the graph cavern solved. Both surveys here are fixed at the
    // origin and run 10 m due north, and .3d legs name their endpoints only by
    // coordinate — so the parsed network wires this cave's a1 to the
    // attachment's simple.a2, and a component walk sees one connected cave.
    INFO("solve error: " << manager.solveErrorMessage().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    REQUIRE(floating.size() == 1);
    const Result result = recordFor(floating, setup.attached);
    CHECK(result.caveId == setup.cave->id());
    CHECK(result.trigger == Result::Trigger::ExternalScope);

    // Reported cave-local and canonical, the spelling cwStationPositionLookup
    // and cwTrip::solvedStations both use, so a caller can look each one up.
    const QString scope = setup.attached->scopePrefix().toLower();
    CHECK(result.stations == QStringList({scope + QStringLiteral("simple.a1"),
                                          scope + QStringLiteral("simple.a2"),
                                          scope + QStringLiteral("simple.a3")}));
}

TEST_CASE("An attached centerline cavern dropped as hanging floats with the names its file gave",
          "[FloatingSurveys]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_hanging.svx"));

    cwLinePlotManager manager;
    const QList<Result> floating = solveAndFindFloating(manager, region, setup.tripDirs);

    // Cavern warns and exits zero, so the solve "succeeded" while placing not
    // one station of this survey.
    INFO("solve error: " << manager.solveErrorMessage().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());
    CHECK(manager.cavernLog().contains(QStringLiteral("not all connected")));
    REQUIRE(setup.attached->solvedStations().isEmpty());

    REQUIRE(floating.size() == 1);
    const Result result = recordFor(floating, setup.attached);
    CHECK(result.trigger == Result::Trigger::ExternalScope);

    // Which is the whole reason the scan harvests: the names are read from the
    // attachment on its own, then scoped to the trip so the record reads
    // cave-local like one the solve wrote.
    const QString scope = setup.attached->scopePrefix().toLower();
    CHECK(result.stations == QStringList({scope + QStringLiteral("hanging.h1"),
                                          scope + QStringLiteral("hanging.h2"),
                                          scope + QStringLiteral("hanging.h3")}));
}

TEST_CASE("An equate that ties an attached centerline in stops it floating",
          "[FloatingSurveys]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    // The tie the suggester will eventually create on the user's behalf. It is
    // the whole loop this signal drives: float -> tie -> silence.
    setup.cave->equates()->appendEquate(cwEquate({
        cwStationHandle(cwStationHandle::NativeCave, setup.cave->id(), QStringLiteral("A2")),
        cwStationHandle(cwStationHandle::Trip, setup.attached->id(),
                        QStringLiteral("simple.a1"))}));

    cwLinePlotManager manager;
    const QList<Result> floating = solveAndFindFloating(manager, region, setup.tripDirs);

    INFO("solve error: " << manager.solveErrorMessage().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());
    CHECK(floating.isEmpty());
}

TEST_CASE("A solve that never produced a topology reports nothing floating",
          "[FloatingSurveys]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    addAttachedTrip(cave, QStringLiteral("Attached"), QStringLiteral("survex_simple.svx"));

    // An empty network is "no answer yet", not "everything is hanging" — the
    // distinction a banner bound to this list depends on, since a manager
    // publishes its first result before any solve has run.
    const cwCavingRegionData snapshot = region.data();
    CHECK(cwFindFloatingSurveys::fromExternalScopes(snapshot,
                                                    cwSurveyNetwork(),
                                                    cwScopeLabels(snapshot))
              .isEmpty());
    CHECK(cave->tripCount() == 1);
}

TEST_CASE("A run that stops before the solve keeps the answer it could not recheck",
          "[FloatingSurveys]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    REQUIRE(solveAndFindFloating(manager, region, setup.tripDirs).size() == 1);

    // A second cave whose native chunks share no station name. That check runs
    // instead of the solve, region-wide, so this run never reaches the
    // post-solve pass that found the attachment above.
    cwCave* broken = addEmptyCave(region, QStringLiteral("Bravo"));
    addNativeTripWithShot(broken, QStringLiteral("Main"),
                          QStringLiteral("B1"), QStringLiteral("B2"));
    cwTrip* stray = addNativeTripWithShot(broken, QStringLiteral("Stray"),
                                          QStringLiteral("Z1"), QStringLiteral("Z2"));
    manager.waitToFinish();

    REQUIRE(manager.hasSolveError());

    // Both halves: Bravo's failure says nothing about Alpha's attachment.
    const QList<Result> floating = manager.floatingSurveys();
    CHECK(recordFor(floating, stray).trigger == Result::Trigger::UnconnectedChunks);
    CHECK(recordFor(floating, setup.attached).trigger == Result::Trigger::ExternalScope);
    CHECK(floating.size() == 2);
}

TEST_CASE("The floating-survey signal stays quiet when the answer is unchanged",
          "[FloatingSurveys]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    QSignalSpy spy(&manager, &cwLinePlotManager::floatingSurveysChanged);

    REQUIRE(solveAndFindFloating(manager, region, setup.tripDirs).size() == 1);
    REQUIRE(spy.size() == 1);

    // A re-solve of the same data must not re-pulse a banner the user is
    // already looking at.
    manager.rerunSurvex();
    manager.waitToFinish();
    CHECK(spy.size() == 1);
}
