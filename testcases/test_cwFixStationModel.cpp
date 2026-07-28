//Our includes
#include "cwCoordinateText.h"
#include "cwCoordinateTransform.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwProtoUtils.h"
#include "cavewhere.pb.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwGeoReference.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwGeoPoint.h"
#include "cwSurveyNetwork.h"
#include "cwFutureManagerModel.h"

//Test helpers
#include "LoadProjectHelper.h"

//Submodule includes
#include "GitRepository.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QThread>

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
    //The CS before the component, as cwFixStation documents: it decides which
    //axis the coordinate leads with, so writing one first and naming the system
    //afterwards re-reads the same text the other way round.
    REQUIRE(model.setData(idx, QStringLiteral("EPSG:32612"), cwFixStationModel::InputCSRole));
    REQUIRE(model.setData(idx, 500000.0, cwFixStationModel::EastingRole));

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
    CHECK(roles.value(cwFixStationModel::CoordinateTextRole) == "coordinateText");

    // Only persisted roles live here. The read-only warnings derived from the
    // solve are added by cwFixStationDiagnosticsModel, so that dataChanged on
    // this model keeps meaning "save and re-solve this".
    CHECK(roles.size() == 9);
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

TEST_CASE("cwFixStationModel looks a fix up by station name",
          "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;

    cwFixStation fix;
    fix.setStationName(QStringLiteral("A1"));
    model.appendFixStation(fix);

    SECTION("An exact name matches") {
        CHECK(model.indexOf(QStringLiteral("A1")) == 0);
        CHECK(model.isFixed(QStringLiteral("A1")));
    }

    SECTION("Matching follows the rule the rest of the app resolves fixes by") {
        // Trim then case-insensitive, same as cwSurveyNetwork::hasStation — a
        // fix the survex export anchors fine must not read as a different
        // station here.
        CHECK(model.indexOf(QStringLiteral("a1")) == 0);
        CHECK(model.indexOf(QStringLiteral("  a1  ")) == 0);
    }

    SECTION("A different station doesn't match") {
        CHECK(model.indexOf(QStringLiteral("A2")) == -1);
        CHECK_FALSE(model.isFixed(QStringLiteral("A2")));
    }

    SECTION("An empty name matches nothing, including a blank row") {
        model.addFixStation();
        REQUIRE(model.rowCount() == 2);
        CHECK(model.indexOf(QString()) == -1);
        CHECK(model.indexOf(QStringLiteral("   ")) == -1);
        CHECK_FALSE(model.isFixed(QString()));
    }
}

TEST_CASE("cwFixStationModel addFixStation(name) won't double-anchor a station",
          "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;

    const int firstRow = model.addFixStation(QStringLiteral("A1"));
    REQUIRE(firstRow == 0);
    REQUIRE(model.rowCount() == 1);
    CHECK(model.fixStationAt(0).stationName() == QStringLiteral("A1"));

    SECTION("Marking the same station again returns the row it already has") {
        CHECK(model.addFixStation(QStringLiteral("A1")) == 0);
        CHECK(model.addFixStation(QStringLiteral("a1")) == 0);
        CHECK(model.rowCount() == 1);
    }

    SECTION("A different station gets its own row") {
        CHECK(model.addFixStation(QStringLiteral("A2")) == 1);
        CHECK(model.rowCount() == 2);
    }

    SECTION("A blank name anchors nothing") {
        CHECK(model.addFixStation(QStringLiteral("   ")) == -1);
        CHECK(model.rowCount() == 1);
    }

    SECTION("The stored name is trimmed") {
        CHECK(model.addFixStation(QStringLiteral("  A2  ")) == 1);
        CHECK(model.fixStationAt(1).stationName() == QStringLiteral("A2"));
    }
}

TEST_CASE("cwFixStationModel removeFixStation drops the row for a station",
          "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;
    model.addFixStation(QStringLiteral("A1"));
    model.addFixStation(QStringLiteral("A2"));
    REQUIRE(model.rowCount() == 2);

    SECTION("The named station's fix goes away") {
        model.removeFixStation(QStringLiteral("a1"));
        REQUIRE(model.rowCount() == 1);
        CHECK(model.fixStationAt(0).stationName() == QStringLiteral("A2"));
    }

    SECTION("An unfixed station is a no-op") {
        model.removeFixStation(QStringLiteral("A3"));
        CHECK(model.rowCount() == 2);
    }
}

TEST_CASE("cwFixStationModel reports every change to the set of fixed stations",
          "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;
    QSignalSpy fixedSpy(&model, &cwFixStationModel::fixedStationsChanged);
    QSignalSpy countSpy(&model, &cwFixStationModel::countChanged);

    SECTION("Adding a fix") {
        model.addFixStation(QStringLiteral("A1"));
        CHECK(fixedSpy.count() == 1);
    }

    SECTION("Removing a fix") {
        model.addFixStation(QStringLiteral("A1"));
        fixedSpy.clear();
        model.removeFixStation(QStringLiteral("A1"));
        CHECK(fixedSpy.count() == 1);
    }

    SECTION("Renaming a fix, which leaves the row count alone") {
        model.addFixStation(QStringLiteral("A1"));
        fixedSpy.clear();
        countSpy.clear();

        model.setData(model.index(0), QStringLiteral("A2"),
                      cwFixStationModel::StationNameRole);

        // This is why countChanged() can't stand in: a rename changes which
        // station shows as fixed without changing how many fixes there are.
        CHECK(fixedSpy.count() == 1);
        CHECK(countSpy.count() == 0);
    }

    SECTION("Replacing the rows wholesale, even at the same count") {
        model.addFixStation(QStringLiteral("A1"));
        fixedSpy.clear();
        countSpy.clear();

        cwFixStation replacement;
        replacement.setStationName(QStringLiteral("A2"));
        model.setFixStations({replacement});

        CHECK(fixedSpy.count() == 1);
        CHECK(countSpy.count() == 0);
    }

    SECTION("Editing a coordinate is not a change to the set") {
        model.addFixStation(QStringLiteral("A1"));
        fixedSpy.clear();

        model.setData(model.index(0), 100.0, cwFixStationModel::EastingRole);
        CHECK(fixedSpy.count() == 0);
    }
}

TEST_CASE("cwFixStations and globalCS survive a project save/load",
          "[FixStation][cwSaveLoad]") {
    // Build a project with two caves: one with two fixes, one with none.
    // Set globalCS on the region. Save to a temp dir, reload into a fresh
    // project, and verify everything came back intact. worldOrigin is not
    // persisted — it's a derived centroid recomputed on the first
    // line-plot completion of each session.
    auto creatorRoot = std::make_unique<cwRootData>();
    auto creatorProject = creatorRoot->project();
    auto creatorRegion = creatorProject->cavingRegion();

    creatorRegion->geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));

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

    CHECK(loadedRegion->geoReference()->globalCoordinateSystem() == QStringLiteral("EPSG:32612"));

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

TEST_CASE("Changing globalCS marks the project modified",
          "[FixStation][cwSaveLoad][globalCS]") {
    // Regression: cwSaveLoad::connectObjects() wires cave / trip / note /
    // sketch signals but does not listen to cwCavingRegion::globalCSChanged.
    // Without that, editing the region CS in an open project leaves
    // cwProject::modified() == false, so an autosave / commit pipeline keyed
    // off the dirty bit never runs and the change can be dropped on close.
    //
    // Standalone cwProject (no cwRootData) so subsystems like
    // cwLinePlotManager don't trigger spurious auto-saves that would dirty
    // the baseline before we make our own change.
    QQuickGit::Account account;
    account.setName(QStringLiteral("Test"));
    account.setEmail(QStringLiteral("test@example.com"));

    auto project = std::make_unique<cwProject>();
    addTokenManager(project.get());
    project->setGitAccount(&account);

    auto region = project->cavingRegion();
    region->addCave();
    region->cave(0)->setName(QStringLiteral("Cave"));

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("globalcs-dirty-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(project->saveAs(projectPath));
    project->waitSaveToFinish();
    REQUIRE(project->save());
    project->waitSaveToFinish();

    REQUIRE(isProjectModified(project.get()) == false);

    region->geoReference()->setGlobalCoordinateSystem(QStringLiteral("EPSG:32612"));
    CHECK(isProjectModified(project.get()));
}

TEST_CASE("cwFixStation proto round-trip preserves all fields", "[FixStation][proto]") {
    cwFixStation original;
    original.setStationName(QStringLiteral("A1"));
    original.setInputCS(QStringLiteral("EPSG:4326"));
    original.setCoordinate(QStringLiteral("46.12113, -115.59902, 30ft"));
    original.setHorizontalVariance(0.5);
    original.setVerticalVariance(1.0);

    CavewhereProto::FixStation proto;
    cwProtoUtils::saveFixStation(&proto, original);

    //The axis order is written for the benefit of a build that still reads it,
    //and nothing on the way back in checks it — so it is asserted here or not
    //at all. A geographic row is stored latitude-first.
    CHECK(proto.coordinatetextaxisorder()
          == CavewhereProto::FixStation_AxisOrder_LatitudeLongitude);

    cwFixStation restored = cwProtoUtils::fromProtoFixStation(proto);

    CHECK(restored.id() == original.id());
    CHECK(restored.stationName() == original.stationName());
    CHECK(restored.inputCS() == original.inputCS());
    CHECK(restored.easting() == original.easting());
    CHECK(restored.northing() == original.northing());
    CHECK(restored.elevation() == original.elevation());
    CHECK(restored.horizontalVariance() == original.horizontalVariance());
    CHECK(restored.verticalVariance() == original.verticalVariance());
    CHECK(restored.coordinate() == original.coordinate());
    CHECK(restored == original);
}

TEST_CASE("cwFixStation loads from a project written before the coordinate was a string",
          "[FixStation][proto]") {
    //Every fix in every project saved so far is three numbers and no string.
    //They become a coordinate spelled out from those numbers, in metres, and
    //land on exactly the doubles they were saved as.
    CavewhereProto::FixStation proto;
    proto.set_inputcs("EPSG:32611");
    proto.set_easting(610016.792);
    proto.set_northing(5615117.075);
    proto.set_elevation(304.0);

    const cwFixStation restored = cwProtoUtils::fromProtoFixStation(proto);
    CHECK(restored.easting() == 610016.792);
    CHECK(restored.northing() == 5615117.075);
    CHECK(restored.elevation() == 304.0);
    CHECK(restored.coordinate() == QStringLiteral("610016.792, 5615117.075, 304m"));
}

TEST_CASE("cwFixStation keeps the numbers of a row that never declared a coordinate system",
          "[FixStation][proto]") {
    //The old picker's "Local" wrote a blank input CS, so rows like this exist.
    //There is no axis order to read them under, so they come back with nothing
    //derived — but the numbers themselves must survive as the text they were,
    //or opening and saving a project would quietly empty the row. The user
    //picks the system the row was always in and the coordinate is there.
    CavewhereProto::FixStation proto;
    proto.set_easting(610016.792);
    proto.set_northing(5615117.075);
    proto.set_elevation(304.0);

    const cwFixStation restored = cwProtoUtils::fromProtoFixStation(proto);
    CHECK(restored.state() == cwFixStation::NoSystem);
    CHECK(restored.coordinate() == QStringLiteral("610016.792, 5615117.075, 304m"));
    CHECK(restored.easting() == 0.0);

    //Naming the system it was written in is all it takes to get them back.
    cwFixStation named = restored;
    named.setInputCS(QStringLiteral("EPSG:32611"));
    CHECK(named.state() == cwFixStation::Valid);
    CHECK(named.easting() == 610016.792);
    CHECK(named.northing() == 5615117.075);
    CHECK(named.elevation() == 304.0);
}

TEST_CASE("cwFixStation loads a row nobody ever filled in as having no coordinate",
          "[FixStation][proto]") {
    //"Mark Station as Fixed" writes a row before the user types anything, so
    //three zeros with no string is *not entered* — spelling it out as
    //"0, 0, 0m" would make it *entered at the origin*, and the diagnostics
    //defer on exactly that difference.
    CavewhereProto::FixStation proto;
    proto.set_easting(0.0);
    proto.set_northing(0.0);
    proto.set_elevation(0.0);

    const cwFixStation restored = cwProtoUtils::fromProtoFixStation(proto);
    CHECK(restored.coordinate().isEmpty());
    CHECK(restored.state() == cwFixStation::Empty);
}

TEST_CASE("cwFixStation load prefers the numbers over a string that disagrees with them",
          "[FixStation][proto]") {
    //Until the one-shot migration runs, a saved fix carries both — and the
    //numbers are what the project has meant all along. A string that no longer
    //describes them (typed before its CS was corrected, or a two-component
    //paste that left an elevation standing beside it) must not be allowed to
    //move the station on the way in.
    SECTION("a string read under another axis order than the row's CS now implies") {
        //The shape this branch exists for: the text was typed while the row was
        //projected, so it leads with the easting, and the row is geographic
        //now. Read latitude-first it says something else entirely, and the
        //numbers are what the project has meant all along.
        CavewhereProto::FixStation proto;
        cwProtoUtils::saveString(proto.mutable_inputcs(), QStringLiteral("EPSG:4326"));
        proto.set_easting(-115.59902);
        proto.set_northing(46.12113);
        proto.set_elevation(304.0);
        cwProtoUtils::saveString(proto.mutable_coordinatetext(),
                   QStringLiteral("-115.59902, 46.12113, 304m"));

        const cwFixStation restored = cwProtoUtils::fromProtoFixStation(proto);
        CHECK(restored.easting() == -115.59902);
        CHECK(restored.northing() == 46.12113);
        //Re-spelled in the order the row now reads, rather than kept as text
        //that would put the station somewhere else.
        CHECK(restored.coordinate() == QStringLiteral("46.12113, -115.59902, 304m"));
    }

    SECTION("a two-component string beside an elevation the row still has") {
        CavewhereProto::FixStation proto;
        cwProtoUtils::saveString(proto.mutable_inputcs(), QStringLiteral("EPSG:32611"));
        proto.set_easting(1.0);
        proto.set_northing(2.0);
        proto.set_elevation(304.0);
        cwProtoUtils::saveString(proto.mutable_coordinatetext(), QStringLiteral("1, 2"));

        const cwFixStation restored = cwProtoUtils::fromProtoFixStation(proto);
        CHECK(restored.elevation() == 304.0);
        CHECK(restored.hasElevation());
    }

    SECTION("but a string that does describe them is kept, word for word") {
        //The common case, and the one the others must not swallow: a coordinate
        //the user wrote comes back as they wrote it, feet and all.
        CavewhereProto::FixStation proto;
        cwProtoUtils::saveString(proto.mutable_inputcs(), QStringLiteral("EPSG:32611"));
        proto.set_easting(1.0);
        proto.set_northing(2.0);
        proto.set_elevation(30.0 * 0.3048);
        cwProtoUtils::saveString(proto.mutable_coordinatetext(), QStringLiteral("1, 2, 30ft"));

        CHECK(cwProtoUtils::fromProtoFixStation(proto).coordinate()
              == QStringLiteral("1, 2, 30ft"));
    }

    SECTION("and a string with no numbers beside it is kept even when it can't be read") {
        //The hand-edited file. There are no numbers to fall back to, so the
        //text is all there is — dropping it would leave the row saying nothing
        //about a coordinate its owner clearly meant to write.
        CavewhereProto::FixStation proto;
        cwProtoUtils::saveString(proto.mutable_inputcs(), QStringLiteral("EPSG:32611"));
        cwProtoUtils::saveString(proto.mutable_coordinatetext(),
                                 QStringLiteral("N 46 07 16 W 115 35 56"));

        const cwFixStation restored = cwProtoUtils::fromProtoFixStation(proto);
        CHECK(restored.coordinate() == QStringLiteral("N 46 07 16 W 115 35 56"));
        CHECK(restored.state() == cwFixStation::Unreadable);

        //And it survives being written back out, which is the round trip a
        //project makes every time it is opened and saved.
        CavewhereProto::FixStation again;
        cwProtoUtils::saveFixStation(&again, restored);
        CHECK(cwProtoUtils::fromProtoFixStation(again).coordinate()
              == QStringLiteral("N 46 07 16 W 115 35 56"));
    }
}

TEST_CASE("Loading a project does not re-save the cave file",
          "[FixStation][cwSaveLoad]") {
    // Regression guard: cwSaveLoad::connectCave wires fix-station model
    // signals (rowsInserted/rowsRemoved/modelReset/dataChanged) to a per-cave
    // save lambda. If connectCave were ever invoked before the cave's data
    // was populated during load, the modelReset emitted by setFixStations()
    // would fire that lambda and rewrite the cave's .cwcave file mid-load.
    // This test asserts the cave file is byte-identical (mtime unchanged)
    // after a clean load.
    auto creatorRoot = std::make_unique<cwRootData>();
    auto creatorProject = creatorRoot->project();
    auto creatorRegion = creatorProject->cavingRegion();

    creatorRegion->addCave();
    auto cave = creatorRegion->cave(0);
    REQUIRE(cave != nullptr);
    cave->setName(QStringLiteral("Cave with fix"));

    cwFixStation a;
    a.setStationName(QStringLiteral("A1"));
    a.setInputCS(QStringLiteral("EPSG:32612"));
    a.setEasting(500123.456);
    a.setNorthing(4194567.89);
    a.setElevation(2750.5);
    cave->fixStations()->setFixStations({a});

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString projectPath = QDir(tempDir.path())
                                    .filePath(QStringLiteral("fixstations-load-stable-%1.cwproj")
                                                  .arg(QCoreApplication::applicationPid()));
    REQUIRE(creatorProject->saveAs(projectPath));
    creatorRoot->futureManagerModel()->waitForFinished();
    creatorProject->waitSaveToFinish();

    const QString savedProjectFile = creatorProject->filename();
    REQUIRE(QFileInfo::exists(savedProjectFile));

    // Locate the cave's .cwcave file on disk.
    const QDir projectDir(QFileInfo(savedProjectFile).absoluteDir());
    QFileInfoList caveFiles;
    QDirIterator it(projectDir.absolutePath(),
                    QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QDir d(it.filePath());
        caveFiles.append(d.entryInfoList(QStringList() << QStringLiteral("*.cwcave"), QDir::Files));
    }
    REQUIRE(caveFiles.size() == 1);
    const QString caveFilePath = caveFiles.first().absoluteFilePath();
    const QDateTime mtimeBeforeLoad = QFileInfo(caveFilePath).lastModified();

    // Sleep long enough that any save during load would produce a distinct
    // mtime even on coarse-resolution filesystems (HFS+ is 1s; APFS / ext4 are
    // sub-millisecond, so 50 ms covers either).
    QThread::msleep(50);

    // Load into a fresh project; flush any saves that might have been queued.
    auto loaderRoot = std::make_unique<cwRootData>();
    auto loaderProject = loaderRoot->project();
    addTokenManager(loaderProject);
    loaderProject->loadOrConvert(savedProjectFile);
    loaderRoot->futureManagerModel()->waitForFinished();
    loaderProject->waitLoadToFinish();
    loaderProject->waitSaveToFinish();

    const QDateTime mtimeAfterLoad = QFileInfo(caveFilePath).lastModified();
    CHECK(mtimeAfterLoad == mtimeBeforeLoad);
}

TEST_CASE("cwFixStationModel setCoordinateText writes the whole coordinate at once",
          "[FixStation][cwFixStationModel]") {
    cwFixStationModel model;
    model.addFixStation();
    REQUIRE(model.rowCount() == 1);
    //A projected system, so the coordinates below read easting-first. New rows
    //start on WGS84, where the same text would lead with the latitude.
    model.setData(model.index(0), QStringLiteral("EPSG:32611"),
                  cwFixStationModel::InputCSRole);

    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

    SECTION("a pasted coordinate lands in all three components") {
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                      cwUnits::Metric) == QString());

        const cwFixStation fix = model.fixStationAt(0);
        CHECK(fix.easting() == 46.12113);
        CHECK(fix.northing() == -115.59902);
        CHECK(fix.elevation() == 304.0);
    }

    SECTION("a feet suffix is converted, because nothing downstream carries a unit") {
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304ft"),
                                      cwUnits::Metric) == QString());
        CHECK(model.fixStationAt(0).elevation() == 304.0 * 0.3048);
    }

    SECTION("a bare elevation follows the unit system it is given") {
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304"),
                                      cwUnits::Imperial) == QString());
        CHECK(model.fixStationAt(0).elevation() == 304.0 * 0.3048);
    }

    SECTION("one dataChanged covers all three, so the line plot re-solves once") {
        //cwLinePlotManager reruns survex on every dataChanged from this model.
        //Three setData() calls would be three solves for one paste.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric) == QString());

        REQUIRE(dataChangedSpy.count() == 1);
        const auto roles = dataChangedSpy.first().at(2).value<QList<int>>();
        CHECK(roles.size() == 4);
        CHECK(roles.contains(cwFixStationModel::EastingRole));
        CHECK(roles.contains(cwFixStationModel::NorthingRole));
        CHECK(roles.contains(cwFixStationModel::ElevationRole));
        //The coordinate itself rides along, so an editor bound to it re-reads in
        //the same pass rather than offering the previous one.
        CHECK(roles.contains(cwFixStationModel::CoordinateTextRole));

        const auto topLeft = dataChangedSpy.first().at(0).toModelIndex();
        const auto bottomRight = dataChangedSpy.first().at(1).toModelIndex();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == 0);
    }

    SECTION("a rewording moves the coordinate role and nothing else") {
        //Re-spacing a coordinate changes how it reads, not where the station
        //is, and the emission has to say so — a consumer that filters on the
        //components should see nothing here. (cwLinePlotManager doesn't filter
        //and re-solves regardless; that is its business, not this contract's.)
        REQUIRE(model.setCoordinateText(0, QStringLiteral("1, 2, 3m"),
                                        cwUnits::Metric) == QString());
        dataChangedSpy.clear();

        CHECK(model.setCoordinateText(0, QStringLiteral("1,2,3m"), cwUnits::Metric) == QString());
        CHECK(model.fixStationAt(0).coordinate() == QStringLiteral("1,2,3m"));

        REQUIRE(dataChangedSpy.count() == 1);
        const auto roles = dataChangedSpy.first().at(2).value<QList<int>>();
        CHECK(roles == QList<int>{cwFixStationModel::CoordinateTextRole});
    }

    SECTION("a coordinate this surface accepts is always one the row can read back") {
        //The text is stored and the numbers are read back out of it, so anything
        //stored has to parse. A trailing separator is legal on the way in, and
        //spelling the elevation's unit out by appending to it would produce
        //"…304,m" — text that parses on this side of the write and not on the
        //other, leaving an accepted coordinate sitting on a row at 0, 0, 0.
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304,"),
                                      cwUnits::Metric) == QString());

        const cwFixStation fix = model.fixStationAt(0);
        CHECK(fix.state() == cwFixStation::Valid);
        CHECK(fix.easting() == 46.12113);
        CHECK(fix.elevation() == 304.0);
    }

    SECTION("text that won't parse is refused with a reason and changes nothing") {
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric) == QString());
        dataChangedSpy.clear();

        const QString error = model.setCoordinateText(0, QStringLiteral("somewhere over there"),
                                                      cwUnits::Metric);
        CHECK_FALSE(error.isEmpty());

        //A rejected paste must not half-write the row. This surface is where a
        //coordinate is judged: unreadable text only ever arrives from a file
        //someone edited by hand, and there it is kept.
        CHECK(model.fixStationAt(0).coordinate()
              == QStringLiteral("46.12113, -115.59902, 304m"));
        CHECK(model.fixStationAt(0).easting() == 46.12113);
        CHECK(dataChangedSpy.count() == 0);
    }

    SECTION("re-committing the same coordinate emits nothing") {
        //The one field is both display and input, so pressing Enter without
        //editing is the common case — it must not dirty the project.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric) == QString());
        dataChangedSpy.clear();

        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                      cwUnits::Metric) == QString());
        CHECK(dataChangedSpy.count() == 0);
    }

    SECTION("the axis order comes from the row's own CS") {
        //Without this the model could read every row easting-first and every
        //test above would still pass, while a lat/long row put its latitude in
        //the easting and plotted the cave in the Gulf of Guinea.
        model.setData(model.index(0), QStringLiteral("EPSG:4326"),
                      cwFixStationModel::InputCSRole);
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                      cwUnits::Metric) == QString());

        const cwFixStation fix = model.fixStationAt(0);
        CHECK(fix.northing() == 46.12113);
        CHECK(fix.easting() == -115.59902);
    }

    SECTION("a second, different write is not swallowed by the no-op guard") {
        //Every other successful write here starts from a fresh, empty row, so a
        //guard that returned early on any *one* matching component would pass
        //them all. Editing just the elevation is the case that catches it.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric) == QString());
        dataChangedSpy.clear();

        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 305m"),
                                      cwUnits::Metric) == QString());
        CHECK(model.fixStationAt(0).elevation() == 305.0);
        CHECK(dataChangedSpy.count() == 1);
    }

    SECTION("opening the editor and leaving without typing writes nothing at all") {
        //The commonest gesture there is, and it has to stay free even in an
        //imperial project: the editor opens on the stored coordinate, and
        //handing that back must not dirty the project or re-solve the line
        //plot. Round-tripping the *rendered* value would not be free — m→ft→m
        //lands one ulp out for about an eighth of all doubles — which is why
        //what the editor offers is the string itself.
        //Stored in millimeters so that the string is its own answer and not any
        //rendering of the row: a model that handed back format() instead of the
        //text would offer "…, 1m" or "…, 3.280839895013123ft" here, and this
        //section would catch it.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 1000mm"),
                                        cwUnits::Metric) == QString());
        const cwFixStation before = model.fixStationAt(0);

        //The premise, asserted rather than assumed: the rendered round trip
        //really is lossy here, so this is a trap a numeric guard would miss.
        const QString rendered = cwCoordinateText::format(before.easting(), before.northing(),
                                                          before.elevation(), cwUnits::Imperial,
                                                          cwCoordinateText::EastingNorthing);
        const auto reparsed = cwCoordinateText::parse(rendered, cwUnits::Imperial,
                                                      cwCoordinateText::EastingNorthing);
        REQUIRE_FALSE(reparsed.hasError());
        REQUIRE(reparsed.value().elevation != before.elevation());

        dataChangedSpy.clear();
        const QString offered = model.data(model.index(0),
                                           cwFixStationModel::CoordinateTextRole).toString();
        //What makes the round trip free is that this is the text, not a render
        //of the numbers — the premise above is what a render would have cost.
        REQUIRE(offered == QStringLiteral("46.12113, -115.59902, 1000mm"));
        REQUIRE(offered != rendered);

        CHECK(model.setCoordinateText(0, offered, cwUnits::Imperial) == QString());
        CHECK(model.fixStationAt(0).elevation() == before.elevation());
        CHECK(dataChangedSpy.count() == 0);
    }

    SECTION("a two-component paste says the elevation was never entered") {
        //"46.12113, -115.59902" is the shape a coordinate copied from a map
        //arrives in. It says nothing about elevation — so the row says nothing
        //about it either, rather than claiming sea level.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric) == QString());
        REQUIRE(model.fixStationAt(0).hasElevation());

        CHECK(model.setCoordinateText(0, QStringLiteral("46.2, -115.6"),
                                      cwUnits::Metric) == QString());
        CHECK(model.fixStationAt(0).easting() == 46.2);
        CHECK_FALSE(model.fixStationAt(0).hasElevation());
        CHECK(model.fixStationAt(0).elevation() == 0.0);

        //And entering one stays a matter of saying so.
        CHECK(model.setCoordinateText(0, QStringLiteral("46.2, -115.6, 0"),
                                      cwUnits::Metric) == QString());
        CHECK(model.fixStationAt(0).hasElevation());
    }

    SECTION("a row that doesn't exist is a silent no-op") {
        //Out of range is a programming error, not something to show the user.
        CHECK(model.setCoordinateText(5, QStringLiteral("1, 2, 3m"),
                                      cwUnits::Metric) == QString());
        CHECK(model.setCoordinateText(-1, QStringLiteral("1, 2, 3m"),
                                      cwUnits::Metric) == QString());
        CHECK(dataChangedSpy.count() == 0);
    }
}

TEST_CASE("cwFixStationModel setCoordinateText writes the row it was given",
          "[FixStation][cwFixStationModel]") {
    //With one row in the model, hardcoding index 0 in the body passes every
    //other case here. Three rows is what makes `row` load-bearing: a user
    //editing the third fix would otherwise move the first station's anchor,
    //and with it the whole cave.
    cwFixStationModel model;
    REQUIRE(model.addFixStation(QStringLiteral("A1")) == 0);
    REQUIRE(model.addFixStation(QStringLiteral("A2")) == 1);
    REQUIRE(model.addFixStation(QStringLiteral("A3")) == 2);
    model.setData(model.index(2), QStringLiteral("EPSG:32611"),
                  cwFixStationModel::InputCSRole);

    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
    REQUIRE(model.setCoordinateText(2, QStringLiteral("11, 22, 33m"),
                                    cwUnits::Metric) == QString());

    CHECK(model.fixStationAt(2).easting() == 11.0);
    CHECK(model.fixStationAt(0).easting() == 0.0);
    CHECK(model.fixStationAt(1).easting() == 0.0);

    REQUIRE(dataChangedSpy.count() == 1);
    CHECK(dataChangedSpy.first().at(0).toModelIndex().row() == 2);
}

TEST_CASE("cwFixStationModel keeps the coordinate as it was written",
          "[FixStation][cwFixStationModel]") {
    //The cell renders every row in the project's units so a column of fixes can
    //be scanned; the editor re-offers what was written. Asserting only one of
    //the two lets the split be implemented as a no-op in the other.
    cwFixStationModel model;
    model.addFixStation();
    REQUIRE(model.rowCount() == 1);

    const QModelIndex idx = model.index(0);
    //A projected system, so the coordinates below read easting-first. New rows
    //start on WGS84, where the same text would lead with the latitude.
    model.setData(idx, QStringLiteral("EPSG:32611"), cwFixStationModel::InputCSRole);

    SECTION("the editor gets the user's string, the cell the project's units") {
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 30ft"),
                                        cwUnits::Metric) == QString());

        const cwFixStation fix = model.fixStationAt(0);
        CHECK(fix.elevation() == 30.0 * 0.3048);
        CHECK(fix.coordinate() == QStringLiteral("46.12113, -115.59902, 30ft"));
        CHECK(model.data(idx, cwFixStationModel::CoordinateTextRole).toString()
              == QStringLiteral("46.12113, -115.59902, 30ft"));
        CHECK(cwCoordinateText::format(fix.easting(), fix.northing(), fix.elevation(),
                                       cwUnits::Metric, cwCoordinateText::EastingNorthing)
              == QStringLiteral("46.12113, -115.59902, 9.144m"));
    }

    SECTION("a bare elevation is stored with the unit it was read in") {
        //"304" means "304 in the project's units" at the moment it is typed.
        //Kept verbatim it would quietly become 304 ft — a 213 m drop — the first
        //time the project switched to imperial and the string was read again.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304"),
                                        cwUnits::Metric) == QString());
        CHECK(model.fixStationAt(0).coordinate()
              == QStringLiteral("46.12113, -115.59902, 304m"));

        //And re-committing what was typed is still a no-op, because it
        //normalizes to the string already there.
        QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304"),
                                      cwUnits::Metric) == QString());
        CHECK(dataChangedSpy.count() == 0);
    }

    SECTION("a component written by any other path writes the coordinate back out") {
        //There is one place a coordinate lives now, so a number set directly —
        //an import, a single-cell edit — has to land there too.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("1, 2, 30ft"),
                                        cwUnits::Metric) == QString());

        QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
        model.setData(idx, 500.0, cwFixStationModel::ElevationRole);

        CHECK(model.fixStationAt(0).coordinate() == QStringLiteral("1, 2, 500m"));
        //A view showing the string has to hear that it changed.
        REQUIRE(dataChangedSpy.count() == 1);
        const auto roles = dataChangedSpy.first().at(2).value<QList<int>>();
        //The whole list, not just what it contains: the easting and northing
        //didn't move, and an emission that named them anyway would pass a
        //contains() check while telling every consumer the station did.
        CHECK(roles.size() == 2);
        CHECK(roles.contains(cwFixStationModel::ElevationRole));
        CHECK(roles.contains(cwFixStationModel::CoordinateTextRole));
    }

    SECTION("a name or variance edit keeps it, because neither is a component") {
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 30ft"),
                                        cwUnits::Metric) == QString());

        model.setData(idx, QStringLiteral("A1"), cwFixStationModel::StationNameRole);
        model.setData(idx, 0.5, cwFixStationModel::HorizontalVarianceRole);
        CHECK(model.fixStationAt(0).coordinate()
              == QStringLiteral("46.12113, -115.59902, 30ft"));
    }

    SECTION("changing between two projected systems moves nothing") {
        //The wrong-UTM-zone case: same axis order, so there is nothing to read
        //differently, and the string is left exactly as it was written.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("610016.792, 5615117.075, 304m"),
                                        cwUnits::Metric) == QString());
        model.setData(idx, QStringLiteral("EPSG:32611"), cwFixStationModel::InputCSRole);
        const cwFixStation before = model.fixStationAt(0);

        QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
        model.setData(idx, QStringLiteral("EPSG:32613"), cwFixStationModel::InputCSRole);

        CHECK(model.fixStationAt(0).coordinate() == before.coordinate());
        CHECK(model.fixStationAt(0).easting() == before.easting());
        CHECK(model.fixStationAt(0).northing() == before.northing());
        CHECK(model.fixStationAt(0).elevation() == before.elevation());

        REQUIRE(dataChangedSpy.count() == 1);
        const auto roles = dataChangedSpy.first().at(2).value<QList<int>>();
        CHECK(roles == QList<int>{cwFixStationModel::InputCSRole});
    }

    SECTION("changing to a geographic one re-reads the coordinate, and says so") {
        //A user correcting the system is telling us how to read what they
        //already typed. The horizontals swap, and the roles that carry them have
        //to go out or the row would show one coordinate and solve another.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric) == QString());
        REQUIRE(model.fixStationAt(0).easting() == 46.12113);

        QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
        model.setData(idx, QStringLiteral("EPSG:4326"), cwFixStationModel::InputCSRole);

        CHECK(model.fixStationAt(0).northing() == 46.12113);
        CHECK(model.fixStationAt(0).easting() == -115.59902);
        //Untouched: re-reading a string is not rewriting it.
        CHECK(model.fixStationAt(0).coordinate()
              == QStringLiteral("46.12113, -115.59902, 304m"));

        REQUIRE(dataChangedSpy.count() == 1);
        const auto roles = dataChangedSpy.first().at(2).value<QList<int>>();
        //Sized as well as named: the elevation is the same 304 either way round,
        //so an emission that swept it in with the rest would still satisfy every
        //contains() below.
        CHECK(roles.size() == 3);
        CHECK(roles.contains(cwFixStationModel::InputCSRole));
        CHECK(roles.contains(cwFixStationModel::EastingRole));
        CHECK(roles.contains(cwFixStationModel::NorthingRole));
        CHECK_FALSE(roles.contains(cwFixStationModel::CoordinateTextRole));
    }

    SECTION("a row nobody typed into has no coordinate at all") {
        //"Mark Station as Fixed" creates the row before anything is entered, and
        //that is a different thing from a coordinate at the origin.
        CHECK(model.fixStationAt(0).state() == cwFixStation::Empty);
        CHECK(model.data(idx, cwFixStationModel::CoordinateTextRole).toString().isEmpty());

        //And it stays that way when the editor opens on the zeros the row
        //renders as and hands them straight back. Taking that would make an
        //unfilled row a fix at the origin, dirty the project and re-solve.
        QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
        CHECK(model.setCoordinateText(0, QStringLiteral("0, 0, 0m"),
                                      cwUnits::Metric) == QString());
        CHECK(model.fixStationAt(0).state() == cwFixStation::Empty);
        CHECK(dataChangedSpy.count() == 0);

        //Typed rather than offered back, it is a coordinate like any other.
        CHECK(model.setCoordinateText(0, QStringLiteral("0, 0, 1m"),
                                      cwUnits::Metric) == QString());
        CHECK(model.fixStationAt(0).state() == cwFixStation::Valid);
    }
}

TEST_CASE("cwFixStationModel setCoordinateText leaves the station name alone",
          "[FixStation][cwFixStationModel]") {
    //The coordinate field sits next to the name field; writing one must not
    //disturb the other, and StationNameRole must stay out of the emission so
    //fixedStationsChanged() doesn't fire for a coordinate edit.
    cwFixStationModel model;
    REQUIRE(model.addFixStation(QStringLiteral("A1")) == 0);

    QSignalSpy fixedStationsSpy(&model, &cwFixStationModel::fixedStationsChanged);
    REQUIRE(model.setCoordinateText(0, QStringLiteral("1, 2, 3m"), cwUnits::Metric) == QString());

    CHECK(model.fixStationAt(0).stationName() == QStringLiteral("A1"));
    CHECK(fixedStationsSpy.count() == 0);
}

TEST_CASE("cwFixStationModel gives a new row a coordinate system to start on",
          "[FixStation][cwFixStationModel]") {
    // Without one there is no axis order, so the row could not read a coordinate
    // back at all (#625). Both entry points have to agree, since "Mark Station as
    // Fixed" comes in through the named overload and the page's "+" through the
    // other. Deliberately not cwFixStation's own default: a row that really does
    // arrive blank, from an old file or a hand edit, must stay blank so it can be
    // flagged rather than silently reinterpreted.
    cwFixStationModel model;

    model.addFixStation();
    REQUIRE(model.count() == 1);
    CHECK(model.fixStationAt(0).inputCS() == cwCoordinateTransform::Wgs84);

    model.addFixStation(QStringLiteral("A1"));
    REQUIRE(model.count() == 2);
    CHECK(model.fixStationAt(1).stationName() == QStringLiteral("A1"));
    CHECK(model.fixStationAt(1).inputCS() == cwCoordinateTransform::Wgs84);

    // A default-constructed fix is still blank, which is what makes NoSystem a
    // state that arrives from elsewhere rather than one the app can create.
    CHECK(cwFixStation().inputCS().isEmpty());
}
