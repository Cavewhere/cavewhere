//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QCoreApplication>
#include <QPointer>
#include <QSignalSpy>
#include <QUndoStack>
#include <QVector3D>

//Std includes
#include <utility>

//Our includes
#include "cwScopeStationListModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwProject.h"
#include "cwExternalCenterline.h"
#include "cwFixStationDiagnostics.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"

//Test helpers
#include "ExternalCenterlineTestHelpers.h"
#include "SurveyTreeTestHelper.h"

namespace {

QStringList roleValues(const cwScopeStationListModel& model, int role)
{
    QStringList values;
    for (int i = 0; i < model.rowCount(); ++i) {
        values.append(model.data(model.index(i, 0), role).toString());
    }
    return values;
}

QList<cwStationHandle> handles(const cwScopeStationListModel& model)
{
    QList<cwStationHandle> values;
    for (int i = 0; i < model.rowCount(); ++i) {
        values.append(model.data(model.index(i, 0),
                                 cwScopeStationListModel::StationHandleRole)
                      .value<cwStationHandle>());
    }
    return values;
}

cwShot unitShot()
{
    cwShot shot;
    shot.setDistance(cwDistanceReading("10"));
    shot.setCompass(cwCompassReading("0"));
    shot.setBackCompass(cwCompassReading("180"));
    shot.setClino(cwClinoReading("0"));
    shot.setBackClino(cwClinoReading("0"));
    return shot;
}

cwStation namedStation(const QString& name)
{
    cwStation station;
    station.setName(name);
    return station;
}

//A native trip in a source root, scoped by its label "a" in the root's block,
//with the root lookup keyed by the prefixed names the solver produces. The
//model must hand back the tails.
cwTrip* addPrefixedTrip(cwCave* cave, const QStringList& prefixedNames)
{
    cave->setSourceId(QUuid::createUuid());
    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("A"));
    cave->addTrip(trip);

    cwStationPositionLookup lookup;
    for (int i = 0; i < prefixedNames.size(); ++i) {
        lookup.setPosition(prefixedNames.at(i), QVector3D(i, i, i));
    }
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);
    return trip;
}

//A trip attached to a Survex file whose scan harvested \a harvest. The file
//never exists; the harvest stands in for the scan.
cwTrip* addHarvestedTrip(cwCave* cave, const QString& name, const QStringList& harvest)
{
    cwTrip* trip = new cwTrip();
    trip->setName(name);
    cave->addTrip(trip);
    trip->setExternalCenterline(cwExternalCenterline(
        QStringLiteral("/nonexistent/") + name + QStringLiteral(".svx")));
    trip->setExternalStations(harvest);
    return trip;
}

QStringList numberedStations(const QString& prefix, int count)
{
    QStringList names;
    for (int i = 1; i <= count; ++i) {
        names.append(prefix + QString::number(i));
    }
    return names;
}

}

TEST_CASE("A native (unprefixed) trip lists its solved chunk stations", "[Model][ScopeStations]")
{
    // A native trip has no scope of its own: its stations are bare names in
    // the cave's scope, which is what the handles must say.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = new cwTrip();
    cave->addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendShot(namedStation(QStringLiteral("s1")),
                      namedStation(QStringLiteral("s2")), unitShot());

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("s1"), QVector3D(1, 2, 3));
    lookup.setPosition(QStringLiteral("s2"), QVector3D(4, 5, 6));
    lookup.setPosition(QStringLiteral("outside"), QVector3D(7, 8, 9));
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);

    cwScopeStationListModel model;
    model.setTrip(trip);

    // Bare names, unprefixed; the foreign "outside" station is excluded.
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("s1"), QStringLiteral("s2") }));
    CHECK(handles(model)
          == QList<cwStationHandle>({
                 cwStationHandle(cwStationHandle::NativeCave, cave->id(), QStringLiteral("s1")),
                 cwStationHandle(cwStationHandle::NativeCave, cave->id(), QStringLiteral("s2"))
             }));
    CHECK(model.data(model.index(0, 0), cwScopeStationListModel::PositionRole).value<QVector3D>()
          == QVector3D(1, 2, 3));

    // Membership advisory: in-scope names hit (case-insensitively), others miss.
    CHECK(model.containsStation(QStringLiteral("s1")));
    CHECK(model.containsStation(QStringLiteral("S2")));
    CHECK_FALSE(model.containsStation(QStringLiteral("outside")));
    CHECK_FALSE(model.containsStation(QStringLiteral("nope")));

    // Pin the QML-facing role names; "stationPosition" avoids shadowing by
    // QC control roots that have their own position property.
    CHECK(model.roleNames().value(cwScopeStationListModel::StationNameRole)
          == QByteArrayLiteral("stationName"));
    CHECK(model.roleNames().value(cwScopeStationListModel::StationHandleRole)
          == QByteArrayLiteral("stationHandle"));
    CHECK(model.roleNames().value(cwScopeStationListModel::PositionRole)
          == QByteArrayLiteral("stationPosition"));
}

TEST_CASE("A native-prefixed trip lists scope-relative tails", "[Model][ScopeStations]")
{
    // Prefix "A": the cave lookup keys stations A.s1/A.s2, and the trip is the
    // scope those tails hang off — so the handles name the trip, not the cave.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = addPrefixedTrip(cave, { QStringLiteral("A.s1"), QStringLiteral("A.s2") });

    cwScopeStationListModel model;
    model.setTrip(trip);

    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("s1"), QStringLiteral("s2") }));
    CHECK(handles(model)
          == QList<cwStationHandle>({
                 cwStationHandle(cwStationHandle::Trip, trip->id(), QStringLiteral("s1")),
                 cwStationHandle(cwStationHandle::Trip, trip->id(), QStringLiteral("s2"))
             }));
}

TEST_CASE("Rows are sorted in natural order by station name", "[Model][ScopeStations]")
{
    // The lookup hands stations back in lexicographic key order ("A.a10"
    // before "A.a2"), so an unsorted model can't pass by luck. The trailing
    // number must ascend numerically: "a2" before "a10".
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = addPrefixedTrip(cave, {
        QStringLiteral("A.b1"), QStringLiteral("A.a2"), QStringLiteral("A.a1"),
        QStringLiteral("A.c3"), QStringLiteral("A.a10"), QStringLiteral("A.b2"),
        QStringLiteral("A.aa1")
    });

    cwScopeStationListModel model;
    model.setTrip(trip);

    REQUIRE(model.rowCount() == 7);
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("a1"),
                           QStringLiteral("a2"),
                           QStringLiteral("a10"),
                           QStringLiteral("aa1"),
                           QStringLiteral("b1"),
                           QStringLiteral("b2"),
                           QStringLiteral("c3") }));
}

TEST_CASE("matchingStations filters in-scope names by typed prefix", "[Model][ScopeStations]")
{
    // A tail is everything below the trip's scope, so it can itself be dotted
    // when the scoped survey nests further ("sidepassage.b1").
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = addPrefixedTrip(cave, {
        QStringLiteral("A.a1"), QStringLiteral("A.a2"),
        QStringLiteral("A.sidepassage.b1")
    });

    cwScopeStationListModel model;
    model.setTrip(trip);

    // Empty prefix returns every in-scope name, in row order.
    CHECK(model.matchingStations(QString())
          == QStringList({ QStringLiteral("a1"),
                           QStringLiteral("a2"),
                           QStringLiteral("sidepassage.b1") }));

    // Prefix filters case-insensitively.
    CHECK(model.matchingStations(QStringLiteral("a"))
          == QStringList({ QStringLiteral("a1"), QStringLiteral("a2") }));
    CHECK(model.matchingStations(QStringLiteral("A2"))
          == QStringList({ QStringLiteral("a2") }));
    CHECK(model.matchingStations(QStringLiteral("side"))
          == QStringList({ QStringLiteral("sidepassage.b1") }));

    // No match yields an empty list (drives the out-of-scope advisory).
    CHECK(model.matchingStations(QStringLiteral("z")).isEmpty());

    // A trip matches from the start of the tail only; matching any dotted
    // part belongs to node scope.
    CHECK(model.matchingStations(QStringLiteral("b1")).isEmpty());

    // The dotted tail travels whole in the handle — a station is named by its
    // scope plus that tail, never by a flattened qualified string.
    CHECK(handles(model).last()
          == cwStationHandle(cwStationHandle::Trip, trip->id(),
                             QStringLiteral("sidepassage.b1")));
}

TEST_CASE("Clearing the trip empties the model", "[Model][ScopeStations]")
{
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = addPrefixedTrip(cave, { QStringLiteral("A.s1") });

    cwScopeStationListModel model;
    CHECK(model.rowCount() == 0);

    model.setTrip(trip);
    CHECK(model.rowCount() == 1);

    model.setTrip(nullptr);
    CHECK(model.rowCount() == 0);

    // Clearing drops the subscription with the trip, so a later solve in the
    // cave it used to watch cannot reach the model.
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    cwStationPositionLookup grown = cave->scopedPositions();
    grown.setPosition(QStringLiteral("A.s2"), QVector3D(4, 5, 6));
    SurveyTreeTestHelper::setScopedPositions(cave, grown);

    CHECK(resetSpy.count() == 0);
}

TEST_CASE("A re-solve re-pulls the trip's solved stations", "[Model][ScopeStations]")
{
    // Setting the trip is the whole contract: the model subscribes to that
    // trip's solvedStationsChanged, which its cave relays when the solve lands
    // in the region's lookup — so the rows refresh with no help from the caller.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = addPrefixedTrip(cave, { QStringLiteral("A.s1") });

    cwScopeStationListModel model;
    model.setTrip(trip);
    REQUIRE(model.rowCount() == 1);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    // The solve advances: a second station appears in the cave lookup.
    cwStationPositionLookup second = cave->scopedPositions();
    second.setPosition(QStringLiteral("A.s2"), QVector3D(4, 5, 6));
    SurveyTreeTestHelper::setScopedPositions(cave, second);

    CHECK(resetSpy.count() == 1);
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("s1"), QStringLiteral("s2") }));
}

TEST_CASE("A positions-only re-solve refreshes the rows", "[Model][ScopeStations]")
{
    // Re-surveying a shot moves stations without changing which stations exist
    // or what they connect to. The region network compares equal across such a
    // solve, so a network-change pulse would never fire and the rows would keep
    // stale positions. The region's lookup is the pulse, so this lands.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = addPrefixedTrip(cave, { QStringLiteral("A.s1") });

    cwScopeStationListModel model;
    model.setTrip(trip);
    REQUIRE(model.data(model.index(0, 0), cwScopeStationListModel::PositionRole)
            .value<QVector3D>() == QVector3D(0, 0, 0));

    cwStationPositionLookup moved = cave->scopedPositions();
    moved.setPosition(QStringLiteral("A.s1"), QVector3D(9, 9, 9));
    SurveyTreeTestHelper::setScopedPositions(cave, moved);

    CHECK(model.data(model.index(0, 0), cwScopeStationListModel::PositionRole)
          .value<QVector3D>() == QVector3D(9, 9, 9));
}

TEST_CASE("A foreign region's solve leaves the model alone", "[Model][ScopeStations]")
{
    // Two projects solve independently. A region pulses only the trips its tree
    // lists, so a solve in one cannot reach a model watching a trip in the other.
    cwCavingRegion firstRegion;
    cwCave* first = new cwCave();
    first->setName(QStringLiteral("First"));
    firstRegion.addCave(first);
    cwTrip* firstTrip = addPrefixedTrip(first, { QStringLiteral("A.s1") });

    cwCavingRegion secondRegion;
    cwCave* second = new cwCave();
    second->setName(QStringLiteral("Second"));
    secondRegion.addCave(second);
    addPrefixedTrip(second, { QStringLiteral("A.s9") });

    cwScopeStationListModel model;
    model.setTrip(firstTrip);
    REQUIRE(roleValues(model, cwScopeStationListModel::StationNameRole)
            == QStringList({ QStringLiteral("s1") }));

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    cwStationPositionLookup elsewhere = second->scopedPositions();
    elsewhere.setPosition(QStringLiteral("A.s8"), QVector3D(1, 1, 1));
    SurveyTreeTestHelper::setScopedPositions(second, elsewhere);

    CHECK(resetSpy.count() == 0);
}

TEST_CASE("A station the solve never placed is still a row", "[Model][ScopeStations]")
{
    // The follow-up this model was blind to: an attachment cavern dropped has
    // no solved station at all, so a model reading only the solve listed
    // nothing — a dozen lines from a banner naming those very stations. A
    // person referencing one from a note needs the name then, not after the
    // tie-in that places it.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = new cwTrip();
    trip->setName(QStringLiteral("Topo 1"));
    cave->addTrip(trip);
    trip->setExternalCenterline(cwExternalCenterline(QStringLiteral("/tmp/cave.svx")));

    cwScopeStationListModel model;
    model.setTrip(trip);
    REQUIRE(model.rowCount() == 0);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    trip->setExternalStations({ QStringLiteral("h1"), QStringLiteral("h2") });

    CHECK(resetSpy.count() == 1);
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("h1"), QStringLiteral("h2") }));

    // Undefined, not the origin — a known station that is nowhere yet must not
    // read as one placed at 0,0,0.
    CHECK_FALSE(model.data(model.index(0, 0),
                           cwScopeStationListModel::PositionRole).isValid());

    // And the solve placing one fills that row's position in without adding a
    // second row for the same station.
    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("topo_1.h1"), QVector3D(1, 2, 3));
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);

    CHECK(model.rowCount() == 2);
    CHECK(model.data(model.index(0, 0), cwScopeStationListModel::PositionRole)
          .value<QVector3D>() == QVector3D(1, 2, 3));
}

TEST_CASE("A cave pulses solvedStationsChanged only on the trips it lists",
          "[Trip][ScopeStations]")
{
    // The forwarding the model rides on: the trip owns solvedStations() but the
    // region owns the lookup it reads, and the cave relays the region's pulse
    // to its trips. A removed trip is cut off, the same way
    // cwSurveyNode::disconnectTrip already cuts it off from scopeChanged.
    //
    // The undo stack is what keeps the removed trip alive here — without one,
    // pushUndo redoes and deletes the command, which owns the trip it removed.
    QUndoStack undoStack;
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cave->setUndoStack(&undoStack);

    cwTrip* trip = addPrefixedTrip(cave, { QStringLiteral("A.s1") });
    QPointer<cwTrip> tripGuard(trip);

    const auto advanceSolve = [cave](const QString& station) {
        cwStationPositionLookup grown = cave->scopedPositions();
        grown.setPosition(station, QVector3D(4, 5, 6));
        SurveyTreeTestHelper::setScopedPositions(cave, grown);
    };

    QSignalSpy pulseSpy(trip, &cwTrip::solvedStationsChanged);

    advanceSolve(QStringLiteral("A.s2"));
    CHECK(pulseSpy.count() == 1);

    cave->removeTrip(0);
    REQUIRE_FALSE(tripGuard.isNull());

    advanceSolve(QStringLiteral("A.s3"));
    CHECK(pulseSpy.count() == 1);
}

TEST_CASE("Node scope lists a native trip's stations under its scopePrefix",
          "[Model][ScopeStations]")
{
    // A flat native trip has no scope of its own, so its stations are the bare
    // names a fix on the cave takes, and they are listed before any solve.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = SurveyTreeTestHelper::addTrip(cave, QStringLiteral("Trip 1"), QStringLiteral("a"));
    REQUIRE(trip->scopePrefix().isEmpty());

    cwScopeStationListModel model;
    model.setNode(cave);

    REQUIRE(roleValues(model, cwScopeStationListModel::StationNameRole)
            == QStringList({ QStringLiteral("a1"), QStringLiteral("a2") }));
    CHECK(handles(model)
          == QList<cwStationHandle>({
                 cwStationHandle(cwStationHandle::NativeCave, cave->id(), QStringLiteral("a1")),
                 cwStationHandle(cwStationHandle::NativeCave, cave->id(), QStringLiteral("a2"))
             }));

    // Once solved, every listed name is one the station diagnostic accepts.
    // The cave-prefixed spelling is not.
    SurveyTreeTestHelper::surveyStations(cave, { QStringLiteral("a1"), QStringLiteral("a2") });
    const cwSurveyNetwork network = cave->scopedNetwork();
    REQUIRE_FALSE(network.isEmpty());
    for (const QString& name : roleValues(model, cwScopeStationListModel::StationNameRole)) {
        INFO(name.toStdString());
        CHECK(cwFixStationDiagnostics::classifyStationReference(
                  name, network, cwFixStationDiagnostics::SurveyData::Stations)
              == cwFixStationDiagnostics::StationReference::Ok);
    }
    CHECK(cwFixStationDiagnostics::classifyStationReference(
              cave->scopePrefix() + QStringLiteral("a1"), network,
              cwFixStationDiagnostics::SurveyData::Stations)
          == cwFixStationDiagnostics::StationReference::Unknown);
}

TEST_CASE("Node scope lists an attached trip's harvest under the trip's label",
          "[Model][ScopeStations]")
{
    // The name a fix takes for a station of a trip's attached file is the
    // trip's label plus the file's name, and the harvest gives it before any
    // solve.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = addHarvestedTrip(cave, QStringLiteral("trip1"), { QStringLiteral("entrance") });
    REQUIRE(trip->scopePrefix() == QStringLiteral("trip1."));

    cwScopeStationListModel model;
    model.setNode(cave);

    REQUIRE(roleValues(model, cwScopeStationListModel::StationNameRole)
            == QStringList({ QStringLiteral("trip1.entrance") }));
    CHECK(handles(model)
          == QList<cwStationHandle>({
                 cwStationHandle(cwStationHandle::Trip, trip->id(), QStringLiteral("entrance"))
             }));
    CHECK_FALSE(model.data(model.index(0, 0), cwScopeStationListModel::PositionRole).isValid());

    CHECK(model.containsStation(QStringLiteral("TRIP1.Entrance")));
    CHECK_FALSE(model.containsStation(QStringLiteral("entrance")));
    CHECK(model.matchingStations(QStringLiteral("trip1.e"))
          == QStringList({ QStringLiteral("trip1.entrance") }));
}

TEST_CASE("Node scope lists the node's own attachment unscoped",
          "[Model][ScopeStations]")
{
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("/nonexistent/hillside.svx")));
    cave->setExternalStations({ QStringLiteral("hillside.h1") });

    cwScopeStationListModel model;
    model.setNode(cave);

    REQUIRE(roleValues(model, cwScopeStationListModel::StationNameRole)
            == QStringList({ QStringLiteral("hillside.h1") }));
    // No single trip owns a station of the node's own file.
    CHECK_FALSE(handles(model).first().isValid());
    CHECK_FALSE(model.data(model.index(0, 0), cwScopeStationListModel::PositionRole).isValid());

    // A rescan of the node's own file reaches the rows once the event loop turns.
    cave->setExternalStations({ QStringLiteral("hillside.h1"), QStringLiteral("hillside.h2") });
    QCoreApplication::processEvents();

    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("hillside.h1"), QStringLiteral("hillside.h2") }));
}

TEST_CASE("Node scope lists trips in node order, then the node's own attachment",
          "[Model][ScopeStations]")
{
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("/nonexistent/hillside.svx")));
    cave->setExternalStations({ QStringLiteral("hillside.h1") });
    SurveyTreeTestHelper::addTrip(cave, QStringLiteral("Trip 1"), QStringLiteral("b"));
    addHarvestedTrip(cave, QStringLiteral("trip2"), { QStringLiteral("e2"), QStringLiteral("e10") });
    // The same station named twice is one row.
    SurveyTreeTestHelper::addTrip(cave, QStringLiteral("Trip 3"), QStringLiteral("B"));

    cwScopeStationListModel model;
    model.setNode(cave);

    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("b1"), QStringLiteral("b2"),
                           QStringLiteral("trip2.e2"), QStringLiteral("trip2.e10"),
                           QStringLiteral("hillside.h1") }));
}

TEST_CASE("Node scope matches a typed name at the start of any dotted part",
          "[Model][ScopeStations]")
{
    // The trip label appears nowhere in the UI, so a person types the name the
    // file gives the station and still finds the scoped name a fix takes.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    addHarvestedTrip(cave, QStringLiteral("trip1"),
                     { QStringLiteral("entrance"), QStringLiteral("sump") });

    cwScopeStationListModel model;
    model.setNode(cave);
    REQUIRE(roleValues(model, cwScopeStationListModel::StationNameRole)
            == QStringList({ QStringLiteral("trip1.entrance"), QStringLiteral("trip1.sump") }));

    CHECK(model.matchingStations(QStringLiteral("ent"))
          == QStringList({ QStringLiteral("trip1.entrance") }));
    CHECK(model.matchingStations(QStringLiteral("SU"))
          == QStringList({ QStringLiteral("trip1.sump") }));
    // A part matches from its start only.
    CHECK(model.matchingStations(QStringLiteral("rance")).isEmpty());
}

TEST_CASE("Node scope lists whole-name matches before part matches",
          "[Model][ScopeStations]")
{
    // Rows run trips first, then the node's own attachment; a name that
    // matches from its start still leads the list.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    addHarvestedTrip(cave, QStringLiteral("trip1"), { QStringLiteral("entrance") });
    cave->setExternalCenterline(cwExternalCenterline(QStringLiteral("/nonexistent/hillside.svx")));
    cave->setExternalStations({ QStringLiteral("entry") });

    cwScopeStationListModel model;
    model.setNode(cave);
    REQUIRE(roleValues(model, cwScopeStationListModel::StationNameRole)
            == QStringList({ QStringLiteral("trip1.entrance"), QStringLiteral("entry") }));

    CHECK(model.matchingStations(QStringLiteral("ent"))
          == QStringList({ QStringLiteral("entry"), QStringLiteral("trip1.entrance") }));
}

TEST_CASE("Node scope leaves out a station the attached file fixes itself",
          "[Model][ScopeStations]")
{
    // The exporter drops a node fix on a station its file already fixes, so
    // listing it would offer a name that cannot take a fix.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = addHarvestedTrip(cave, QStringLiteral("trip1"),
                                   { QStringLiteral("entrance"), QStringLiteral("sink") });
    cave->setFileFixedStations({ { trip->id(), { QStringLiteral("entrance") } } });

    cwScopeStationListModel model;
    model.setNode(cave);

    REQUIRE(roleValues(model, cwScopeStationListModel::StationNameRole)
            == QStringList({ QStringLiteral("trip1.sink") }));

    // A rescan that finds the file fixing the other station too drops it.
    cave->setFileFixedStations({ { trip->id(), { QStringLiteral("entrance"), QStringLiteral("sink") } } });
    QCoreApplication::processEvents();

    CHECK(model.rowCount() == 0);
}

TEST_CASE("Node scope coalesces a burst of pulses into one reset",
          "[Model][ScopeStations]")
{
    // Two harvests and a solve land in one turn; the model rebuilds once,
    // after the event loop turns.
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* first = addHarvestedTrip(cave, QStringLiteral("trip1"), { QStringLiteral("e1") });
    cwTrip* second = addHarvestedTrip(cave, QStringLiteral("trip2"), {});

    cwScopeStationListModel model;
    model.setNode(cave);
    REQUIRE(roleValues(model, cwScopeStationListModel::StationNameRole)
            == QStringList({ QStringLiteral("trip1.e1") }));

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    first->setExternalStations({ QStringLiteral("e1"), QStringLiteral("e2") });
    second->setExternalStations({ QStringLiteral("f1") });
    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("trip1.e1"), QVector3D(1, 2, 3));
    SurveyTreeTestHelper::setScopedPositions(cave, lookup);

    CHECK(resetSpy.count() == 0);

    QCoreApplication::processEvents();

    CHECK(resetSpy.count() == 1);
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("trip1.e1"), QStringLiteral("trip1.e2"),
                           QStringLiteral("trip2.f1") }));
    CHECK(model.data(model.index(0, 0), cwScopeStationListModel::PositionRole).value<QVector3D>()
          == QVector3D(1, 2, 3));
}

TEST_CASE("Node scope costs one reset per burst at four times the size",
          "[Model][ScopeStations]")
{
    // Every trip pulses on a solve and on a harvest. A 4x larger node must
    // still cost one reset per burst, never one per trip.
    constexpr int kStationsPerTrip = 5;
    constexpr int kSmallTripCount = 4;
    constexpr int kSizeFactor = 4;

    const auto resetsPerBurst = [](int tripCount) {
        cwCavingRegion region;
        cwCave* cave = new cwCave();
        cave->setName(QStringLiteral("Cave"));
        region.addCave(cave);
        QList<cwTrip*> trips;
        for (int i = 1; i <= tripCount; ++i) {
            trips.append(addHarvestedTrip(cave, QStringLiteral("t") + QString::number(i),
                                         numberedStations(QStringLiteral("s"), kStationsPerTrip)));
        }

        cwScopeStationListModel model;
        model.setNode(cave);
        CHECK(model.rowCount() == tripCount * kStationsPerTrip);

        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

        cwStationPositionLookup lookup;
        lookup.setPosition(QStringLiteral("t1.s1"), QVector3D(1, 1, 1));
        SurveyTreeTestHelper::setScopedPositions(cave, lookup);
        QCoreApplication::processEvents();
        const int solveResets = resetSpy.count();

        for (cwTrip* trip : std::as_const(trips)) {
            trip->setExternalStations(numberedStations(QStringLiteral("s"), kStationsPerTrip + 1));
        }
        QCoreApplication::processEvents();
        const int harvestResets = resetSpy.count() - solveResets;

        CHECK(model.rowCount() == tripCount * (kStationsPerTrip + 1));
        return std::make_pair(solveResets, harvestResets);
    };

    const auto small = resetsPerBurst(kSmallTripCount);
    const auto large = resetsPerBurst(kSmallTripCount * kSizeFactor);

    CHECK(small.first == 1);
    CHECK(small.second == 1);
    CHECK(large.first == 1);
    CHECK(large.second == 1);
}

TEST_CASE("Setting one scope clears the other", "[Model][ScopeStations]")
{
    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    region.addCave(cave);
    cwTrip* trip = SurveyTreeTestHelper::addTrip(cave, QStringLiteral("Trip 1"), QStringLiteral("a"));

    cwScopeStationListModel model;
    model.setTrip(trip);
    QSignalSpy tripSpy(&model, &cwScopeStationListModel::tripChanged);
    QSignalSpy nodeSpy(&model, &cwScopeStationListModel::nodeChanged);

    model.setNode(cave);
    CHECK(model.node() == cave);
    CHECK(model.trip() == nullptr);
    CHECK(tripSpy.count() == 1);
    CHECK(nodeSpy.count() == 1);

    model.setTrip(trip);
    CHECK(model.trip() == trip);
    CHECK(model.node() == nullptr);
    CHECK(tripSpy.count() == 2);
    CHECK(nodeSpy.count() == 2);
}

TEST_CASE("Node scope announces a destroyed node", "[Model][ScopeStations]")
{
    auto* cave = new cwCave();
    cave->setName(QStringLiteral("Cave"));
    SurveyTreeTestHelper::addTrip(cave, QStringLiteral("Trip 1"), QStringLiteral("a"));

    cwScopeStationListModel model;
    model.setNode(cave);
    REQUIRE(model.rowCount() == 2);
    QSignalSpy nodeSpy(&model, &cwScopeStationListModel::nodeChanged);

    delete cave;
    CHECK(model.node() == nullptr);
    CHECK(nodeSpy.count() == 1);

    QCoreApplication::processEvents();
    CHECK(model.rowCount() == 0);
}

TEST_CASE("Node scope on a source root lists native trips across its subtree",
          "[Model][ScopeStations]")
{
    // A source root's fixes reach every native trip below it, each under the
    // label the root's block gives it, as writeFixStations accepts them.
    cwCavingRegion region;
    cwCave* root = new cwCave();
    root->setName(QStringLiteral("Root"));
    region.addCave(root);
    root->setSourceId(QUuid::createUuid());
    root->setSourcePath(QString());
    REQUIRE(root->isSourceRoot());

    cwCave* side = new cwCave();
    side->setName(QStringLiteral("Side"));
    root->addNode(side);
    cwTrip* trip = SurveyTreeTestHelper::addTrip(side, QStringLiteral("Lower"), QStringLiteral("a"));
    const QString prefix = trip->scopePrefix();
    REQUIRE_FALSE(prefix.isEmpty());

    cwScopeStationListModel model;
    model.setNode(root);

    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ prefix + QStringLiteral("a1"), prefix + QStringLiteral("a2") }));

    // A native trip added further down reaches the rows once the event loop turns.
    cwTrip* another = SurveyTreeTestHelper::addTrip(side, QStringLiteral("Upper"), QStringLiteral("c"));
    QCoreApplication::processEvents();

    CHECK(model.containsStation(another->scopePrefix() + QStringLiteral("c1")));
}
