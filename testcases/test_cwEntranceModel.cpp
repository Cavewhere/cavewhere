// Pins a survey node's entrance list (cwEntranceModel) and the user verbs on
// cwSurveyNode that change a fix and an entrance together. The model setters,
// load and import touch one list only; the verbs are the one place the two
// move together.

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwEntrance.h"
#include "cwEntranceModel.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwFutureManagerModel.h"
#include "cwProject.h"
#include "cwProtoUtils.h"
#include "cwRootData.h"
#include "cwSurveyNode.h"
#include "cavewhere.pb.h"

//Test helpers
#include "LoadProjectHelper.h"
#include "SurveyTreeTestHelper.h"

//Qt includes
#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

//Std includes
#include <memory>

namespace {

const QString kA1 = QStringLiteral("A1");
const QString kLowerA1 = QStringLiteral("a1");
const QString kA10 = QStringLiteral("A10");
const QString kB2 = QStringLiteral("B2");
const QString kC3 = QStringLiteral("C3");
const QString kNorthShaft = QStringLiteral("North Shaft");

using SurveyTreeTestHelper::entranceNames;
using SurveyTreeTestHelper::entranceOn;

QStringList fixNames(const cwFixStationModel* model)
{
    QStringList names;
    for (const cwFixStation& fix : model->fixStations()) {
        names.append(fix.stationName());
    }
    return names;
}

// std::string, so a failure prints both lists.
std::string joined(const QStringList& names)
{
    return names.join(QStringLiteral(", ")).toStdString();
}

} // namespace

TEST_CASE("cwEntranceModel adds an entrance once per station and never for a blank name",
          "[cwEntranceModel]")
{
    cwEntranceModel model;
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy countSpy(&model, &cwEntranceModel::countChanged);

    CHECK(model.addEntrance(kA1) == 0);
    CHECK(model.addEntrance(QStringLiteral(" a1 ")) == 0);
    CHECK(model.addEntrance(QString()) == -1);
    CHECK(model.addEntrance(QStringLiteral("   ")) == -1);
    CHECK(model.addEntrance(kB2) == 1);

    CHECK(model.rowCount() == 2);
    CHECK(model.count() == 2);
    CHECK(joined(entranceNames(&model)) == joined({kA1, kB2}));
    CHECK(model.entranceAt(0).label().isEmpty());
    CHECK_FALSE(model.entranceAt(0).id().isNull());

    REQUIRE(insertSpy.count() == 2);
    CHECK(insertSpy.at(0).at(1).toInt() == 0);
    CHECK(insertSpy.at(0).at(2).toInt() == 0);
    CHECK(insertSpy.at(1).at(1).toInt() == 1);
    CHECK(insertSpy.at(1).at(2).toInt() == 1);
    CHECK(countSpy.count() == 2);
}

TEST_CASE("cwEntranceModel looks an entrance up by station name", "[cwEntranceModel]")
{
    cwEntranceModel model;
    model.setEntrances({entranceOn(kA1), entranceOn(kB2)});

    CHECK(model.indexOf(kA1) == 0);
    CHECK(model.indexOf(QStringLiteral("b2")) == 1);
    CHECK(model.indexOf(QStringLiteral(" B2 ")) == 1);
    CHECK(model.indexOf(kC3) == -1);
    CHECK(model.indexOf(QString()) == -1);
    CHECK(model.isEntrance(QStringLiteral("a1")));
    CHECK_FALSE(model.isEntrance(kC3));
}

TEST_CASE("cwEntranceModel removes one row by name or by row", "[cwEntranceModel]")
{
    cwEntranceModel model;
    model.setEntrances({entranceOn(kA1), entranceOn(kB2), entranceOn(kC3)});
    REQUIRE(model.count() == 3);

    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);
    model.removeEntrance(QStringLiteral("b2"));
    CHECK(joined(entranceNames(&model)) == joined({kA1, kC3}));
    REQUIRE(removeSpy.count() == 1);
    CHECK(removeSpy.first().at(1).toInt() == 1);
    CHECK(removeSpy.first().at(2).toInt() == 1);

    model.removeEntrance(QStringLiteral("Z9"));
    model.removeAt(5);
    model.removeAt(-1);
    CHECK(removeSpy.count() == 1);

    model.removeAt(0);
    CHECK(joined(entranceNames(&model)) == joined({kC3}));
    CHECK(model.count() == 1);
}

TEST_CASE("cwEntranceModel inserts at a clamped row and replaces its rows in one reset",
          "[cwEntranceModel]")
{
    cwEntranceModel model;
    model.appendEntrance(entranceOn(kB2));
    model.insertEntrance(-3, entranceOn(kA1));
    model.insertEntrance(99, entranceOn(kC3));
    CHECK(joined(entranceNames(&model)) == joined({kA1, kB2, kC3}));

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy countSpy(&model, &cwEntranceModel::countChanged);
    const cwEntrance labeled = entranceOn(kA10, kNorthShaft);
    model.setEntrances({labeled});
    CHECK(resetSpy.count() == 1);
    CHECK(countSpy.count() == 1);
    REQUIRE(model.count() == 1);
    CHECK(model.entranceAt(0) == labeled);
    CHECK(model.entrances() == QList<cwEntrance>({labeled}));
}

TEST_CASE("cwEntranceModel edits a label or a name in place with one dataChanged",
          "[cwEntranceModel]")
{
    cwEntranceModel model;
    REQUIRE(model.addEntrance(kA1) == 0);
    const QModelIndex idx = model.index(0);
    const QUuid id = model.entranceAt(0).id();

    CHECK(model.data(idx, cwEntranceModel::StationNameRole).toString() == kA1);
    CHECK(model.data(idx, cwEntranceModel::IdRole).toUuid() == id);
    const QVariant blankLabel = model.data(idx, cwEntranceModel::LabelRole);
    CHECK(blankLabel.typeId() == QMetaType::QString);
    CHECK(blankLabel.toString().isEmpty());
    CHECK(model.flags(idx).testFlag(Qt::ItemIsEditable));

    QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);
    REQUIRE(model.setData(idx, kNorthShaft, cwEntranceModel::LabelRole));
    CHECK(model.data(idx, cwEntranceModel::LabelRole).toString() == kNorthShaft);
    CHECK(model.entranceAt(0).label() == kNorthShaft);
    REQUIRE(spy.count() == 1);
    CHECK(spy.first().at(2).value<QList<int>>() == QList<int>({cwEntranceModel::LabelRole}));

    // Writing the value the row holds writes nothing.
    CHECK_FALSE(model.setData(idx, kNorthShaft, cwEntranceModel::LabelRole));
    CHECK(spy.count() == 1);

    REQUIRE(model.setData(idx, kA10, cwEntranceModel::StationNameRole));
    CHECK(model.entranceAt(0).stationName() == kA10);
    CHECK(model.entranceAt(0).id() == id);
    REQUIRE(spy.count() == 2);
    CHECK(spy.at(1).at(2).value<QList<int>>() == QList<int>({cwEntranceModel::StationNameRole}));

    CHECK_FALSE(model.setData(model.index(3), kNorthShaft, cwEntranceModel::LabelRole));
}

TEST_CASE("cwEntranceModel exposes role names for QML", "[cwEntranceModel]")
{
    cwEntranceModel model;
    const QHash<int, QByteArray> roles = model.roleNames();
    CHECK(roles.value(cwEntranceModel::StationNameRole) == "stationName");
    CHECK(roles.value(cwEntranceModel::LabelRole) == "label");
    CHECK(roles.value(cwEntranceModel::IdRole) == "id");
    CHECK(roles.size() == 3);
}

TEST_CASE("cwEntranceModel announces every change to the set of entrance stations, and only those",
          "[cwEntranceModel]")
{
    cwEntranceModel model;
    QSignalSpy spy(&model, &cwEntranceModel::entranceStationsChanged);

    REQUIRE(model.addEntrance(kA1) == 0);
    CHECK(spy.count() == 1);

    // A label is no change to which stations are entrances.
    REQUIRE(model.setData(model.index(0), kNorthShaft, cwEntranceModel::LabelRole));
    CHECK(spy.count() == 1);

    REQUIRE(model.setData(model.index(0), kA10, cwEntranceModel::StationNameRole));
    CHECK(spy.count() == 2);

    model.setEntrances({entranceOn(kB2), entranceOn(kC3)});
    CHECK(spy.count() == 3);

    model.removeEntrance(kB2);
    CHECK(spy.count() == 4);
}

TEST_CASE("cwEntrance proto round-trip keeps the id, station and label",
          "[cwEntranceModel][proto]")
{
    SECTION("a labeled entrance") {
        const cwEntrance original = entranceOn(kA1, kNorthShaft);

        CavewhereProto::Entrance proto;
        cwProtoUtils::saveEntrance(&proto, original);
        CHECK(QString::fromStdString(proto.stationname()) == kA1);
        CHECK(QString::fromStdString(proto.label()) == kNorthShaft);
        CHECK(proto.has_id());

        const cwEntrance restored = cwProtoUtils::fromProtoEntrance(proto);
        CHECK(restored.id() == original.id());
        CHECK(restored.stationName() == kA1);
        CHECK(restored.label() == kNorthShaft);
        CHECK(restored == original);
    }

    SECTION("an unlabeled entrance writes no label") {
        const cwEntrance original = entranceOn(kB2);

        CavewhereProto::Entrance proto;
        cwProtoUtils::saveEntrance(&proto, original);
        CHECK(proto.has_stationname());
        CHECK_FALSE(proto.has_label());

        const cwEntrance restored = cwProtoUtils::fromProtoEntrance(proto);
        CHECK(restored == original);
        CHECK(restored.label().isEmpty());
    }
}

TEST_CASE("A survey node's data snapshot carries its entrances", "[SurveyNode][cwEntranceModel]")
{
    cwCave source;
    const QList<cwEntrance> entrances = {entranceOn(kA1, kNorthShaft), entranceOn(kB2)};
    source.entrances()->setEntrances(entrances);

    const cwCaveData data = source.data();
    CHECK(data.entrances == entrances);

    cwCave copy;
    copy.setData(data);
    CHECK(copy.entrances()->entrances() == entrances);
}

TEST_CASE("Entrance edits on a saved project survive reopening it",
          "[cwEntranceModel][cwSaveLoad]")
{
    auto creatorRoot = std::make_unique<cwRootData>();
    auto creatorProject = creatorRoot->project();
    auto creatorRegion = creatorProject->cavingRegion();
    creatorRegion->addCave();
    cwCave* cave = creatorRegion->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Entrance Cave"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("entrances-roundtrip-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(creatorProject->saveAs(projectPath));
    creatorRoot->futureManagerModel()->waitForFinished();
    creatorProject->waitSaveToFinish();

    // Edited after the save, so only the model's own save trigger can carry
    // these to disk. The label edit comes last and alone, so only dataChanged
    // carries it.
    cwEntranceModel* entrances = cave->entrances();
    REQUIRE(entrances->addEntrance(kA1) == 0);
    REQUIRE(entrances->addEntrance(kB2) == 1);
    creatorRoot->futureManagerModel()->waitForFinished();
    creatorProject->waitSaveToFinish();
    REQUIRE(entrances->setData(entrances->index(0), kNorthShaft, cwEntranceModel::LabelRole));
    const QList<cwEntrance> saved = entrances->entrances();
    creatorRoot->futureManagerModel()->waitForFinished();
    creatorProject->waitSaveToFinish();

    auto loaderRoot = std::make_unique<cwRootData>();
    auto loaderProject = loaderRoot->project();
    addTokenManager(loaderProject);
    loaderProject->loadOrConvert(creatorProject->filename());
    loaderRoot->futureManagerModel()->waitForFinished();
    loaderProject->waitLoadToFinish();

    REQUIRE(loaderProject->cavingRegion()->caveCount() == 1);
    const cwEntranceModel* loaded = loaderProject->cavingRegion()->cave(0)->entrances();
    REQUIRE(loaded->count() == 2);
    CHECK(loaded->entranceAt(0).stationName() == kA1);
    CHECK(loaded->entranceAt(0).label() == kNorthShaft);
    CHECK(loaded->entranceAt(1).stationName() == kB2);
    CHECK(loaded->entranceAt(1).label().isEmpty());
    CHECK(loaded->entrances() == saved);
    CHECK(loaderProject->cavingRegion()->cave(0)->fixStations()->count() == 0);
}

TEST_CASE("addFixedEntrance fixes a station and marks it an entrance",
          "[SurveyNode][cwEntranceModel][FixStation]")
{
    cwCave cave;
    cwFixStationModel* fixes = cave.fixStations();
    cwEntranceModel* entrances = cave.entrances();

    SECTION("an unfixed station gets a fix and an entrance") {
        CHECK(cave.addFixedEntrance(kA1) == 0);
        CHECK(joined(fixNames(fixes)) == joined({kA1}));
        CHECK(joined(entranceNames(entrances)) == joined({kA1}));
        CHECK(entrances->entranceAt(0).label().isEmpty());
    }

    SECTION("a station already fixed is left as it is") {
        // Edit Fixed Coordinates on a fix the user left un-entranced.
        REQUIRE(fixes->addFixStation(kA1) == 0);
        CHECK(cave.addFixedEntrance(QStringLiteral("a1")) == 0);
        CHECK(fixes->count() == 1);
        CHECK(entrances->count() == 0);
    }

    SECTION("an existing entrance is kept, not duplicated") {
        REQUIRE(entrances->addEntrance(kA1) == 0);
        REQUIRE(entrances->setData(entrances->index(0), kNorthShaft, cwEntranceModel::LabelRole));
        const QUuid entranceId = entrances->entranceAt(0).id();

        CHECK(cave.addFixedEntrance(kA1) == 0);
        CHECK(joined(fixNames(fixes)) == joined({kA1}));
        REQUIRE(entrances->count() == 1);
        CHECK(entrances->entranceAt(0).id() == entranceId);
        CHECK(entrances->entranceAt(0).label() == kNorthShaft);
    }

    SECTION("a blank name adds nothing") {
        CHECK(cave.addFixedEntrance(QStringLiteral("  ")) == -1);
        CHECK(fixes->count() == 0);
        CHECK(entrances->count() == 0);
    }
}

TEST_CASE("removeFixAndEntrance removes a station's fix and its entrance",
          "[SurveyNode][cwEntranceModel][FixStation]")
{
    cwCave cave;
    cwFixStationModel* fixes = cave.fixStations();
    cwEntranceModel* entrances = cave.entrances();
    REQUIRE(cave.addFixedEntrance(kA1) == 0);
    REQUIRE(cave.addFixedEntrance(kB2) == 1);

    cave.removeFixAndEntrance(QStringLiteral("a1"));

    CHECK(joined(fixNames(fixes)) == joined({kB2}));
    CHECK(joined(entranceNames(entrances)) == joined({kB2}));
}

TEST_CASE("removeFixAndEntranceAt removes a fix row and the entrance it names",
          "[SurveyNode][cwEntranceModel][FixStation]")
{
    cwCave cave;
    cwFixStationModel* fixes = cave.fixStations();
    cwEntranceModel* entrances = cave.entrances();
    fixes->addFixStation();
    REQUIRE(fixes->addFixStation(kA1) == 1);
    REQUIRE(entrances->addEntrance(kA1) == 0);

    // A blank row names no station, so it takes no entrance with it.
    cave.removeFixAndEntranceAt(0);
    CHECK(joined(fixNames(fixes)) == joined({kA1}));
    CHECK(joined(entranceNames(entrances)) == joined({kA1}));

    cave.removeFixAndEntranceAt(3);
    CHECK(fixes->count() == 1);
    CHECK(entrances->count() == 1);

    cave.removeFixAndEntranceAt(0);
    CHECK(fixes->count() == 0);
    CHECK(entrances->count() == 0);
}

TEST_CASE("A fix written through the fix model alone marks no entrance",
          "[SurveyNode][cwEntranceModel][FixStation]")
{
    // Load, import and the model setters never couple a fix to an entrance.
    cwCave cave;
    cwFixStationModel* fixes = cave.fixStations();
    cwEntranceModel* entrances = cave.entrances();

    fixes->addFixStation(kA1);
    fixes->addFixStation();
    REQUIRE(fixes->setData(fixes->index(1), kB2, cwFixStationModel::StationNameRole));
    fixes->appendFixStation(SurveyTreeTestHelper::fixAt(kC3));
    fixes->insertFixStation(0, SurveyTreeTestHelper::fixAt(kA10));
    REQUIRE(fixes->count() == 4);
    CHECK(entrances->count() == 0);

    fixes->setFixStations({SurveyTreeTestHelper::fixAt(kA1)});
    CHECK(entrances->count() == 0);

    cwCaveData data = cave.data();
    data.fixStations.append(SurveyTreeTestHelper::fixAt(kB2));
    cave.setData(data);
    REQUIRE(fixes->count() == 2);
    CHECK(entrances->count() == 0);
}

TEST_CASE("A fix removed through the fix model alone leaves its entrance",
          "[SurveyNode][cwEntranceModel][FixStation]")
{
    cwCave cave;
    cwFixStationModel* fixes = cave.fixStations();
    cwEntranceModel* entrances = cave.entrances();
    REQUIRE(cave.addFixedEntrance(kA1) == 0);
    REQUIRE(cave.addFixedEntrance(kB2) == 1);

    fixes->removeFixStation(kA1);
    fixes->removeAt(0);
    CHECK(fixes->count() == 0);
    CHECK(joined(entranceNames(entrances)) == joined({kA1, kB2}));
}

TEST_CASE("setFixStationName moves a fix's entrance with it",
          "[SurveyNode][cwEntranceModel][FixStation]")
{
    cwCave cave;
    cwFixStationModel* fixes = cave.fixStations();
    cwEntranceModel* entrances = cave.entrances();

    SECTION("a corrected name carries the entrance, its id and its label") {
        REQUIRE(fixes->addFixStation(kA1) == 0);
        REQUIRE(entrances->addEntrance(kA1) == 0);
        REQUIRE(entrances->setData(entrances->index(0), kNorthShaft, cwEntranceModel::LabelRole));
        const QUuid entranceId = entrances->entranceAt(0).id();
        QSignalSpy fixSpy(fixes, &QAbstractItemModel::dataChanged);

        CHECK(cave.setFixStationName(0, kA10));

        CHECK(joined(fixNames(fixes)) == joined({kA10}));
        // The same one announcement a page edit through setData makes.
        REQUIRE(fixSpy.count() == 1);
        CHECK(fixSpy.first().at(2).value<QList<int>>() == QList<int>({cwFixStationModel::StationNameRole}));
        REQUIRE(entrances->count() == 1);
        CHECK(entrances->entranceAt(0).stationName() == kA10);
        CHECK(entrances->entranceAt(0).id() == entranceId);
        CHECK(entrances->entranceAt(0).label() == kNorthShaft);
    }

    SECTION("a rename by letter case respells the entrance and keeps its id and label") {
        REQUIRE(fixes->addFixStation(kLowerA1) == 0);
        REQUIRE(entrances->addEntrance(kLowerA1) == 0);
        REQUIRE(entrances->setData(entrances->index(0), kNorthShaft, cwEntranceModel::LabelRole));
        const QUuid entranceId = entrances->entranceAt(0).id();
        QSignalSpy entranceSpy(entrances, &QAbstractItemModel::dataChanged);

        CHECK(cave.setFixStationName(0, kA1));

        CHECK(joined(fixNames(fixes)) == joined({kA1}));
        REQUIRE(entrances->count() == 1);
        CHECK(entrances->entranceAt(0).stationName() == kA1);
        CHECK(entrances->entranceAt(0).id() == entranceId);
        CHECK(entrances->entranceAt(0).label() == kNorthShaft);
        REQUIRE(entranceSpy.count() == 1);
        CHECK(entranceSpy.first().at(2).value<QList<int>>() == QList<int>({cwEntranceModel::StationNameRole}));
    }

    SECTION("naming a blank row marks the new station an entrance") {
        fixes->addFixStation();
        REQUIRE(fixes->count() == 1);

        CHECK(cave.setFixStationName(0, kB2));

        CHECK(joined(fixNames(fixes)) == joined({kB2}));
        CHECK(joined(entranceNames(entrances)) == joined({kB2}));
    }

    SECTION("renaming onto a station that is already an entrance keeps both entries") {
        REQUIRE(fixes->addFixStation(kA1) == 0);
        REQUIRE(entrances->addEntrance(kA1) == 0);
        REQUIRE(entrances->addEntrance(kB2) == 1);

        CHECK(cave.setFixStationName(0, kB2));

        CHECK(joined(fixNames(fixes)) == joined({kB2}));
        CHECK(joined(entranceNames(entrances)) == joined({kA1, kB2}));
    }

    SECTION("renaming a fix that is no entrance marks none") {
        REQUIRE(fixes->addFixStation(kA1) == 0);

        CHECK(cave.setFixStationName(0, kA10));

        CHECK(joined(fixNames(fixes)) == joined({kA10}));
        CHECK(entrances->count() == 0);
    }

    SECTION("clearing a name leaves the entrance where it was") {
        REQUIRE(fixes->addFixStation(kA1) == 0);
        REQUIRE(entrances->addEntrance(kA1) == 0);

        CHECK(cave.setFixStationName(0, QString()));

        CHECK(fixes->fixStationAt(0).stationName().isEmpty());
        CHECK(joined(entranceNames(entrances)) == joined({kA1}));
    }

    SECTION("the same name, or a row that doesn't exist, changes nothing") {
        REQUIRE(fixes->addFixStation(kA1) == 0);
        REQUIRE(entrances->addEntrance(kA1) == 0);
        QSignalSpy fixSpy(fixes, &QAbstractItemModel::dataChanged);

        CHECK_FALSE(cave.setFixStationName(0, kA1));
        CHECK_FALSE(cave.setFixStationName(4, kB2));

        CHECK(fixSpy.count() == 0);
        CHECK(joined(fixNames(fixes)) == joined({kA1}));
        CHECK(joined(entranceNames(entrances)) == joined({kA1}));
    }
}
