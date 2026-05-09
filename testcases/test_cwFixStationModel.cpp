//Our includes
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwProtoUtils.h"
#include "cavewhere.pb.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

TEST_CASE("cwFixStationModel starts empty", "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;
    CHECK(model.rowCount() == 0);
    CHECK(model.count() == 0);
}

TEST_CASE("cwFixStationModel addFixStation appends a row", "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);

    model.addFixStation();
    CHECK(model.rowCount() == 1);
    REQUIRE(insertSpy.count() == 1);

    const QList<QVariant>& args = insertSpy.first();
    CHECK(args.at(1).toInt() == 0);
    CHECK(args.at(2).toInt() == 0);
}

TEST_CASE("cwFixStationModel removeAt drops the row", "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;
    model.addFixStation();
    model.addFixStation();
    REQUIRE(model.rowCount() == 2);

    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.removeAt(0);
    CHECK(model.rowCount() == 1);
    REQUIRE(removeSpy.count() == 1);
    CHECK(removeSpy.first().at(1).toInt() == 0);
}

TEST_CASE("cwFixStationModel setData edits a cell and emits dataChanged",
          "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;
    model.addFixStation();

    QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);

    const QModelIndex idx = model.index(0);
    REQUIRE(model.setData(idx, QStringLiteral("A1"), cwFixStationModel::StationNameRole));
    REQUIRE(model.setData(idx, 500000.0, cwFixStationModel::EastingRole));
    REQUIRE(model.setData(idx, QStringLiteral("EPSG:32612"), cwFixStationModel::InputCSRole));

    CHECK(model.data(idx, cwFixStationModel::StationNameRole).toString() == QStringLiteral("A1"));
    CHECK(model.data(idx, cwFixStationModel::EastingRole).toDouble() == 500000.0);
    CHECK(model.data(idx, cwFixStationModel::InputCSRole).toString() == QStringLiteral("EPSG:32612"));

    CHECK(spy.count() == 3);

    // Setting to the same value should not emit dataChanged again.
    REQUIRE(!model.setData(idx, QStringLiteral("A1"), cwFixStationModel::StationNameRole));
    CHECK(spy.count() == 3);
}

TEST_CASE("cwFixStationModel exposes role names for QML", "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;
    const QHash<int, QByteArray> roles = model.roleNames();
    CHECK(roles.value(cwFixStationModel::StationNameRole) == "stationName");
    CHECK(roles.value(cwFixStationModel::InputCSRole) == "inputCS");
    CHECK(roles.value(cwFixStationModel::EastingRole) == "easting");
    CHECK(roles.value(cwFixStationModel::NorthingRole) == "northing");
    CHECK(roles.value(cwFixStationModel::ElevationRole) == "elevation");
    CHECK(roles.value(cwFixStationModel::IdRole) == "id");
}

TEST_CASE("cwFixStationModel setFixStations replaces contents", "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;
    model.addFixStation();
    model.addFixStation();
    REQUIRE(model.rowCount() == 2);

    cwFixStation a;
    a.setStationName(QStringLiteral("X"));
    cwFixStation b;
    b.setStationName(QStringLiteral("Y"));

    model.setFixStations({a, b});
    REQUIRE(model.rowCount() == 2);
    CHECK(model.fixStationAt(0).stationName() == QStringLiteral("X"));
    CHECK(model.fixStationAt(1).stationName() == QStringLiteral("Y"));
}

TEST_CASE("cwFixStation proto round-trip preserves all fields", "[FixStation][proto]") {
    cwFixStation original;
    original.setStationName(QStringLiteral("A1"));
    original.setInputCS(QStringLiteral("EPSG:32612"));
    original.setEasting(500123.456789);
    original.setNorthing(4194567.891234);
    original.setElevation(2750.5);
    original.setHorizontalVariance(0.5);
    original.setVerticalVariance(1.0);

    CavewhereProto::FixStation proto;
    cwProtoUtils::saveFixStation(&proto, original);

    cwFixStation restored = cwProtoUtils::fromProtoFixStation(proto);

    CHECK(restored.id() == original.id());
    CHECK(restored.stationName() == original.stationName());
    CHECK(restored.inputCS() == original.inputCS());
    CHECK(restored.easting() == original.easting());
    CHECK(restored.northing() == original.northing());
    CHECK(restored.elevation() == original.elevation());
    CHECK(restored.horizontalVariance() == original.horizontalVariance());
    CHECK(restored.verticalVariance() == original.verticalVariance());
}
