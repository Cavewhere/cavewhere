//Our includes
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
#include <catch2/catch_approx.hpp>
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
    original.setInputCS(QStringLiteral("EPSG:32612"));
    original.setEasting(500123.456789);
    original.setNorthing(4194567.891234);
    original.setElevation(2750.5);
    original.setHorizontalVariance(0.5);
    original.setVerticalVariance(1.0);
    original.setCoordinateText(QStringLiteral("46.12113, -115.59902, 30ft"),
                               cwCoordinateText::LatitudeLongitude);

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
    CHECK(restored.coordinateText() == original.coordinateText());
    CHECK(restored.coordinateTextAxisOrder() == original.coordinateTextAxisOrder());
}

TEST_CASE("cwFixStation loads from a project written before it kept the typed string",
          "[FixStation][proto]") {
    //No migration: a fix with no stored text renders from its numbers, which is
    //every fix in every project saved so far. The other direction costs nothing
    //either — a new build's fix opened by an old one loses the display string
    //and keeps the coordinate.
    CavewhereProto::FixStation proto;
    proto.set_easting(610016.792);
    proto.set_northing(5615117.075);
    proto.set_elevation(304.0);

    const cwFixStation restored = cwProtoUtils::fromProtoFixStation(proto);
    CHECK(restored.easting() == 610016.792);
    CHECK(restored.northing() == 5615117.075);
    CHECK(restored.elevation() == 304.0);
    CHECK(restored.coordinateText().isEmpty());
    CHECK(restored.coordinateTextAxisOrder() == cwCoordinateText::EastingNorthing);
}

TEST_CASE("cwFixStation proto round-trip keeps the typed string past the components",
          "[FixStation][proto]") {
    //Loading writes the three components, each of which drops the stored string
    //by design. Reading it back before them — or writing it first — would leave
    //every saved coordinate blank on the way in, and the fallback rendering
    //makes that silent: the field would still show the right numbers.
    cwFixStation original;
    original.setEasting(1.0);
    original.setNorthing(2.0);
    original.setElevation(3.0);
    original.setCoordinateText(QStringLiteral("1, 2, 3m"), cwCoordinateText::EastingNorthing);

    CavewhereProto::FixStation proto;
    cwProtoUtils::saveFixStation(&proto, original);
    CHECK(cwProtoUtils::fromProtoFixStation(proto).coordinateText()
          == QStringLiteral("1, 2, 3m"));
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

    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);

    SECTION("a pasted lat/long lands in all three roles") {
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                      cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());

        const cwFixStation fix = model.fixStationAt(0);
        CHECK(fix.easting() == Catch::Approx(46.12113));
        CHECK(fix.northing() == Catch::Approx(-115.59902));
        CHECK(fix.elevation() == Catch::Approx(304.0));
    }

    SECTION("a feet suffix is converted, because nothing downstream carries a unit") {
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304ft"),
                                      cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(model.fixStationAt(0).elevation() == Catch::Approx(304.0 * 0.3048));
    }

    SECTION("a bare elevation follows the unit system it is given") {
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304"),
                                      cwUnits::Imperial,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(model.fixStationAt(0).elevation() == Catch::Approx(304.0 * 0.3048));
    }

    SECTION("one dataChanged covers all three, so the line plot re-solves once") {
        //cwLinePlotManager reruns survex on every dataChanged from this model.
        //Three setData() calls would be three solves for one paste.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());

        REQUIRE(dataChangedSpy.count() == 1);
        const auto roles = dataChangedSpy.first().at(2).value<QList<int>>();
        CHECK(roles.size() == 4);
        CHECK(roles.contains(cwFixStationModel::EastingRole));
        CHECK(roles.contains(cwFixStationModel::NorthingRole));
        CHECK(roles.contains(cwFixStationModel::ElevationRole));
        //The string the user typed rides along, so an editor bound to it
        //re-reads in the same pass rather than offering the previous one.
        CHECK(roles.contains(cwFixStationModel::CoordinateTextRole));

        const auto topLeft = dataChangedSpy.first().at(0).toModelIndex();
        const auto bottomRight = dataChangedSpy.first().at(1).toModelIndex();
        CHECK(topLeft.row() == 0);
        CHECK(bottomRight.row() == 0);
    }

    SECTION("text that won't parse is refused with a reason and changes nothing") {
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());
        dataChangedSpy.clear();

        const QString error = model.setCoordinateText(0, QStringLiteral("somewhere over there"),
                                                      cwUnits::Metric,
                                                      cwCoordinateText::EastingNorthing);
        CHECK_FALSE(error.isEmpty());

        //A rejected paste must not half-write the row.
        CHECK(model.fixStationAt(0).easting() == Catch::Approx(46.12113));
        CHECK(model.fixStationAt(0).northing() == Catch::Approx(-115.59902));
        CHECK(model.fixStationAt(0).elevation() == Catch::Approx(304.0));
        CHECK(dataChangedSpy.count() == 0);
    }

    SECTION("re-committing the same coordinate emits nothing") {
        //The one field is both display and input, so pressing Enter without
        //editing is the common case — it must not dirty the project.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());
        dataChangedSpy.clear();

        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                      cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(dataChangedSpy.count() == 0);
    }

    SECTION("the axis order is forwarded, not assumed") {
        //Without this the model could hardcode EastingNorthing and every test
        //above would still pass, while a lat/long row put its latitude in the
        //easting and plotted the cave in the Gulf of Guinea.
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                      cwUnits::Metric,
                                      cwCoordinateText::LatitudeLongitude) == QString());

        const cwFixStation fix = model.fixStationAt(0);
        CHECK(fix.northing() == Catch::Approx(46.12113));
        CHECK(fix.easting() == Catch::Approx(-115.59902));
    }

    SECTION("a second, different write is not swallowed by the no-op guard") {
        //Every other successful write here starts from a fresh 0/0/0 row, so a
        //guard that returned early on any *one* matching component would pass
        //them all. Editing just the elevation is the case that catches it.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());
        dataChangedSpy.clear();

        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 305m"),
                                      cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(model.fixStationAt(0).elevation() == Catch::Approx(305.0));
        CHECK(dataChangedSpy.count() == 1);
    }

    SECTION("committing the row's own rendering never moves the numbers") {
        //format() renders the elevation in feet and parse() converts it back,
        //and (m/0.3048)*0.3048 lands one ulp away for about an eighth of all
        //doubles. Text that reads back as what the row already renders has to
        //leave every component exactly where it is rather than write the round
        //trip back — and it stores no string of its own, because a row holding
        //none renders that very text anyway.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 1m"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());
        dataChangedSpy.clear();

        const cwFixStation before = model.fixStationAt(0);
        const QString shown = cwCoordinateText::format(before.easting(), before.northing(),
                                                       before.elevation(), cwUnits::Imperial,
                                                       cwCoordinateText::EastingNorthing);
        //The premise: the round trip really is lossy here, so a numeric guard
        //alone could not have caught this.
        const auto reparsed = cwCoordinateText::parse(shown, cwUnits::Imperial,
                                                      cwCoordinateText::EastingNorthing);
        REQUIRE_FALSE(reparsed.hasError());
        REQUIRE(reparsed.value().elevation != before.elevation());

        CHECK(model.setCoordinateText(0, shown, cwUnits::Imperial,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(model.fixStationAt(0).elevation() == before.elevation());
        CHECK(model.fixStationAt(0).coordinateText() == QString());

        //Only the stored string moved, so nothing re-solves: the coordinate
        //roles must stay out of the emission.
        REQUIRE(dataChangedSpy.count() == 1);
        const auto roles = dataChangedSpy.first().at(2).value<QList<int>>();
        CHECK(roles == QList<int>{cwFixStationModel::CoordinateTextRole});
    }

    SECTION("opening the editor and leaving without typing writes nothing at all") {
        //The commonest gesture there is, and the one the whole no-op rule exists
        //for: the editor offers the row's own string, and handing it back
        //unchanged must not dirty the project or re-solve the line plot. Both
        //halves matter — a row that kept a string and a row that never had one
        //take different paths to the same answer.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 30ft"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());
        REQUIRE(model.fixStationAt(0).coordinateText()
                == QStringLiteral("46.12113, -115.59902, 30ft"));
        dataChangedSpy.clear();

        CHECK(model.setCoordinateText(0, model.fixStationAt(0).coordinateText(),
                                      cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(dataChangedSpy.count() == 0);

        //Now the same row with no string of its own — an imported fix, or one
        //loaded from a project written before this field existed. Its editor
        //opens on format(), so that is what comes back.
        model.setData(model.index(0), 305.0, cwFixStationModel::ElevationRole);
        REQUIRE(model.fixStationAt(0).coordinateText().isEmpty());
        dataChangedSpy.clear();

        const cwFixStation fix = model.fixStationAt(0);
        const QString offered = cwCoordinateText::format(fix.easting(), fix.northing(),
                                                         fix.elevation(), cwUnits::Metric,
                                                         cwCoordinateText::EastingNorthing);
        CHECK(model.setCoordinateText(0, offered, cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(dataChangedSpy.count() == 0);
        CHECK(model.fixStationAt(0).coordinateText().isEmpty());
    }

    SECTION("a two-component paste leaves the elevation it already had") {
        //"46.12113, -115.59902" is the shape a coordinate copied from a map
        //arrives in. It says nothing about elevation, so reading its absence as
        //zero would drop the cave by however deep the entrance was.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304m"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());

        CHECK(model.setCoordinateText(0, QStringLiteral("46.2, -115.6"), cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(model.fixStationAt(0).easting() == Catch::Approx(46.2));
        CHECK(model.fixStationAt(0).elevation() == Catch::Approx(304.0));

        //Clearing it stays possible, by saying so.
        CHECK(model.setCoordinateText(0, QStringLiteral("46.2, -115.6, 0"), cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(model.fixStationAt(0).elevation() == Catch::Approx(0.0));
    }

    SECTION("a row that doesn't exist is a silent no-op") {
        //Out of range is a programming error, not something to show the user.
        CHECK(model.setCoordinateText(5, QStringLiteral("1, 2, 3m"), cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(model.setCoordinateText(-1, QStringLiteral("1, 2, 3m"), cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());
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

    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
    REQUIRE(model.setCoordinateText(2, QStringLiteral("11, 22, 33m"), cwUnits::Metric,
                                    cwCoordinateText::EastingNorthing) == QString());

    CHECK(model.fixStationAt(2).easting() == Catch::Approx(11.0));
    CHECK(model.fixStationAt(0).easting() == Catch::Approx(0.0));
    CHECK(model.fixStationAt(1).easting() == Catch::Approx(0.0));

    REQUIRE(dataChangedSpy.count() == 1);
    CHECK(dataChangedSpy.first().at(0).toModelIndex().row() == 2);
}

TEST_CASE("cwFixStationModel keeps the string the coordinate was typed as",
          "[FixStation][cwFixStationModel]") {
    //U14. The cell renders every row in the project's units so a column of
    //fixes can be scanned; the editor re-offers what was written. Asserting
    //only one of the two lets the split be implemented as a no-op in the other.
    cwFixStationModel model;
    model.addFixStation();
    REQUIRE(model.rowCount() == 1);

    const QModelIndex idx = model.index(0);

    SECTION("the editor gets the user's string, the cell the project's units") {
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 30ft"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());

        const cwFixStation fix = model.fixStationAt(0);
        CHECK(fix.elevation() == Catch::Approx(30.0 * 0.3048));
        CHECK(fix.coordinateText() == QStringLiteral("46.12113, -115.59902, 30ft"));
        CHECK(model.data(idx, cwFixStationModel::CoordinateTextRole).toString()
              == QStringLiteral("46.12113, -115.59902, 30ft"));
        CHECK(cwCoordinateText::format(fix.easting(), fix.northing(), fix.elevation(),
                                       cwUnits::Metric, cwCoordinateText::EastingNorthing)
              == QStringLiteral("46.12113, -115.59902, 9.144m"));
    }

    SECTION("a bare elevation is stored with the unit it was read in") {
        //Trap 1: "304" means "304 in the project's units" at the moment it is
        //typed. Kept verbatim, it would quietly become 304 ft — a 213 m drop —
        //the first time the project switched to imperial and the user accepted
        //the string the editor offered them.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());
        CHECK(model.fixStationAt(0).coordinateText()
              == QStringLiteral("46.12113, -115.59902, 304m"));

        //And re-committing what was typed is still a no-op, because it
        //normalizes to the string already there.
        QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
        CHECK(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 304"),
                                      cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());
        CHECK(dataChangedSpy.count() == 0);
    }

    SECTION("a component written by any other path drops the string") {
        //The stored text is a claim about the numbers. The moment one moves
        //without going through here, the claim is false and the editor is
        //better off falling back to the row's own rendering.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 30ft"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());
        REQUIRE_FALSE(model.fixStationAt(0).coordinateText().isEmpty());

        QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
        model.setData(idx, 500.0, cwFixStationModel::ElevationRole);

        CHECK(model.fixStationAt(0).coordinateText().isEmpty());
        //A view showing the string has to hear that it went away.
        REQUIRE(dataChangedSpy.count() == 1);
        const auto roles = dataChangedSpy.first().at(2).value<QList<int>>();
        CHECK(roles.contains(cwFixStationModel::ElevationRole));
        CHECK(roles.contains(cwFixStationModel::CoordinateTextRole));
    }

    SECTION("a name or variance edit keeps it, because neither is a component") {
        REQUIRE(model.setCoordinateText(0, QStringLiteral("46.12113, -115.59902, 30ft"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());

        model.setData(idx, QStringLiteral("A1"), cwFixStationModel::StationNameRole);
        model.setData(idx, 0.5, cwFixStationModel::HorizontalVarianceRole);
        CHECK(model.fixStationAt(0).coordinateText()
              == QStringLiteral("46.12113, -115.59902, 30ft"));
    }

    SECTION("changing the coordinate system writes nothing") {
        //Trap 2. A user who pasted good numbers under the wrong CS fixes it by
        //correcting the CS — the numbers were right the whole time and only
        //ever needed reading under the right system. Any rule that rewrote the
        //text or the components here would move the fix out from under someone
        //who was correcting metadata, not data.
        REQUIRE(model.setCoordinateText(0, QStringLiteral("610016.792, 5615117.075, 304m"),
                                        cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());
        model.setData(idx, QStringLiteral("EPSG:32611"), cwFixStationModel::InputCSRole);
        const cwFixStation before = model.fixStationAt(0);

        //Projected to projected — the wrong-UTM-zone case, which has no axis
        //order question at all.
        model.setData(idx, QStringLiteral("EPSG:32613"), cwFixStationModel::InputCSRole);
        CHECK(model.fixStationAt(0).coordinateText() == before.coordinateText());
        CHECK(model.fixStationAt(0).easting() == before.easting());
        CHECK(model.fixStationAt(0).northing() == before.northing());
        CHECK(model.fixStationAt(0).elevation() == before.elevation());

        //And projected to geographic, where the axes do swap on screen. Still
        //nothing: only a commit ever changes a number.
        model.setData(idx, QStringLiteral("EPSG:4326"), cwFixStationModel::InputCSRole);
        CHECK(model.fixStationAt(0).coordinateText() == before.coordinateText());
        CHECK(model.fixStationAt(0).easting() == before.easting());
        CHECK(model.fixStationAt(0).northing() == before.northing());
        CHECK(model.fixStationAt(0).elevation() == before.elevation());
    }

    SECTION("re-committing the same string under a flipped axis order is a real edit") {
        //The trap the rule above creates. After the CS flips, the string in the
        //editor is unchanged but now means the other thing, and pressing Enter
        //on it is exactly how the user corrects the row. A no-op guard keyed on
        //the text alone would swallow that and leave them pressing Enter on a
        //warned fix while nothing happened.
        const QString typed = QStringLiteral("46.12113, -115.59902, 304m");
        REQUIRE(model.setCoordinateText(0, typed, cwUnits::Metric,
                                        cwCoordinateText::EastingNorthing) == QString());
        REQUIRE(model.fixStationAt(0).easting() == Catch::Approx(46.12113));

        QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
        CHECK(model.setCoordinateText(0, typed, cwUnits::Metric,
                                      cwCoordinateText::LatitudeLongitude) == QString());

        CHECK(model.fixStationAt(0).northing() == Catch::Approx(46.12113));
        CHECK(model.fixStationAt(0).easting() == Catch::Approx(-115.59902));
        CHECK(model.fixStationAt(0).coordinateText() == typed);
        CHECK(dataChangedSpy.count() == 1);

        //Committed under the new order, it is now the string this row holds —
        //so the *next* unchanged commit is a no-op again.
        dataChangedSpy.clear();
        CHECK(model.setCoordinateText(0, typed, cwUnits::Metric,
                                      cwCoordinateText::LatitudeLongitude) == QString());
        CHECK(dataChangedSpy.count() == 0);
    }

    SECTION("a row nobody typed into keeps no string") {
        //Imports and old projects land here: the numbers are set directly, and
        //the row renders from them forever after unless someone edits the field.
        model.setData(idx, 610016.792, cwFixStationModel::EastingRole);
        model.setData(idx, 5615117.075, cwFixStationModel::NorthingRole);
        CHECK(model.fixStationAt(0).coordinateText().isEmpty());

        model.setData(idx, QStringLiteral("EPSG:4326"), cwFixStationModel::InputCSRole);
        CHECK(model.fixStationAt(0).coordinateText().isEmpty());
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
    REQUIRE(model.setCoordinateText(0, QStringLiteral("1, 2, 3m"), cwUnits::Metric,
                                      cwCoordinateText::EastingNorthing) == QString());

    CHECK(model.fixStationAt(0).stationName() == QStringLiteral("A1"));
    CHECK(fixedStationsSpy.count() == 0);
}
