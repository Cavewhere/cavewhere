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

// cwSurveyNetwork canonicalizes station keys to lowercase, so qualified
// names read back from the model are lowercase regardless of input case.
cwSurveyNetwork makeScopedNetwork()
{
    cwSurveyNetwork network;
    network.addShot(QStringLiteral("cave_x.trip_y.A1"), QStringLiteral("cave_x.trip_y.A2"));
    network.addShot(QStringLiteral("cave_x.trip_y.A2"), QStringLiteral("cave_x.trip_y.sidepassage.B1"));
    network.addShot(QStringLiteral("cave_x.trip_y.A1"), QStringLiteral("cave_x.trip_z.C1"));
    network.setPosition(QStringLiteral("cave_x.trip_y.A1"), QVector3D(0.0f, 0.0f, 0.0f));
    network.setPosition(QStringLiteral("cave_x.trip_y.A2"), QVector3D(1.0f, 2.0f, 3.0f));
    network.setPosition(QStringLiteral("cave_x.trip_y.sidepassage.B1"), QVector3D(4.0f, 5.0f, 6.0f));
    network.setPosition(QStringLiteral("cave_x.trip_z.C1"), QVector3D(7.0f, 8.0f, 9.0f));
    return network;
}

QStringList roleValues(const cwScopeStationListModel& model, int role)
{
    QStringList values;
    for (int i = 0; i < model.rowCount(); ++i) {
        values.append(model.data(model.index(i, 0), role).toString());
    }
    return values;
}

// A native (unprefixed) trip carrying two chunk stations, plus a cave lookup
// that solved both to a position. This is the case a scopePrefix path cannot
// select — an empty prefix — so it exercises the trip-driven path proper.
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

}

TEST_CASE("Prefix strip and out-of-scope filtering", "[Model][ScopeStations]")
{
    cwScopeStationListModel model;
    model.setNetwork(makeScopedNetwork());
    model.setScopePrefix(QStringLiteral("cave_x.trip_y."));

    REQUIRE(model.rowCount() == 3);

    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("a1"),
                           QStringLiteral("a2"),
                           QStringLiteral("sidepassage.b1") }));
    CHECK(roleValues(model, cwScopeStationListModel::QualifiedNameRole)
          == QStringList({ QStringLiteral("cave_x.trip_y.a1"),
                           QStringLiteral("cave_x.trip_y.a2"),
                           QStringLiteral("cave_x.trip_y.sidepassage.b1") }));

    // Out-of-scope trip_z station is skipped entirely
    CHECK(!roleValues(model, cwScopeStationListModel::QualifiedNameRole)
           .contains(QStringLiteral("cave_x.trip_z.c1")));

    CHECK(model.data(model.index(1, 0), cwScopeStationListModel::PositionRole).value<QVector3D>()
          == QVector3D(1.0f, 2.0f, 3.0f));

    // Pin the QML-facing role names; "stationPosition" avoids shadowing by
    // QC control roots that have their own position property
    CHECK(model.roleNames().value(cwScopeStationListModel::StationNameRole)
          == QByteArrayLiteral("stationName"));
    CHECK(model.roleNames().value(cwScopeStationListModel::QualifiedNameRole)
          == QByteArrayLiteral("qualifiedName"));
    CHECK(model.roleNames().value(cwScopeStationListModel::PositionRole)
          == QByteArrayLiteral("stationPosition"));

    // Mixed-case prefixes are canonicalized the same way as station keys
    model.setScopePrefix(QStringLiteral("CAVE_X.TRIP_Y.SIDEPASSAGE."));
    REQUIRE(model.rowCount() == 1);
    CHECK(model.data(model.index(0, 0), cwScopeStationListModel::StationNameRole).toString()
          == QStringLiteral("b1"));
}

TEST_CASE("Rows are sorted in natural order by qualified name", "[Model][ScopeStations]")
{
    // Station keys are canonical lowercase, so case-insensitivity is
    // unobservable here; this pins the natural order. QHash iteration is
    // arbitrary, so enough stations that an unsorted model can't pass by luck.
    // The trailing number must ascend numerically: "a2" before "a10", not the
    // lexicographic "a10" before "a2".
    cwSurveyNetwork network;
    network.addShot(QStringLiteral("cave_x.trip_y.B1"), QStringLiteral("cave_x.trip_y.A2"));
    network.addShot(QStringLiteral("cave_x.trip_y.A1"), QStringLiteral("cave_x.trip_y.C3"));
    network.addShot(QStringLiteral("cave_x.trip_y.A10"), QStringLiteral("cave_x.trip_y.B2"));
    network.addShot(QStringLiteral("cave_x.trip_y.AA1"), QStringLiteral("cave_x.trip_y.B1"));

    cwScopeStationListModel model;
    model.setNetwork(network);
    model.setScopePrefix(QStringLiteral("cave_x.trip_y."));

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

TEST_CASE("Network replacement resets the model once with the new rows", "[Model][ScopeStations]")
{
    cwScopeStationListModel model;
    model.setNetwork(makeScopedNetwork());
    model.setScopePrefix(QStringLiteral("cave_x.trip_y."));
    REQUIRE(model.rowCount() == 3);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    cwSurveyNetwork replacement;
    replacement.addShot(QStringLiteral("cave_x.trip_y.D1"), QStringLiteral("cave_x.trip_y.D2"));
    model.setNetwork(replacement);

    CHECK(resetSpy.count() == 1);
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("d1"), QStringLiteral("d2") }));

    // Setting an equal network is a no-op: no reset, no signal
    QSignalSpy networkSpy(&model, &cwScopeStationListModel::networkChanged);
    model.setNetwork(replacement);
    CHECK(resetSpy.count() == 1);
    CHECK(networkSpy.count() == 0);
}

TEST_CASE("Empty prefix yields zero rows", "[Model][ScopeStations]")
{
    cwScopeStationListModel model;
    model.setNetwork(makeScopedNetwork());

    // No prefix means no rows — not the whole region
    CHECK(model.rowCount() == 0);

    model.setScopePrefix(QStringLiteral("cave_x.trip_y."));
    CHECK(model.rowCount() == 3);

    model.setScopePrefix(QString());
    CHECK(model.rowCount() == 0);
}

TEST_CASE("Clearing the network empties the model", "[Model][ScopeStations]")
{
    cwScopeStationListModel model;
    model.setNetwork(makeScopedNetwork());
    model.setScopePrefix(QStringLiteral("cave_x.trip_y."));
    REQUIRE(model.rowCount() == 3);

    model.setNetwork(cwSurveyNetwork());
    CHECK(model.rowCount() == 0);
}

TEST_CASE("A native (unprefixed) trip lists its solved chunk stations", "[Model][ScopeStations]")
{
    // The whole point of the trip-driven path: a native trip has an empty scope
    // prefix, which the scopePrefix path treats as zero rows. solvedStations()
    // still yields the bare chunk-station names the note site speaks.
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
    // Native qualified name == the bare name (empty scope prefix).
    CHECK(roleValues(model, cwScopeStationListModel::QualifiedNameRole)
          == QStringList({ QStringLiteral("s1"), QStringLiteral("s2") }));
    CHECK(model.data(model.index(0, 0), cwScopeStationListModel::PositionRole).value<QVector3D>()
          == QVector3D(1, 2, 3));

    // Membership advisory: in-scope names hit (case-insensitively), others miss.
    CHECK(model.containsStation(QStringLiteral("s1")));
    CHECK(model.containsStation(QStringLiteral("S2")));
    CHECK_FALSE(model.containsStation(QStringLiteral("outside")));
    CHECK_FALSE(model.containsStation(QStringLiteral("nope")));
}

TEST_CASE("matchingStations filters in-scope names by typed prefix", "[Model][ScopeStations]")
{
    cwScopeStationListModel model;
    model.setNetwork(makeScopedNetwork());
    model.setScopePrefix(QStringLiteral("cave_x.trip_y."));
    // Rows: a1, a2, sidepassage.b1 (sorted by qualified name).

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
}

TEST_CASE("A native-prefixed trip lists scope-relative tails", "[Model][ScopeStations]")
{
    // Prefix "A": the cave lookup keys stations A.s1/A.s2; the model must strip
    // the prefix to the tail and qualify it back with the same prefix.
    cwCave cave;
    cwTrip* trip = new cwTrip();
    trip->setStationPrefix(QStringLiteral("A"));
    cave.addTrip(trip);

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("A.s1"), QVector3D(1, 2, 3));
    lookup.setPosition(QStringLiteral("A.s2"), QVector3D(4, 5, 6));
    cave.setStationPositionLookup(lookup);

    cwScopeStationListModel model;
    model.setTrip(trip);

    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("s1"), QStringLiteral("s2") }));
    CHECK(roleValues(model, cwScopeStationListModel::QualifiedNameRole)
          == QStringList({ QStringLiteral("A.s1"), QStringLiteral("A.s2") }));
}

TEST_CASE("A set trip takes precedence over scopePrefix", "[Model][ScopeStations]")
{
    cwCave cave;
    cwTrip* trip = new cwTrip();
    trip->setStationPrefix(QStringLiteral("A"));
    cave.addTrip(trip);

    cwStationPositionLookup lookup;
    lookup.setPosition(QStringLiteral("A.s1"), QVector3D(1, 2, 3));
    cave.setStationPositionLookup(lookup);

    cwScopeStationListModel model;
    // A legacy prefix + network is present, but the trip must win.
    model.setNetwork(makeScopedNetwork());
    model.setScopePrefix(QStringLiteral("cave_x.trip_y."));
    REQUIRE(model.rowCount() == 3);

    model.setTrip(trip);
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("s1") }));

    // Clearing the trip falls back to the legacy prefix path.
    model.setTrip(nullptr);
    CHECK(model.rowCount() == 3);
}

TEST_CASE("A network change re-pulls the trip's solved stations", "[Model][ScopeStations]")
{
    // In trip mode the network value is unused, but its change is the re-solve
    // pulse: the model re-reads solvedStations() and picks up the new lookup.
    cwCave cave;
    cwTrip* trip = new cwTrip();
    trip->setStationPrefix(QStringLiteral("A"));
    cave.addTrip(trip);

    cwStationPositionLookup first;
    first.setPosition(QStringLiteral("A.s1"), QVector3D(1, 2, 3));
    cave.setStationPositionLookup(first);

    cwScopeStationListModel model;
    model.setTrip(trip);
    REQUIRE(model.rowCount() == 1);

    // The solve advances: a second station appears in the cave lookup.
    cwStationPositionLookup second = first;
    second.setPosition(QStringLiteral("A.s2"), QVector3D(4, 5, 6));
    cave.setStationPositionLookup(second);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    cwSurveyNetwork pulse;
    pulse.addShot(QStringLiteral("ignored.a"), QStringLiteral("ignored.b"));
    model.setNetwork(pulse);

    CHECK(resetSpy.count() == 1);
    CHECK(roleValues(model, cwScopeStationListModel::StationNameRole)
          == QStringList({ QStringLiteral("s1"), QStringLiteral("s2") }));
}
