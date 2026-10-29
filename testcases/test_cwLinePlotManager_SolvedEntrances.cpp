/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwEntranceModel.h"
#include "cwError.h"
#include "cwErrorModel.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwFixStationModel.h"
#include "cwLinePlotManager.h"
#include "cwRootData.h"
#include "cwShot.h"
#include "cwSolvedEntrance.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"
#include "LoadProjectHelper.h"
#include "TestHelper.h"

// Qt
#include <QHash>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>

// Std
#include <optional>

namespace {

// Solved positions are floats read back from the .3d's centimeters.
constexpr double kPositionMarginMeters = 0.02;
// A re-solve runs cavern on the worker; a loaded machine can take a while.
constexpr int kSolveWaitMs = 20000;

const QString kSurvexEntranceFixture = QStringLiteral("survex_entrance.svx");
const QString kWallsEntranceFixture = QStringLiteral("walls_entrance.srv");
const QString kCompassProjectFixture = QStringLiteral("compass_project/PROJECT.MAK");

//! A native trip on \a cave surveying \a stations in one chunk, 10 m apart.
cwTrip* addNativeChain(cwCave* cave, const QStringList& stations)
{
    cwTrip* trip = addEmptyTrip(cave, QStringLiteral("Chain"));
    trip->calibrations()->setAutoDeclination(false);
    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);
    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("10.0")));
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    for (int i = 1; i < stations.size(); ++i) {
        chunk->appendShot(cwStation(stations.at(i - 1)), cwStation(stations.at(i)), shot);
    }
    return trip;
}

//! The solved entrance on \a stationName, matched case-insensitively.
std::optional<cwSolvedEntrance> entranceNamed(const QList<cwSolvedEntrance>& entrances,
                                              const QString& stationName)
{
    for (const cwSolvedEntrance& entrance : entrances) {
        if (entrance.stationName.compare(stationName, Qt::CaseInsensitive) == 0) {
            return entrance;
        }
    }
    return std::nullopt;
}

QStringList stationNames(const QList<cwSolvedEntrance>& entrances)
{
    QStringList names;
    for (const cwSolvedEntrance& entrance : entrances) {
        names.append(entrance.stationName);
    }
    return names;
}

void checkAt(const cwSolvedEntrance& entrance, const cwStationPositionLookup& lookup,
             const QString& station)
{
    INFO("station " << station.toStdString());
    REQUIRE(lookup.hasPosition(station));
    const QVector3D expected = lookup.position(station);
    CHECK(entrance.position.x() == Catch::Approx(expected.x()).margin(kPositionMarginMeters));
    CHECK(entrance.position.y() == Catch::Approx(expected.y()).margin(kPositionMarginMeters));
    CHECK(entrance.position.z() == Catch::Approx(expected.z()).margin(kPositionMarginMeters));
}

//! The EntranceUnplaced warning on \a node, if the last solve wrote one.
std::optional<cwError> unplacedEntranceWarning(const cwSurveyNode* node)
{
    const QList<cwError> errors = node->errorModel()->errors()->toList();
    for (const cwError& error : errors) {
        if (error.errorTypeId() == static_cast<int>(cwErrorTypeId::EntranceUnplaced)) {
            return error;
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("An attached Survex file's *entrance stations are solved entrances of its node",
          "[LinePlotManager][SolvedEntrances]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    QHash<QUuid, QString> caveDirs;
    cwCave* cave = addCaveAttachedTo(region, QStringLiteral("Doghill"), kSurvexEntranceFixture,
                                     tempRoot, caveDirs);

    cwLinePlotManager manager;
    manager.externalCenterlineManager()->setCaveAttachmentDirs(caveDirs);
    manager.setRegion(&region);
    manager.waitToFinish();
    INFO("driver:\n" << manager.driverSource().toStdString());
    INFO("cavern log:\n" << manager.cavernLog().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    const QList<cwSolvedEntrance> entrances = manager.solvedEntrances(cave->id());
    INFO("solved entrances: " << stationNames(entrances).join(QStringLiteral(", ")).toStdString());
    CHECK(entrances.size() == 2);
    CHECK(manager.solvedEntrances().value(cave->id()) == entrances);

    const std::optional<cwSolvedEntrance> fixedEntrance =
        entranceNamed(entrances, QStringLiteral("doghill.d1"));
    REQUIRE(fixedEntrance.has_value());
    CHECK(fixedEntrance->fixed);
    CHECK(fixedEntrance->qualifiedName.endsWith(QStringLiteral("doghill.d1"), Qt::CaseInsensitive));
    checkAt(*fixedEntrance, cave->scopedPositions(), QStringLiteral("doghill.d1"));

    const std::optional<cwSolvedEntrance> unfixedEntrance =
        entranceNamed(entrances, QStringLiteral("doghill.d3"));
    REQUIRE(unfixedEntrance.has_value());
    CHECK_FALSE(unfixedEntrance->fixed);
    checkAt(*unfixedEntrance, cave->scopedPositions(), QStringLiteral("doghill.d3"));

    // A fixed station is no entrance unless the data marks it one.
    CHECK_FALSE(entranceNamed(entrances, QStringLiteral("doghill.d4")).has_value());
    CHECK_FALSE(entranceNamed(entrances, QStringLiteral("doghill.d2")).has_value());

    CHECK(manager.isSolvedEntrance(cave->id(), QStringLiteral("doghill.d3")));
    CHECK(manager.isSolvedEntrance(cave->id(), QStringLiteral("DOGHILL.D1")));
    CHECK_FALSE(manager.isSolvedEntrance(cave->id(), QStringLiteral("doghill.d4")));
    CHECK_FALSE(manager.isSolvedEntrance(QUuid::createUuid(), QStringLiteral("doghill.d1")));
}

TEST_CASE("An attached Walls file's ENTRANCE flag is a solved entrance of its node",
          "[LinePlotManager][SolvedEntrances]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    QHash<QUuid, QString> caveDirs;
    cwCave* cave = addCaveAttachedTo(region, QStringLiteral("Walls"), kWallsEntranceFixture,
                                     tempRoot, caveDirs);

    cwLinePlotManager manager;
    manager.externalCenterlineManager()->setCaveAttachmentDirs(caveDirs);
    manager.setRegion(&region);
    manager.waitToFinish();
    INFO("driver:\n" << manager.driverSource().toStdString());
    INFO("cavern log:\n" << manager.cavernLog().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    // The #FIX on W1 marks no entrance; the flag on W3 does.
    const QList<cwSolvedEntrance> entrances = manager.solvedEntrances(cave->id());
    INFO("solved entrances: " << stationNames(entrances).join(QStringLiteral(", ")).toStdString());
    REQUIRE(entrances.size() == 1);
    CHECK(entrances.first().stationName.compare(QStringLiteral("W3"), Qt::CaseInsensitive) == 0);
    CHECK_FALSE(entrances.first().fixed);
    checkAt(entrances.first(), cave->scopedPositions(), entrances.first().stationName);
}

TEST_CASE("An attached Compass .mak's fixed points are solved entrances of its source root",
          "[LinePlotManager][SolvedEntrances]")
{
    auto project = makeSavedProject(QStringLiteral("compass-solved-entrances"),
                                    QStringLiteral("NativeCave"), QStringLiteral("NativeTrip"));
    cwSurveyNode* root = attachSourceThroughManager(
        project.get(), project->project->cavingRegion()->rootNode(),
        fixturePath(kCompassProjectFixture));
    drainPipelines(project.get());

    const cwLinePlotManager* manager = project->rootData->linePlotManager();
    INFO("driver:\n" << manager->driverSource().toStdString());
    INFO("cavern log:\n" << manager->cavernLog().toStdString());
    REQUIRE_FALSE(manager->hasSolveError());

    // PROJECT.MAK fixes A1 in ALPHA.DAT and C1 in CHARLIE.DAT; the source root
    // owns every station of the file.
    const QList<cwSolvedEntrance> entrances = manager->solvedEntrances(root->id());
    INFO("solved entrances: " << stationNames(entrances).join(QStringLiteral(", ")).toStdString());
    CHECK(entrances.size() == 2);
    for (const QString& station : {QStringLiteral("alpha.A1"), QStringLiteral("charlie.C1")}) {
        INFO("station " << station.toStdString());
        const std::optional<cwSolvedEntrance> entrance = entranceNamed(entrances, station);
        REQUIRE(entrance.has_value());
        CHECK(entrance->fixed);
        checkAt(*entrance, root->scopedPositions(), station);
    }
}

TEST_CASE("A native entrance on an unfixed cave is a solved, unfixed entrance",
          "[LinePlotManager][SolvedEntrances]")
{
    // The driver fixes an unfixed cave's first station at the origin, so a1
    // carries cavern's fixed flag; the entrance still reads unfixed.
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Native"));
    addNativeChain(cave, {QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a3")});
    cave->entrances()->addEntrance(QStringLiteral("a1"));
    cave->entrances()->addEntrance(QStringLiteral("a3"));

    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();
    INFO("driver:\n" << manager.driverSource().toStdString());
    INFO("cavern log:\n" << manager.cavernLog().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());
    CHECK(manager.driverSource().contains(QStringLiteral("*entrance a1\n")));
    CHECK(manager.driverSource().contains(QStringLiteral("*entrance a3\n")));

    const QList<cwSolvedEntrance> entrances = manager.solvedEntrances(cave->id());
    INFO("solved entrances: " << stationNames(entrances).join(QStringLiteral(", ")).toStdString());
    CHECK(entrances.size() == 2);
    for (const QString& station : {QStringLiteral("a1"), QStringLiteral("a3")}) {
        INFO("station " << station.toStdString());
        const std::optional<cwSolvedEntrance> entrance = entranceNamed(entrances, station);
        REQUIRE(entrance.has_value());
        CHECK_FALSE(entrance->fixed);
        checkAt(*entrance, cave->scopedPositions(), station);
    }
}

TEST_CASE("Marking a native entrance re-solves and adds it to the solved entrances",
          "[LinePlotManager][SolvedEntrances]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Native"));
    addNativeChain(cave, {QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a3")});

    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();
    REQUIRE_FALSE(manager.hasSolveError());
    REQUIRE(manager.solvedEntrances(cave->id()).isEmpty());

    QSignalSpy changedSpy(&manager, &cwLinePlotManager::solvedEntrancesChanged);
    cave->entrances()->addEntrance(QStringLiteral("a2"));
    CHECK(waitUntil([&]() { return manager.isSolvedEntrance(cave->id(), QStringLiteral("a2")); },
                    kSolveWaitMs));
    CHECK(changedSpy.count() >= 1);

    cave->entrances()->removeEntrance(QStringLiteral("a2"));
    CHECK(waitUntil([&]() { return manager.solvedEntrances(cave->id()).isEmpty(); },
                    kSolveWaitMs));
}

TEST_CASE("An entrance on a station the survey does not place warns on its node",
          "[LinePlotManager][SolvedEntrances]")
{
    // ghost is in no shot, so the driver skips it and the node says why.
    // phantom also has a fix, whose FixStationReference warning already
    // speaks for it.
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Native"));
    addNativeChain(cave, {QStringLiteral("a1"), QStringLiteral("a2")});
    cave->entrances()->addEntrance(QStringLiteral("a2"));
    cave->entrances()->addEntrance(QStringLiteral("ghost"));
    cave->entrances()->addEntrance(QStringLiteral("phantom"));
    cave->fixStations()->addFixStation(QStringLiteral("phantom"));

    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();
    INFO("driver:\n" << manager.driverSource().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());
    CHECK_FALSE(manager.driverSource().contains(QStringLiteral("ghost")));

    const std::optional<cwError> warning = unplacedEntranceWarning(cave);
    REQUIRE(warning.has_value());
    const QString text = warning->message() + QLatin1Char('\n') + warning->detail();
    INFO("warning: " << text.toStdString());
    CHECK(warning->type() == cwError::Warning);
    CHECK(text.contains(QStringLiteral("ghost")));
    CHECK_FALSE(text.contains(QStringLiteral("phantom")));
    CHECK_FALSE(text.contains(QStringLiteral("a2")));

    cave->entrances()->removeEntrance(QStringLiteral("ghost"));
    CHECK(waitUntil([cave]() { return !unplacedEntranceWarning(cave).has_value(); },
                    kSolveWaitMs));
}

TEST_CASE("A solve publishes its entrances on the region", "[LinePlotManager][SolvedEntrances]")
{
    cwCavingRegion region;
    cwCave* cave = addEmptyCave(region, QStringLiteral("Native"));
    addNativeChain(cave, {QStringLiteral("a1"), QStringLiteral("a2"), QStringLiteral("a3")});
    cave->entrances()->addEntrance(QStringLiteral("a3"));

    QSignalSpy regionSpy(&region, &cwCavingRegion::solvedEntrancesChanged);
    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();
    INFO("driver:\n" << manager.driverSource().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());
    REQUIRE(manager.isSolvedEntrance(cave->id(), QStringLiteral("a3")));

    const QList<cwSolvedEntrance> entrances = region.solvedEntrances().value(cave->id());
    INFO("region's solved entrances: " << stationNames(entrances).join(QStringLiteral(", ")).toStdString());
    REQUIRE(entrances.size() == 1);
    CHECK(entrances == manager.solvedEntrances(cave->id()));
    CHECK(region.solvedEntrances() == manager.solvedEntrances());
    CHECK(regionSpy.count() >= 1);
}

TEST_CASE("The line plot manager reads the solved entrances from its region",
          "[LinePlotManager][SolvedEntrances]")
{
    // A region with nothing to solve keeps whatever a writer sets on it, so the
    // manager's answers and its notification come from the region alone.
    cwCavingRegion region;
    cwLinePlotManager manager;
    manager.setRegion(&region);
    manager.waitToFinish();
    REQUIRE(manager.solvedEntrances().isEmpty());

    cwSolvedEntrance entrance;
    entrance.stationName = QStringLiteral("b2");
    entrance.qualifiedName = QStringLiteral("hollow.b2");
    entrance.fixed = false;
    const QUuid nodeId = QUuid::createUuid();

    QSignalSpy managerSpy(&manager, &cwLinePlotManager::solvedEntrancesChanged);
    region.setSolvedEntrances({{nodeId, {entrance}}});

    CHECK(managerSpy.count() == 1);
    CHECK(manager.solvedEntrances(nodeId) == QList<cwSolvedEntrance>{entrance});
    CHECK(manager.isSolvedEntrance(nodeId, QStringLiteral("B2")));
}
