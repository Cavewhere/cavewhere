/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Commit 6c of plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html: the suggester.
//
// A floating survey has no position in its cave's frame, so nothing here can be
// ranked by distance and these pin the name reasoning instead: which name is
// compared (the leaf, not the whole tail), which candidate outranks which, and
// that acting on a row actually ends the float.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwEquateModel.h"
#include "cwFloatingSurveyModel.h"
#include "cwLinePlotManager.h"
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTieSuggestionModel.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QSignalSpy>
#include <QTemporaryDir>

namespace {

QString stationAt(const cwTieSuggestionModel& model, int row)
{
    return roleAt(&model, row, cwTieSuggestionModel::StationRole).toString();
}

QString candidateAt(const cwTieSuggestionModel& model, int row)
{
    return roleAt(&model, row, cwTieSuggestionModel::CandidateStationRole).toString();
}

QString candidateTripAt(const cwTieSuggestionModel& model, int row)
{
    return roleAt(&model, row, cwTieSuggestionModel::CandidateTripNameRole).toString();
}

cwTieSuggestionModel::Match matchAt(const cwTieSuggestionModel& model, int row)
{
    const QVariant match = roleAt(&model, row, cwTieSuggestionModel::MatchRole);
    //An unserved role converts to 0, which is SameName — the answer half these
    //cases are checking for, so without this they pass whether or not the role
    //is there at all.
    REQUIRE(match.isValid());
    return match.value<cwTieSuggestionModel::Match>();
}

//! Extends \a trip's first chunk by one shot, so a test can give a trip more
//! stations than addNativeTripWithShot's two.
void appendStation(cwTrip* trip, const QString& fromName, const QString& toName)
{
    REQUIRE(trip->chunkCount() > 0);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("10.0")));
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    trip->chunk(0)->appendShot(cwStation(fromName), cwStation(toName), shot);
}

} // namespace

TEST_CASE("A station named alike anywhere in the cave is the suggestion",
          "[TieSuggestionModel]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* first = addNativeTripWithShot(cave, QStringLiteral("First"),
                                          QStringLiteral("A1"), QStringLiteral("A2"));
    addNativeTripWithShot(cave, QStringLiteral("Second"),
                          QStringLiteral("A2"), QStringLiteral("B7"));

    cwTieSuggestionModel model;
    model.setTrip(first);

    // A2 is in both trips, so it is the one pair a person would make, and it
    // comes first however far down the list its station sorts. A1 has no twin
    // and takes a near match instead — but only from the stations that start
    // like it, so B7 is no more a candidate for A1 than any other station in the
    // cave is.
    REQUIRE(model.rowCount() == 2);
    CHECK(stationAt(model, 0) == QStringLiteral("A2"));
    CHECK(candidateAt(model, 0) == QStringLiteral("A2"));
    CHECK(candidateTripAt(model, 0) == QStringLiteral("Second"));
    CHECK(matchAt(model, 0) == cwTieSuggestionModel::SameName);

    CHECK(stationAt(model, 1) == QStringLiteral("A1"));
    CHECK(candidateAt(model, 1) == QStringLiteral("A2"));
    CHECK(matchAt(model, 1) == cwTieSuggestionModel::SameLetterPrefix);
}

TEST_CASE("A trip is never offered its own stations", "[TieSuggestionModel]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* only = addNativeTripWithShot(cave, QStringLiteral("Only"),
                                         QStringLiteral("A1"), QStringLiteral("A2"));

    cwTieSuggestionModel model;
    model.setTrip(only);

    // The one trip in a cave shares every name with itself. Tying a station to
    // itself is not a tie, and it is the state a brand-new cave is in.
    CHECK(model.rowCount() == 0);
    CHECK_FALSE(model.truncated());
}

TEST_CASE("Nothing is suggested for a trip with no cave", "[TieSuggestionModel]")
{
    cwTrip orphan;

    cwTieSuggestionModel model;
    model.setTrip(&orphan);
    CHECK(model.rowCount() == 0);

    model.setTrip(nullptr);
    CHECK(model.rowCount() == 0);
}

TEST_CASE("Only the leaf of a station's name is compared", "[TieSuggestionModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);
    REQUIRE(manager.floatingSurveyModel()->isFloating(setup.attached->id()));

    cwTieSuggestionModel model;
    model.setTrip(setup.attached);

    // The attached file keeps its own "*begin Simple" inside the trip's scope,
    // so its station reads "simple.a1" where the native station beside it reads
    // "A1". Comparing the tails whole finds nothing at all — and finds it for
    // exactly the surveys that float, since an attachment is what floats.
    REQUIRE(model.rowCount() == 4);
    CHECK(stationAt(model, 0) == QStringLiteral("simple.a1"));
    CHECK(candidateAt(model, 0) == QStringLiteral("A1"));
    CHECK(matchAt(model, 0) == cwTieSuggestionModel::SameName);
    CHECK(stationAt(model, 1) == QStringLiteral("simple.a2"));
    CHECK(candidateAt(model, 1) == QStringLiteral("A2"));
    CHECK(matchAt(model, 1) == cwTieSuggestionModel::SameName);

    // simple.a3 is past the end of the native trip, so it can only be guessed at.
    CHECK(stationAt(model, 2) == QStringLiteral("simple.a3"));
    CHECK(matchAt(model, 2) == cwTieSuggestionModel::SameLetterPrefix);
    CHECK(stationAt(model, 3) == QStringLiteral("simple.a3"));
    CHECK(matchAt(model, 3) == cwTieSuggestionModel::SameLetterPrefix);
}

TEST_CASE("Acting on a suggestion ends the float", "[TieSuggestionModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);
    REQUIRE(manager.floatingSurveyModel()->isFloating(setup.attached->id()));

    cwTieSuggestionModel model;
    model.setTrip(setup.attached);
    REQUIRE(model.rowCount() > 0);

    // The whole loop the feature exists for, in one call: the survey that
    // nothing joined to its cave is joined to it, and stops being reported as
    // adrift. Everything in between — which home the equate went in, the survex
    // it renders to, the run it triggers — is what the row hides from the view
    // that shows it.
    REQUIRE(model.tieAt(0));
    CHECK(setup.cave->equates()->count() == 1);

    manager.waitToFinish();
    REQUIRE_FALSE(manager.hasSolveError());
    CHECK_FALSE(manager.floatingSurveyModel()->isFloating(setup.attached->id()));
}

TEST_CASE("A row that names nothing ties nothing", "[TieSuggestionModel]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* first = addNativeTripWithShot(cave, QStringLiteral("First"),
                                          QStringLiteral("A1"), QStringLiteral("A2"));
    addNativeTripWithShot(cave, QStringLiteral("Second"),
                          QStringLiteral("A2"), QStringLiteral("A3"));

    cwTieSuggestionModel model;
    model.setTrip(first);
    REQUIRE(model.rowCount() > 0);

    // A view hands back whatever index its delegate carried, and a delegate can
    // outlive the row it was built for by a frame.
    CHECK_FALSE(model.tieAt(-1));
    CHECK_FALSE(model.tieAt(model.rowCount()));
    CHECK(cave->equates()->count() == 0);
}

TEST_CASE("More matches than a person can read are cut off and said to be",
          "[TieSuggestionModel]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* first = addNativeTripWithShot(cave, QStringLiteral("First"),
                                          QStringLiteral("A1"), QStringLiteral("A2"));
    cwTrip* second = addNativeTripWithShot(cave, QStringLiteral("Second"),
                                           QStringLiteral("A1"), QStringLiteral("A2"));
    for (int i = 3; i <= 20; ++i) {
        appendStation(first, QStringLiteral("A%1").arg(i - 1), QStringLiteral("A%1").arg(i));
        appendStation(second, QStringLiteral("A%1").arg(i - 1), QStringLiteral("A%1").arg(i));
    }

    cwTieSuggestionModel model;
    model.setTrip(first);

    // Twenty stations named alike in two trips is twenty suggestions, and a
    // banner that lists all of them is a wall the user scrolls past. Cutting the
    // list off is only safe if the cut says so, or "no more matches" and "too
    // many to show" look the same.
    CHECK(model.rowCount() == 10);
    CHECK(model.truncated());
}

TEST_CASE("Suggestions arrive with the run rather than being asked for",
          "[TieSuggestionModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    // Bound before the solve on purpose: an attachment's station names live in
    // its file, so until a run has read them there is nothing to suggest — and
    // the banner holding this is on screen from the moment the attach lands.
    cwTieSuggestionModel model;
    model.setTrip(setup.attached);
    REQUIRE(model.rowCount() == 0);

    QSignalSpy countSpy(&model, &cwTieSuggestionModel::countChanged);

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);

    CHECK(model.rowCount() == 4);
    CHECK(countSpy.size() >= 1);
}

TEST_CASE("A renamed trip is renamed in the row that offers it",
          "[TieSuggestionModel]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    cwTrip* first = addNativeTripWithShot(cave, QStringLiteral("First"),
                                          QStringLiteral("A1"), QStringLiteral("A2"));
    cwTrip* second = addNativeTripWithShot(cave, QStringLiteral("Second"),
                                           QStringLiteral("A1"), QStringLiteral("B1"));

    cwTieSuggestionModel model;
    model.setTrip(first);
    REQUIRE(model.rowCount() > 0);
    REQUIRE(candidateTripAt(model, 0) == QStringLiteral("Second"));

    // The trip name is the whole of what tells two candidate "A1"s apart, so a
    // row showing the name from before a rename points at the wrong one.
    second->setName(QStringLiteral("Renamed"));
    CHECK(candidateTripAt(model, 0) == QStringLiteral("Renamed"));
}
