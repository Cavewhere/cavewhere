//Our includes
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwProtoUtils.h"
#include "cavewhere.pb.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwGeoPoint.h"
#include "cwFutureManagerModel.h"

//Test helpers
#include "LoadProjectHelper.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>

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

TEST_CASE("cwFixStations and globalCS/worldOrigin survive a project save/load",
          "[FixStation][cwSaveLoad]") {
    // Build a project with two caves: one with two fixes, one with none.
    // Set globalCS + worldOrigin on the region. Save to a temp dir, reload into
    // a fresh project, and verify everything came back intact.
    auto creatorRoot = std::make_unique<cwRootData>();
    auto creatorProject = creatorRoot->project();
    auto creatorRegion = creatorProject->cavingRegion();

    creatorRegion->setGlobalCS(QStringLiteral("EPSG:32612"));
    creatorRegion->setWorldOrigin(cwGeoPoint(500000.0, 4194000.0, 2700.0));

    creatorRegion->addCave();
    auto fixedCave = creatorRegion->cave(0);
    REQUIRE(fixedCave != nullptr);
    fixedCave->setName(QStringLiteral("Fixed Cave"));

    cwFixStation a;
    a.setStationName(QStringLiteral("A1"));
    a.setInputCS(QStringLiteral("EPSG:4326"));
    a.setEasting(-110.123456);
    a.setNorthing(37.987654);
    a.setElevation(2750.5);
    a.setHorizontalVariance(0.5);
    a.setVerticalVariance(1.0);

    cwFixStation b;
    b.setStationName(QStringLiteral("B2"));
    b.setInputCS(QStringLiteral("EPSG:32612"));
    b.setEasting(500123.456);
    b.setNorthing(4194567.89);
    b.setElevation(2745.0);

    fixedCave->fixStations()->setFixStations({a, b});

    creatorRegion->addCave();
    auto unfixedCave = creatorRegion->cave(1);
    REQUIRE(unfixedCave != nullptr);
    unfixedCave->setName(QStringLiteral("Unfixed Cave"));
    REQUIRE(unfixedCave->fixStations()->count() == 0);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("fixstations-roundtrip-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(creatorProject->saveAs(projectPath));
    creatorRoot->futureManagerModel()->waitForFinished();
    creatorProject->waitSaveToFinish();

    const QString savedProjectFile = creatorProject->filename();
    REQUIRE(QFileInfo::exists(savedProjectFile));

    // Reload into a fresh project.
    auto loaderRoot = std::make_unique<cwRootData>();
    auto loaderProject = loaderRoot->project();
    addTokenManager(loaderProject);
    loaderProject->loadOrConvert(savedProjectFile);
    loaderRoot->futureManagerModel()->waitForFinished();
    loaderProject->waitLoadToFinish();

    auto loadedRegion = loaderProject->cavingRegion();
    REQUIRE(loadedRegion != nullptr);

    CHECK(loadedRegion->globalCS() == QStringLiteral("EPSG:32612"));
    const cwGeoPoint loadedOrigin = loadedRegion->worldOrigin();
    CHECK(loadedOrigin.x == 500000.0);
    CHECK(loadedOrigin.y == 4194000.0);
    CHECK(loadedOrigin.z == 2700.0);

    REQUIRE(loadedRegion->caveCount() == 2);

    // Caves are loaded in directory order; locate by name.
    cwCave* loadedFixed = nullptr;
    cwCave* loadedUnfixed = nullptr;
    for (cwCave* cave : loadedRegion->caves()) {
        if (cave->name() == QStringLiteral("Fixed Cave")) {
            loadedFixed = cave;
        } else if (cave->name() == QStringLiteral("Unfixed Cave")) {
            loadedUnfixed = cave;
        }
    }
    REQUIRE(loadedFixed != nullptr);
    REQUIRE(loadedUnfixed != nullptr);

    REQUIRE(loadedFixed->fixStations()->count() == 2);
    const cwFixStation loadedA = loadedFixed->fixStations()->fixStationAt(0);
    const cwFixStation loadedB = loadedFixed->fixStations()->fixStationAt(1);

    CHECK(loadedA.stationName() == a.stationName());
    CHECK(loadedA.inputCS() == a.inputCS());
    CHECK(loadedA.easting() == a.easting());
    CHECK(loadedA.northing() == a.northing());
    CHECK(loadedA.elevation() == a.elevation());
    CHECK(loadedA.horizontalVariance() == a.horizontalVariance());
    CHECK(loadedA.verticalVariance() == a.verticalVariance());
    CHECK(loadedA.id() == a.id());

    CHECK(loadedB.stationName() == b.stationName());
    CHECK(loadedB.inputCS() == b.inputCS());
    CHECK(loadedB.easting() == b.easting());
    CHECK(loadedB.northing() == b.northing());
    CHECK(loadedB.elevation() == b.elevation());
    CHECK(loadedB.id() == b.id());

    CHECK(loadedUnfixed->fixStations()->count() == 0);
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
