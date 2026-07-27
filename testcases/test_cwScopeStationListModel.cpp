//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>
#include <QVector3D>

//Our includes
#include "cwScopeStationListModel.h"
#include "cwSurveyNetwork.h"
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
}

TEST_CASE("A network change re-pulls the trip's solved stations", "[Model][ScopeStations]")
{
    // The network's value is unused, but its change is the re-solve pulse: the
    // model re-reads solvedStations() and picks up the new lookup.
    cwCave cave;
    cwTrip* trip = addPrefixedTrip(&cave, { QStringLiteral("A.s1") });

    cwScopeStationListModel model;
    model.setTrip(trip);
    REQUIRE(model.rowCount() == 1);

    // The solve advances: a second station appears in the cave lookup.
    cwStationPositionLookup second = cave.stationPositionLookup();
    second.setPosition(QStringLiteral("A.s2"), QVector3D(4, 5, 6));
    cave.setStationPositionLookup(second);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    cwSurveyNetwork pulse;
    pulse.addShot(QStringLiteral("ignored.a"), QStringLiteral("ignored.b"));
    model.setNetwork(pulse);

    CHECK(resetSpy.count() == 1);
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("s1"), QStringLiteral("s2") }));

    // Setting an equal network is a no-op: no reset, no signal.
    QSignalSpy networkSpy(&model, &cwScopeStationListModel::networkChanged);
    model.setNetwork(pulse);
    CHECK(resetSpy.count() == 1);
    CHECK(networkSpy.count() == 0);
}
