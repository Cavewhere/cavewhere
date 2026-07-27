//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QPointer>
#include <QSignalSpy>
#include <QUndoStack>
#include <QVector3D>

//Our includes
#include "cwScopeStationListModel.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"

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

//A trip scoped by a station prefix, with the cave lookup keyed by the
//prefixed names the solver produces. The model must hand back the tails.
cwTrip* addPrefixedTrip(cwCave* cave, const QStringList& prefixedNames)
{
    cwTrip* trip = new cwTrip();
    trip->setStationPrefix(QStringLiteral("A"));
    cave->addTrip(trip);

    cwStationPositionLookup lookup;
    for (int i = 0; i < prefixedNames.size(); ++i) {
        lookup.setPosition(prefixedNames.at(i), QVector3D(i, i, i));
    }
    cave->setStationPositionLookup(lookup);
    return trip;
}

}

TEST_CASE("A native (unprefixed) trip lists its solved chunk stations", "[Model][ScopeStations]")
{
    // A native trip has no scope of its own: its stations are bare names in
    // the cave's scope, which is what the handles must say.
    cwCave cave;
    cwTrip* trip = new cwTrip();
    cave.addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    chunk->appendShot(namedStation(QStringLiteral("s1")),
                      namedStation(QStringLiteral("s2")), unitShot());

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("s1"), QVector3D(1, 2, 3));
    lookup.setPosition(QStringLiteral("s2"), QVector3D(4, 5, 6));
    lookup.setPosition(QStringLiteral("outside"), QVector3D(7, 8, 9));
    cave.setStationPositionLookup(lookup);

    cwScopeStationListModel model;
    model.setTrip(trip);

    // Bare names, unprefixed; the foreign "outside" station is excluded.
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("s1"), QStringLiteral("s2") }));
    CHECK(handles(model)
          == QList<cwStationHandle>({
                 cwStationHandle(cwStationHandle::NativeCave, cave.id(), QStringLiteral("s1")),
                 cwStationHandle(cwStationHandle::NativeCave, cave.id(), QStringLiteral("s2"))
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
    cwCave cave;
    cwTrip* trip = addPrefixedTrip(&cave, { QStringLiteral("A.s1"), QStringLiteral("A.s2") });

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
    cwCave cave;
    cwTrip* trip = addPrefixedTrip(&cave, {
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
    cwCave cave;
    cwTrip* trip = addPrefixedTrip(&cave, {
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

    // The dotted tail travels whole in the handle — a station is named by its
    // scope plus that tail, never by a flattened qualified string.
    CHECK(handles(model).last()
          == cwStationHandle(cwStationHandle::Trip, trip->id(),
                             QStringLiteral("sidepassage.b1")));
}

TEST_CASE("Clearing the trip empties the model", "[Model][ScopeStations]")
{
    cwCave cave;
    cwTrip* trip = addPrefixedTrip(&cave, { QStringLiteral("A.s1") });

    cwScopeStationListModel model;
    CHECK(model.rowCount() == 0);

    model.setTrip(trip);
    CHECK(model.rowCount() == 1);

    model.setTrip(nullptr);
    CHECK(model.rowCount() == 0);

    // Clearing drops the subscription with the trip, so a later solve in the
    // cave it used to watch cannot reach the model.
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    cwStationPositionLookup grown = cave.stationPositionLookup();
    grown.setPosition(QStringLiteral("A.s2"), QVector3D(4, 5, 6));
    cave.setStationPositionLookup(grown);

    CHECK(resetSpy.count() == 0);
}

TEST_CASE("A re-solve re-pulls the trip's solved stations", "[Model][ScopeStations]")
{
    // Setting the trip is the whole contract: the model subscribes to that
    // trip's solvedStationsChanged, which its cave pulses when the solve lands
    // in the lookup — so the rows refresh with no help from the caller.
    cwCave cave;
    cwTrip* trip = addPrefixedTrip(&cave, { QStringLiteral("A.s1") });

    cwScopeStationListModel model;
    model.setTrip(trip);
    REQUIRE(model.rowCount() == 1);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    // The solve advances: a second station appears in the cave lookup.
    cwStationPositionLookup second = cave.stationPositionLookup();
    second.setPosition(QStringLiteral("A.s2"), QVector3D(4, 5, 6));
    cave.setStationPositionLookup(second);

    CHECK(resetSpy.count() == 1);
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("s1"), QStringLiteral("s2") }));
}

TEST_CASE("A positions-only re-solve refreshes the rows", "[Model][ScopeStations]")
{
    // Re-surveying a shot moves stations without changing which stations exist
    // or what they connect to. The region network compares equal across such a
    // solve, so a network-change pulse would never fire and the rows would keep
    // stale positions. The cave's lookup is the pulse, so this lands.
    cwCave cave;
    cwTrip* trip = addPrefixedTrip(&cave, { QStringLiteral("A.s1") });

    cwScopeStationListModel model;
    model.setTrip(trip);
    REQUIRE(model.data(model.index(0, 0), cwScopeStationListModel::PositionRole)
            .value<QVector3D>() == QVector3D(0, 0, 0));

    cwStationPositionLookup moved = cave.stationPositionLookup();
    moved.setPosition(QStringLiteral("A.s1"), QVector3D(9, 9, 9));
    cave.setStationPositionLookup(moved);

    CHECK(model.data(model.index(0, 0), cwScopeStationListModel::PositionRole)
          .value<QVector3D>() == QVector3D(9, 9, 9));
}

TEST_CASE("A foreign cave's solve leaves the model alone", "[Model][ScopeStations]")
{
    // Two caves solve independently. Because the cave pulses only the trips it
    // lists, a solve in one cannot reach a model watching a trip in the other.
    cwCave first;
    cwTrip* firstTrip = addPrefixedTrip(&first, { QStringLiteral("A.s1") });

    cwCave second;
    addPrefixedTrip(&second, { QStringLiteral("A.s9") });

    cwScopeStationListModel model;
    model.setTrip(firstTrip);
    REQUIRE(roleValues(model, cwScopeStationListModel::StationNameRole)
            == QStringList({ QStringLiteral("s1") }));

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    cwStationPositionLookup elsewhere = second.stationPositionLookup();
    elsewhere.setPosition(QStringLiteral("A.s8"), QVector3D(1, 1, 1));
    second.setStationPositionLookup(elsewhere);

    CHECK(resetSpy.count() == 0);
}

TEST_CASE("A cave pulses solvedStationsChanged only on the trips it lists",
          "[Trip][ScopeStations]")
{
    // The forwarding the model rides on: the trip owns solvedStations() but the
    // cave owns the lookup it reads, so the cave is what says the answer moved.
    // A removed trip is cut off, the same way cwCave::disconnectTrip already
    // cuts it off from scopeChanged.
    //
    // The undo stack is what keeps the removed trip alive here — without one,
    // pushUndo redoes and deletes the command, which owns the trip it removed.
    QUndoStack undoStack;
    cwCave cave;
    cave.setUndoStack(&undoStack);

    cwTrip* trip = addPrefixedTrip(&cave, { QStringLiteral("A.s1") });
    QPointer<cwTrip> tripGuard(trip);

    const auto advanceSolve = [&cave](const QString& station) {
        cwStationPositionLookup grown = cave.stationPositionLookup();
        grown.setPosition(station, QVector3D(4, 5, 6));
        cave.setStationPositionLookup(grown);
    };

    QSignalSpy pulseSpy(trip, &cwTrip::solvedStationsChanged);

    advanceSolve(QStringLiteral("A.s2"));
    CHECK(pulseSpy.count() == 1);

    cave.removeTrip(0);
    REQUIRE_FALSE(tripGuard.isNull());

    advanceSolve(QStringLiteral("A.s3"));
    CHECK(pulseSpy.count() == 1);
}
