/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Commit 6b of plans/EXTERNAL_FILE_EQUATES_AND_SCOPING.html: the rows the
// floating-survey banner binds to.
//
// The model's whole job is turning a finished run's answer — uuids, and the
// station spelling the exporter used — into something a person reads, against
// a region that keeps being edited while the answer stands still. So these
// pin the two ways those disagree: a name that moved after the run, and a trip
// the region dropped after the run.

// Catch
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwFindFloatingSurveys.h"
#include "cwFloatingSurveyModel.h"
#include "cwLinePlotManager.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"

// Qt
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>

namespace {

QString stringAt(const cwFloatingSurveyModel* model, int row, int role)
{
    return roleAt(model, row, role).toString();
}

QStringList stationsAt(const cwFloatingSurveyModel* model, int row)
{
    return roleAt(model, row, cwFloatingSurveyModel::StationsRole).toStringList();
}

cwFloatingSurveyModel::Trigger triggerAt(const cwFloatingSurveyModel* model, int row)
{
    const QVariant trigger = roleAt(model, row, cwFloatingSurveyModel::TriggerRole);
    //UnconnectedChunks is 0, and so is what an unhandled role converts to — so
    //without this the comparison against it passes on a role data() never
    //served.
    REQUIRE(trigger.isValid());
    return trigger.value<cwFloatingSurveyModel::Trigger>();
}

} // namespace

TEST_CASE("An attached centerline that floats becomes one readable row",
          "[FloatingSurveyModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    const cwFloatingSurveyModel* model = manager.floatingSurveyModel();
    REQUIRE(model != nullptr);
    QSignalSpy countSpy(model, &cwFloatingSurveyModel::countChanged);

    solveRegion(manager, region, setup.tripDirs);

    INFO("solve error: " << manager.solveErrorMessage().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());
    REQUIRE(model->rowCount() == 1);
    CHECK(countSpy.size() == 1);

    CHECK(stringAt(model, 0, cwFloatingSurveyModel::CaveNameRole) == setup.cave->name());
    CHECK(stringAt(model, 0, cwFloatingSurveyModel::TripNameRole) == setup.attached->name());
    CHECK(roleAt(model, 0, cwFloatingSurveyModel::CaveIdRole).toUuid() == setup.cave->id());
    CHECK(roleAt(model, 0, cwFloatingSurveyModel::TripIdRole).toUuid() == setup.attached->id());
    CHECK(triggerAt(model, 0) == cwFloatingSurveyModel::ExternalScope);

    // The trip's own namespace, not the cave-local spelling the record carries:
    // the surfaces that show these rows are the editor and the trip page, where
    // every other station is written the same way.
    const QStringList tripLocal({QStringLiteral("simple.a1"),
                                 QStringLiteral("simple.a2"),
                                 QStringLiteral("simple.a3")});
    CHECK(stationsAt(model, 0) == tripLocal);

    // The per-trip surfaces ask through these two rather than the rows, so they
    // have to agree with them — and with each other.
    CHECK(model->isFloating(setup.attached->id()));
    CHECK(model->floatingStations(setup.attached->id()) == tripLocal);
    CHECK_FALSE(model->isFloating(setup.cave->trip(0)->id()));
    CHECK(model->floatingStations(setup.cave->trip(0)->id()).isEmpty());
}

TEST_CASE("A native floating chunk becomes a row named the same way",
          "[FloatingSurveyModel]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
    addNativeTripWithShot(cave, QStringLiteral("Main"),
                          QStringLiteral("A1"), QStringLiteral("A2"));
    cwTrip* stray = addNativeTripWithShot(cave, QStringLiteral("Stray"),
                                          QStringLiteral("Z1"), QStringLiteral("Z2"));

    cwLinePlotManager manager;
    solveRegion(manager, region, {});

    // The other trigger, reached through the same rows — a consumer binding to
    // this model never learns which pass answered.
    const cwFloatingSurveyModel* model = manager.floatingSurveyModel();
    REQUIRE(model->rowCount() == 1);
    CHECK(stringAt(model, 0, cwFloatingSurveyModel::TripNameRole) == stray->name());
    CHECK(triggerAt(model, 0) == cwFloatingSurveyModel::UnconnectedChunks);
    CHECK(stationsAt(model, 0) == QStringList({QStringLiteral("z1"),
                                               QStringLiteral("z2")}));
}

TEST_CASE("Renaming a cave re-renders its floating rows before the re-solve lands",
          "[FloatingSurveyModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);

    const cwFloatingSurveyModel* model = manager.floatingSurveyModel();
    REQUIRE(model->rowCount() == 1);

    // A rename queues a solve, and asserting before waiting for it is the
    // point: the row is a view of the live region, so it must not wait on a
    // whole cavern run to stop showing a name the user has already replaced.
    setup.cave->setName(QStringLiteral("Bravo"));
    CHECK(stringAt(model, 0, cwFloatingSurveyModel::CaveNameRole)
          == QStringLiteral("Bravo"));

    manager.waitToFinish();
    REQUIRE(model->rowCount() == 1);
    CHECK(stringAt(model, 0, cwFloatingSurveyModel::CaveNameRole)
          == QStringLiteral("Bravo"));
}

TEST_CASE("A record naming a trip the region dropped produces no row",
          "[FloatingSurveyModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);

    const cwFloatingSurveyModel* model = manager.floatingSurveyModel();
    REQUIRE(model->rowCount() == 1);
    QSignalSpy countSpy(model, &cwFloatingSurveyModel::countChanged);

    // The answer always describes a finished run, and a run that stopped early
    // carries its records forward — so a record can outlive the trip it names.
    // Resolving against the live region is what keeps that out of the banner.
    setup.cave->removeTrip(setup.cave->tripCount() - 1);
    CHECK(model->rowCount() == 0);
    CHECK(countSpy.size() == 1);

    manager.waitToFinish();
    CHECK(model->rowCount() == 0);
}

TEST_CASE("A survey cavern dropped outright floats with no station to name",
          "[FloatingSurveyModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_hanging.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);

    // Nothing fixes it and nothing ties it in, so cavern drops it and no
    // station of its is ever placed. This is the worst failure the banner has
    // to speak for, and it is the one where the station list is empty — so a
    // surface that keyed off "are there stations" would go silent for exactly
    // it. The row, and isFloating(), must stand on their own.
    const cwFloatingSurveyModel* model = manager.floatingSurveyModel();
    REQUIRE_FALSE(manager.hasSolveError());
    REQUIRE(model->rowCount() == 1);
    CHECK(stringAt(model, 0, cwFloatingSurveyModel::TripNameRole) == setup.attached->name());
    CHECK(triggerAt(model, 0) == cwFloatingSurveyModel::ExternalScope);
    CHECK(stationsAt(model, 0).isEmpty());

    CHECK(model->isFloating(setup.attached->id()));
    CHECK(model->floatingStations(setup.attached->id()).isEmpty());
}

TEST_CASE("A station the run named under an old scope keeps the spelling it was given",
          "[FloatingSurveyModel]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const AttachedSetup setup =
        setupNativeAndAttached(tempRoot, region, QStringLiteral("survex_simple.svx"));

    cwLinePlotManager manager;
    solveRegion(manager, region, setup.tripDirs);

    const cwFloatingSurveyModel* model = manager.floatingSurveyModel();
    REQUIRE(model->rowCount() == 1);

    // The record spells its stations under the scope the run used. Renaming the
    // trip moves the scope out from under them, so the prefix no longer matches
    // and there is nothing to strip — the row keeps what the run recorded
    // rather than slicing the name to a wrong length.
    const QString recordedScope = setup.attached->scopePrefix().toLower();
    setup.attached->setName(QStringLiteral("Renamed"));

    CHECK(stationsAt(model, 0)
          == QStringList({recordedScope + QStringLiteral("simple.a1"),
                          recordedScope + QStringLiteral("simple.a2"),
                          recordedScope + QStringLiteral("simple.a3")}));
    CHECK(stringAt(model, 0, cwFloatingSurveyModel::TripNameRole)
          == QStringLiteral("Renamed"));
}

TEST_CASE("Rows do not outlive the region that gives them their names",
          "[FloatingSurveyModel]")
{
    cwFloatingSurveyModel model;
    QUuid tripId;

    {
        cwCavingRegion region;
        cwCave* cave = addEmptyCave(region, QStringLiteral("Alpha"));
        const cwTrip* stray = addNativeTripWithShot(cave, QStringLiteral("Stray"),
                                                    QStringLiteral("Z1"),
                                                    QStringLiteral("Z2"));
        tripId = stray->id();

        model.setRegion(&region);
        model.setResults({cwFindFloatingSurveys::Result{
            cave->id(),
            tripId,
            cwFindFloatingSurveys::Result::Trigger::UnconnectedChunks,
            QStringList({QStringLiteral("z1"), QStringLiteral("z2")})}});

        REQUIRE(model.rowCount() == 1);
        REQUIRE(model.isFloating(tripId));
    }

    // The region is gone, so every name in every row named something that no
    // longer exists. A QPointer going null is not enough on its own — the rows
    // are what a surface reads, and nothing else would ever clear them.
    CHECK(model.rowCount() == 0);
    CHECK_FALSE(model.isFloating(tripId));
}
