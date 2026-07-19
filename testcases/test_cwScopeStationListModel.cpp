//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>
#include <QVector3D>

//Our includes
#include "cwScopeStationListModel.h"
#include "cwSurveyNetwork.h"

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

TEST_CASE("Rows are sorted by qualified name", "[Model][ScopeStations]")
{
    // Station keys are canonical lowercase, so case-insensitivity is
    // unobservable here; this pins the lexicographic order. QHash iteration
    // is arbitrary, so enough stations that an unsorted model can't pass by
    // luck.
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
                           QStringLiteral("a10"),
                           QStringLiteral("a2"),
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
