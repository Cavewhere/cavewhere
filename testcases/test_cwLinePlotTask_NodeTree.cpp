/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The line plot at depth N: cavern's names come back as label paths into one
// region lookup, every node's scoped view holds its subtree under the names
// below it, and a node's length and depth fold over its subtree.

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Cavewhere
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwEquate.h"
#include "cwEquateModel.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwExternalCenterline.h"
#include "cwFixStation.h"
#include "cwFixStationDiagnostics.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwLength.h"
#include "cwLinePlotGeometry.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwScopeLabels.h"
#include "cwStationHandle.h"
#include "cwSurveyChunk.h"
#include "cwSurveyNetwork.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"
#include "cwSignalSpy.h"
#include "LoadProjectHelper.h"
#include "SurveyTreeTestHelper.h"
#include "TestHelper.h"

// Qt
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>

// Std
#include <algorithm>

namespace {

// addNativeTripWithShot's default: a 10 m level shot due north.
constexpr double kShotLength = 10.0;
constexpr double kDropLength = 10.0;
constexpr double kStraightDown = -90.0;
// survex_simple.svx: A1-A2 10 m, A2-A3 8.5 m, both level, fixed at the origin.
constexpr double kSimpleFixtureLength = 18.5;
// The frame the solve reports in, and the system the fixes are entered in: a
// transverse Mercator with no scale distortion at its origin, so lengths read
// off solved positions are survey lengths, and positions stay small enough
// for the float lookup to hold them to the millimeter.
const QString kFrameCS = QStringLiteral(
    "+proj=tmerc +lat_0=37.1832 +lon_0=-84.0947 +k=1 +x_0=0 +y_0=0 "
    "+datum=WGS84 +units=m +no_defs +type=crs");
// The Folder and Fisher Ridge are fixed away from the origin survex_simple.svx
// fixes itself at, so each survey's stations sit at their own positions.
constexpr double kFolderNorthing = -1000.0;
constexpr double kFisherRidgeEasting = 1000.0;
constexpr double kPositionMarginMeters = 0.001;

cwStationHandle nativeHandle(const cwSurveyNode* node, const QString& tail)
{
    return cwStationHandle(cwStationHandle::NativeCave, node->id(), tail);
}

cwFixStation fixAt(const QString& station, double easting, double northing)
{
    cwFixStation fix;
    fix.setStationName(station);
    fix.setInputCS(kFrameCS);
    fix.setEasting(easting);
    fix.setNorthing(northing);
    return fix;
}

//! The lookup's station names, lowercased: cavern lowercases the Survex names
//! it reads, and the case a name comes back in is not what these tests are about.
QSet<QString> stationNames(const cwSurveyNode* node)
{
    QSet<QString> names;
    const QStringList keys = node->scopedPositions().positions().keys();
    for (const QString& key : keys) {
        names.insert(key.toLower());
    }
    return names;
}

//! The §7.1 tree with a shot in every node:
//!
//!   Kentucky field seasons       Folder, a trip k1-k2
//!     Side Cave                  Cave, a trip s1-s2
//!       Upper level              Section: a straight drop u1-u2 and a
//!                                self-fixed attached trip (Dome climb)
//!   Fisher Ridge                 Cave, a trip a1-a2
//!
//! Region equates tie each native level to the one above it, so the Folder's
//! fix anchors the native part of its subtree.
struct NodeTree {
    cwCave* folder = nullptr;
    cwCave* sideCave = nullptr;
    cwCave* section = nullptr;
    cwCave* fisherRidge = nullptr;
    QList<cwTrip*> trips;
    QHash<QUuid, QString> tripDirs;
};

NodeTree buildNodeTree(cwCavingRegion& region, const QTemporaryDir& tempRoot)
{
    NodeTree tree;
    region.geoReference()->restore(cwGeoReference::Frozen, kFrameCS, {}, QString());
    tree.folder = addChildNode(region.rootNode(), QStringLiteral("Kentucky field seasons"),
                               cwSurveyNode::Kind::Folder);
    tree.folder->fixStations()->appendFixStation(fixAt(QStringLiteral("k1"), 0.0, kFolderNorthing));
    tree.trips.append(addNativeTripWithShot(tree.folder, QStringLiteral("Ridge walk"),
                                            QStringLiteral("k1"), QStringLiteral("k2")));

    tree.sideCave = addChildNode(tree.folder, QStringLiteral("Side Cave"), cwSurveyNode::Kind::Cave);
    tree.trips.append(addNativeTripWithShot(tree.sideCave, QStringLiteral("Sump dig"),
                                            QStringLiteral("s1"), QStringLiteral("s2")));

    tree.section = addChildNode(tree.sideCave, QStringLiteral("Upper level"),
                                cwSurveyNode::Kind::Folder);
    tree.trips.append(addNativeTripWithShot(tree.section, QStringLiteral("Upper survey"),
                                            QStringLiteral("u1"), QStringLiteral("u2"),
                                            cwDistanceReading(QString::number(kDropLength)),
                                            cwClinoReading(QString::number(kStraightDown))));
    cwTrip* domeClimb = addAttachedTrip(tree.section, QStringLiteral("Dome climb"));
    // The file's *fix is bare, so the attachment chooses the system it reads in.
    cwExternalCenterline domeClimbCenterline = domeClimb->externalCenterline();
    domeClimbCenterline.setFixCoordinateSystem(kFrameCS);
    domeClimb->setExternalCenterline(domeClimbCenterline);
    attachFixture(tempRoot, domeClimb, QStringLiteral("survex_simple.svx"), tree.tripDirs);
    tree.trips.append(domeClimb);

    tree.fisherRidge = addEmptyCave(region, QStringLiteral("Fisher Ridge"));
    tree.fisherRidge->fixStations()->appendFixStation(fixAt(QStringLiteral("a1"), kFisherRidgeEasting, 0.0));
    tree.trips.append(addNativeTripWithShot(tree.fisherRidge, QStringLiteral("Entrance"),
                                            QStringLiteral("a1"), QStringLiteral("a2")));

    region.equates()->appendEquate(cwEquate({nativeHandle(tree.section, QStringLiteral("u1")),
                                             nativeHandle(tree.sideCave, QStringLiteral("s2"))}));
    region.equates()->appendEquate(cwEquate({nativeHandle(tree.sideCave, QStringLiteral("s1")),
                                             nativeHandle(tree.folder, QStringLiteral("k2"))}));

    // A fixed, dated trip would otherwise turn its shots by the IGRF
    // declination, and the .3d stores positions to the centimeter, so a turned
    // 10 m shot reads back a few millimeters off its length.
    for (cwTrip* trip : std::as_const(tree.trips)) {
        trip->calibrations()->setAutoDeclination(false);
    }
    return tree;
}

//! Copies the checked-in fixture into \a destination, so a test can load it and
//! let the project write without touching the source tree.
void copyDirectory(const QDir& source, const QDir& destination)
{
    QDirIterator it(source.absolutePath(), QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString target = destination.absoluteFilePath(source.relativeFilePath(path));
        REQUIRE(QDir().mkpath(QFileInfo(target).absolutePath()));
        REQUIRE(QFile::copy(path, target));
    }
}

cwCave* childNamed(const cwSurveyNode* parent, const QString& name)
{
    const QList<cwSurveyNode*> children = parent->childNodes();
    for (cwSurveyNode* child : children) {
        if (child->name() == name) {
            return qobject_cast<cwCave*>(child);
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("A nested tree solves into one region lookup, each node's view holding its subtree",
          "[LinePlotManager][NodeTree]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const NodeTree tree = buildNodeTree(region, tempRoot);

    cwLinePlotManager manager;
    QSet<cwTrip*> movedTrips;
    QObject::connect(&manager, &cwLinePlotManager::stationPositionInTripsChanged,
                     &manager, [&movedTrips](const QList<cwTrip*>& trips) {
                         for (cwTrip* trip : trips) {
                             movedTrips.insert(trip);
                         }
                     });
    solveRegion(manager, region, tree.tripDirs);
    INFO("cavern log:\n" << manager.cavernLog().toStdString());
    INFO("driver:\n" << manager.driverSource().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    const QSet<QString> domeClimbStations = {
        QStringLiteral("dome_climb.simple.a1"),
        QStringLiteral("dome_climb.simple.a2"),
        QStringLiteral("dome_climb.simple.a3"),
    };

    const auto under = [](const QString& label, const QSet<QString>& names) {
        QSet<QString> prefixed;
        for (const QString& name : names) {
            prefixed.insert(label + QLatin1Char('.') + name);
        }
        return prefixed;
    };

    const QSet<QString> sectionStations =
        QSet<QString>{QStringLiteral("u1"), QStringLiteral("u2")} + domeClimbStations;
    const QSet<QString> sideCaveStations =
        QSet<QString>{QStringLiteral("s1"), QStringLiteral("s2")}
        + under(QStringLiteral("upper_level"), sectionStations);
    const QSet<QString> folderStations =
        QSet<QString>{QStringLiteral("k1"), QStringLiteral("k2")}
        + under(QStringLiteral("side_cave"), sideCaveStations);

    CHECK(stationNames(tree.section) == sectionStations);
    CHECK(stationNames(tree.sideCave) == sideCaveStations);
    CHECK(stationNames(tree.folder) == folderStations);
    CHECK(stationNames(tree.fisherRidge)
          == QSet<QString>{QStringLiteral("a1"), QStringLiteral("a2")});

    // One station, three keys: each ancestor sees it under the labels between
    // it and the station's own node, and the region under the whole path.
    const QVector3D u2 = tree.section->scopedPositions().position(QStringLiteral("u2"));
    CHECK(tree.sideCave->scopedPositions().position(QStringLiteral("upper_level.u2")) == u2);
    CHECK(tree.folder->scopedPositions().position(QStringLiteral("side_cave.upper_level.u2")) == u2);
    CHECK(region.stationPositions().position(tree.section->scopePrefix() + QStringLiteral("u2")) == u2);

    // The equates hold: u1 sits on s2, and s1 on k2.
    const QVector3D u1 = tree.section->scopedPositions().position(QStringLiteral("u1"));
    const QVector3D s2 = tree.sideCave->scopedPositions().position(QStringLiteral("s2"));
    CHECK((u1 - s2).length() == Catch::Approx(0.0).margin(kPositionMarginMeters));
    CHECK((u1 - u2).length() == Catch::Approx(kDropLength).margin(kPositionMarginMeters));

    // A node's length is its own trips' plus its children's; its depth spans
    // its whole subtree, so the Section's drop deepens every node above it.
    const double sectionLength = kDropLength + kSimpleFixtureLength;
    const double sideCaveLength = kShotLength + sectionLength;
    const double folderLength = kShotLength + sideCaveLength;
    CHECK(tree.section->length()->value() == Catch::Approx(sectionLength).margin(kPositionMarginMeters));
    CHECK(tree.sideCave->length()->value() == Catch::Approx(sideCaveLength).margin(kPositionMarginMeters));
    CHECK(tree.folder->length()->value() == Catch::Approx(folderLength).margin(kPositionMarginMeters));
    CHECK(tree.fisherRidge->length()->value() == Catch::Approx(kShotLength).margin(kPositionMarginMeters));
    CHECK(tree.section->depth()->value() == Catch::Approx(kDropLength).margin(kPositionMarginMeters));
    CHECK(tree.sideCave->depth()->value() == Catch::Approx(kDropLength).margin(kPositionMarginMeters));
    CHECK(tree.folder->depth()->value() == Catch::Approx(kDropLength).margin(kPositionMarginMeters));
    CHECK(tree.fisherRidge->depth()->value() == Catch::Approx(0.0).margin(kPositionMarginMeters));

    // Every trip, at every depth, is drawn and reported as moved.
    const auto result = cwLinePlotGeometry::generate(region.data(), region.stationPositions(), manager.regionNetwork());
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    for (const cwTrip* trip : tree.trips) {
        INFO("trip: " << trip->name().toStdString());
        CHECK(vertexCountOf(geometry, trip) > 0);
    }
    CHECK(movedTrips.contains(tree.section->trip(0)));
}

TEST_CASE("A trip two levels down hears exactly one solvedStationsChanged per solve",
          "[LinePlotManager][NodeTree]")
{
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    const NodeTree tree = buildNodeTree(region, tempRoot);
    cwTrip* upperSurvey = tree.section->trip(0);
    REQUIRE(upperSurvey != nullptr);
    REQUIRE(tree.section->parentNode() == tree.sideCave);
    REQUIRE(tree.sideCave->parentNode() == tree.folder);

    cwSignalSpy pulseSpy(upperSurvey, &cwTrip::solvedStationsChanged);

    cwLinePlotManager manager;
    solveRegion(manager, region, tree.tripDirs);
    REQUIRE_FALSE(manager.hasSolveError());
    CHECK(pulseSpy.count() == 1);

    // Lengthen the Folder's shot: k2 moves, and the equates carry every level
    // below it along, the Section included.
    const QVector3D u1Before = tree.section->scopedPositions().position(QStringLiteral("u1"));
    cwSurveyChunk* ridgeWalk = tree.trips.first()->chunk(0);
    ridgeWalk->setData(cwSurveyChunk::ShotDistanceRole, 0, QString::number(kShotLength * 2.0));
    manager.waitToFinish();
    REQUIRE_FALSE(manager.hasSolveError());
    REQUIRE(tree.section->scopedPositions().position(QStringLiteral("u1")) != u1Before);
    CHECK(pulseSpy.count() == 2);

    // Move the Section under Fisher Ridge, still two levels down: the trip
    // hears the pulse through its new parent alone, once per solve.
    region.moveNode(tree.section, tree.fisherRidge, 0);
    manager.waitToFinish();
    REQUIRE_FALSE(manager.hasSolveError());
    REQUIRE(tree.section->parentNode() == tree.fisherRidge);
    CHECK(pulseSpy.count() == 3);

    ridgeWalk->setData(cwSurveyChunk::ShotDistanceRole, 0, QString::number(kShotLength * 3.0));
    manager.waitToFinish();
    REQUIRE_FALSE(manager.hasSolveError());
    CHECK(pulseSpy.count() == 4);
}

TEST_CASE("A Folder's length is the sum of its caves", "[cwLinePlotGeometry][NodeTree]")
{
    cwCavingRegion region;
    cwCave* folder = addChildNode(region.rootNode(), QStringLiteral("Folder"),
                                  cwSurveyNode::Kind::Folder);
    cwCave* upper = addChildNode(folder, QStringLiteral("Upper"), cwSurveyNode::Kind::Cave);
    cwTrip* upperTrip = addNativeTripWithShot(upper, QStringLiteral("Upper trip"),
                                              QStringLiteral("a1"), QStringLiteral("a2"));
    cwCave* lower = addChildNode(folder, QStringLiteral("Lower"), cwSurveyNode::Kind::Cave);
    cwTrip* lowerTrip = addNativeTripWithShot(lower, QStringLiteral("Lower trip"),
                                              QStringLiteral("b1"), QStringLiteral("b2"),
                                              cwDistanceReading(QString::number(kDropLength)),
                                              cwClinoReading(QString::number(kStraightDown)));

    // Hand-set solved positions: the geometry reads each node's stations out
    // of the region lookup under the node's label path.
    constexpr float kUpperFloor = 5.0f;
    constexpr float kLowerFloor = -20.0f;
    cwStationPositionLookup upperLookup;
    upperLookup.setPosition(QStringLiteral("a1"), QVector3D(0.0f, 0.0f, kUpperFloor));
    upperLookup.setPosition(QStringLiteral("a2"), QVector3D(0.0f, float(kShotLength), kUpperFloor));
    SurveyTreeTestHelper::setScopedPositions(upper, upperLookup);

    cwStationPositionLookup lowerLookup;
    lowerLookup.setPosition(QStringLiteral("b1"), QVector3D(0.0f, 0.0f, kLowerFloor + float(kDropLength)));
    lowerLookup.setPosition(QStringLiteral("b2"), QVector3D(0.0f, 0.0f, kLowerFloor));
    SurveyTreeTestHelper::setScopedPositions(lower, lowerLookup);

    const auto result = cwLinePlotGeometry::generate(region.data(), region.stationPositions());
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    CHECK(vertexCountOf(geometry, upperTrip) == 2);
    CHECK(vertexCountOf(geometry, lowerTrip) == 2);

    REQUIRE(geometry.nodeLengthAndDepths.contains(folder->id()));
    REQUIRE(geometry.nodeLengthAndDepths.contains(upper->id()));
    REQUIRE(geometry.nodeLengthAndDepths.contains(lower->id()));

    const cwLinePlotGeometry::LengthAndDepth upperExtent = geometry.nodeLengthAndDepths.value(upper->id());
    const cwLinePlotGeometry::LengthAndDepth lowerExtent = geometry.nodeLengthAndDepths.value(lower->id());
    const cwLinePlotGeometry::LengthAndDepth folderExtent = geometry.nodeLengthAndDepths.value(folder->id());

    CHECK(upperExtent.length() == Catch::Approx(kShotLength));
    CHECK(upperExtent.depth() == Catch::Approx(0.0));
    CHECK(lowerExtent.length() == Catch::Approx(kDropLength));
    CHECK(lowerExtent.depth() == Catch::Approx(kDropLength));
    CHECK(folderExtent.length() == Catch::Approx(upperExtent.length() + lowerExtent.length()));
    // The Folder spans from Upper's floor down to Lower's.
    CHECK(folderExtent.depth() == Catch::Approx(double(kUpperFloor - kLowerFloor)));
}

TEST_CASE("A Folder's length sums a native Cave and an attached Cave, and each attached trip is as long as its legs",
          "[cwLinePlotGeometry][NodeTree]")
{
    cwCavingRegion region;
    cwCave* folder = addChildNode(region.rootNode(), QStringLiteral("folder"),
                                  cwSurveyNode::Kind::Folder);
    cwCave* upper = addChildNode(folder, QStringLiteral("upper"), cwSurveyNode::Kind::Cave);
    cwTrip* upperTrip = addNativeTripWithShot(upper, QStringLiteral("Upper trip"),
                                              QStringLiteral("a1"), QStringLiteral("a2"));

    // A source root with one derived node and two derived trips, the shape an
    // attached Compass file lands in.
    cwCave* sourced = addChildNode(folder, QStringLiteral("project"), cwSurveyNode::Kind::Cave);
    sourced->setExternalCenterline(cwExternalCenterline(QStringLiteral("PROJECT.MAK")));
    sourced->setSourceId(QUuid::createUuid());
    cwCave* alpha = new cwCave();
    alpha->setName(QStringLiteral("Alpha"));
    alpha->setSourceId(sourced->sourceId());
    alpha->setSourcePath(QStringLiteral("ALPHA.DAT"));
    alpha->setCavernLabel(QStringLiteral("alpha"));
    sourced->addNode(alpha);
    const auto addDerivedTrip = [alpha](const QString& name, const QStringList& stations) {
        cwTrip* trip = new cwTrip();
        trip->setName(name);
        trip->setSourcePath(QStringLiteral("ALPHA.DAT/") + name);
        trip->setExternalStations(stations);
        alpha->addTrip(trip);
        return trip;
    };
    cwTrip* first = addDerivedTrip(QStringLiteral("ALPHA1"),
                                   {QStringLiteral("p1"), QStringLiteral("p2"), QStringLiteral("p3")});
    cwTrip* second = addDerivedTrip(QStringLiteral("ALPHA2"),
                                    {QStringLiteral("p3"), QStringLiteral("p4")});

    cwStationPositionLookup upperLookup;
    upperLookup.setPosition(QStringLiteral("a1"), QVector3D(0.0f, 0.0f, 0.0f));
    upperLookup.setPosition(QStringLiteral("a2"), QVector3D(0.0f, float(kShotLength), 0.0f));
    SurveyTreeTestHelper::setScopedPositions(upper, upperLookup);

    // ALPHA1: 6 m north then 8 m east; ALPHA2: 5 m south.
    constexpr double kFirstLegLength = 6.0;
    constexpr double kSecondLegLength = 8.0;
    constexpr double kThirdLegLength = 5.0;
    cwStationPositionLookup sourcedLookup;
    sourcedLookup.setPosition(QStringLiteral("alpha.p1"), QVector3D(0.0f, 0.0f, 0.0f));
    sourcedLookup.setPosition(QStringLiteral("alpha.p2"), QVector3D(0.0f, float(kFirstLegLength), 0.0f));
    sourcedLookup.setPosition(QStringLiteral("alpha.p3"),
                              QVector3D(float(kSecondLegLength), float(kFirstLegLength), 0.0f));
    sourcedLookup.setPosition(QStringLiteral("alpha.p4"),
                              QVector3D(float(kSecondLegLength), float(kFirstLegLength - kThirdLegLength), 0.0f));
    SurveyTreeTestHelper::setScopedPositions(sourced, sourcedLookup);

    cwSurveyNetwork network;
    network.addShot(QStringLiteral("folder.project.alpha.P1"), QStringLiteral("folder.project.alpha.P2"));
    network.addShot(QStringLiteral("folder.project.alpha.P2"), QStringLiteral("folder.project.alpha.P3"));
    network.addShot(QStringLiteral("folder.project.alpha.P3"), QStringLiteral("folder.project.alpha.P4"));

    const auto result = cwLinePlotGeometry::generate(region.data(), region.stationPositions(), network);
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    // The geometry draws every trip, so the lengths below have legs to measure.
    REQUIRE(vertexCountOf(geometry, upperTrip) == 2);
    REQUIRE(vertexCountOf(geometry, first) == 4);
    REQUIRE(vertexCountOf(geometry, second) == 2);

    const double attachedLength = kFirstLegLength + kSecondLegLength + kThirdLegLength;
    CHECK(geometry.nodeLengthAndDepths.value(alpha->id()).length() == Catch::Approx(attachedLength));
    CHECK(geometry.nodeLengthAndDepths.value(sourced->id()).length() == Catch::Approx(attachedLength));
    CHECK(geometry.nodeLengthAndDepths.value(folder->id()).length()
          == Catch::Approx(kShotLength + attachedLength));

    CHECK(tripLengthOf(geometry, upperTrip) == Catch::Approx(kShotLength));
    CHECK(tripLengthOf(geometry, first) == Catch::Approx(kFirstLegLength + kSecondLegLength));
    CHECK(tripLengthOf(geometry, second) == Catch::Approx(kThirdLegLength));
}

TEST_CASE("The depth-2 fixture loads and solves with every trip drawn",
          "[LinePlotManager][NodeTree]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir workingRoot(QDir(tempDir.path()).absoluteFilePath(QStringLiteral("depth2")));
    copyDirectory(QDir(testcasesDatasetSourcePath(QStringLiteral("survey-tree/depth2"))), workingRoot);

    auto rootData = std::make_unique<cwRootData>();
    addTokenManager(rootData->project());
    rootData->project()->loadOrConvert(workingRoot.absoluteFilePath(QStringLiteral("depth2.cwproj")));
    rootData->project()->waitLoadToFinish();

    cwCavingRegion* region = rootData->project()->cavingRegion();
    cwCave* fisherRidge = childNamed(region->rootNode(), QStringLiteral("Fisher Ridge"));
    cwCave* folder = childNamed(region->rootNode(), QStringLiteral("Kentucky field seasons"));
    REQUIRE(fisherRidge != nullptr);
    REQUIRE(folder != nullptr);
    cwCave* sideCave = childNamed(folder, QStringLiteral("Side Cave"));
    REQUIRE(sideCave != nullptr);
    cwCave* section = childNamed(sideCave, QStringLiteral("Upper level"));
    REQUIRE(section != nullptr);

    // The fixture is a persistence fixture: its trips are one shot each and
    // only Fisher Ridge and Side Cave are tied. Join the rest the way a user
    // would — a connecting shot in Fisher Ridge, and a region tie from the
    // Section to its Cave — so cavern places every trip.
    cwTrip* crystalCrawl = tripNamed(fisherRidge, QStringLiteral("Crystal crawl"));
    REQUIRE(crystalCrawl != nullptr);
    SurveyTreeTestHelper::addShot(crystalCrawl, QStringLiteral("A2"), QStringLiteral("B1"));
    region->equates()->appendEquate(cwEquate({nativeHandle(section, QStringLiteral("D1")),
                                              nativeHandle(sideCave, QStringLiteral("C2"))}));

    cwLinePlotManager* manager = rootData->linePlotManager();
    manager->waitToFinish();
    INFO("cavern log:\n" << manager->cavernLog().toStdString());
    INFO("driver:\n" << manager->driverSource().toStdString());
    REQUIRE_FALSE(manager->hasSolveError());

    CHECK(stationNames(section) == QSet<QString>{QStringLiteral("d1"), QStringLiteral("d2")});
    CHECK(stationNames(sideCave).contains(QStringLiteral("upper_level.d2")));
    CHECK(stationNames(folder).contains(QStringLiteral("side_cave.upper_level.d2")));

    const auto result = cwLinePlotGeometry::generate(region->data(), region->stationPositions(), manager->regionNetwork());
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    const QList<cwTrip*> trips = region->rootNode()->allTrips();
    REQUIRE(trips.size() == 4);
    for (const cwTrip* trip : trips) {
        INFO("trip: " << trip->name().toStdString());
        CHECK(vertexCountOf(geometry, trip) > 0);
    }

    rootData->project()->waitSaveToFinish();
}

TEST_CASE("The depth-2 fixture's unconnected surveys warn on their trips",
          "[LinePlotManager][NodeTree]")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QDir workingRoot(QDir(tempDir.path()).absoluteFilePath(QStringLiteral("depth2")));
    copyDirectory(QDir(testcasesDatasetSourcePath(QStringLiteral("survey-tree/depth2"))), workingRoot);

    auto rootData = std::make_unique<cwRootData>();
    addTokenManager(rootData->project());
    rootData->project()->loadOrConvert(workingRoot.absoluteFilePath(QStringLiteral("depth2.cwproj")));
    rootData->project()->waitLoadToFinish();

    cwCavingRegion* region = rootData->project()->cavingRegion();
    cwCave* fisherRidge = childNamed(region->rootNode(), QStringLiteral("Fisher Ridge"));
    cwCave* folder = childNamed(region->rootNode(), QStringLiteral("Kentucky field seasons"));
    REQUIRE(fisherRidge != nullptr);
    REQUIRE(folder != nullptr);
    cwCave* sideCave = childNamed(folder, QStringLiteral("Side Cave"));
    REQUIRE(sideCave != nullptr);
    cwCave* section = childNamed(sideCave, QStringLiteral("Upper level"));
    REQUIRE(section != nullptr);
    REQUIRE(section->trips().size() == 1);
    cwTrip* upperSurvey = section->trips().first();

    cwTrip* entranceSurvey = tripNamed(fisherRidge, QStringLiteral("Entrance survey"));
    cwTrip* crystalCrawl = tripNamed(fisherRidge, QStringLiteral("Crystal crawl"));
    REQUIRE(entranceSurvey != nullptr);
    REQUIRE(crystalCrawl != nullptr);

    cwLinePlotManager* manager = rootData->linePlotManager();
    manager->waitToFinish();
    INFO("solve error: " << manager->solveErrorMessage().toStdString());

    SECTION("The untied Section warns on its trip and rolls up to its node")
    {
        // Entrance survey shares no station with Crystal crawl, but the
        // fixture's region tie joins it to Side Cave, which the solve places,
        // so it is tied in. The Section is tied to nothing; cavern drops it.
        INFO("cavern log:\n" << manager->cavernLog().toStdString());
        REQUIRE_FALSE(manager->hasSolveError());
        CHECK_FALSE(unconnectedStationsEntry(entranceSurvey->errorModel()).has_value());
        CHECK_FALSE(unconnectedStationsEntry(crystalCrawl->errorModel()).has_value());

        const std::optional<cwError> entry = unconnectedStationsEntry(upperSurvey->errorModel());
        REQUIRE(entry.has_value());
        CHECK(entry->type() == cwError::Warning);
        CHECK(entry->message()
              == QStringLiteral("2 stations in %1 are not tied to the cave").arg(upperSurvey->name()));
        CHECK(entry->detail() == QStringLiteral("d1, d2"));
        CHECK_FALSE(unconnectedStationsEntry(section->errorModel()).has_value());

        // The node counts its trips' warnings, so silencing the trip's entry
        // takes exactly one off the node.
        cwErrorListModel* upperErrors = upperSurvey->errorModel()->errors();
        const int nodeWarnings = section->errorModel()->warningCount();
        const int row = upperErrors->indexOf(*entry);
        REQUIRE(row >= 0);
        REQUIRE(upperErrors->setData(upperErrors->index(row), true,
                                     static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole)));
        CHECK(section->errorModel()->warningCount() == nodeWarnings - 1);
    }

    SECTION("Tying the surveys in clears each warning on the next solve")
    {
        // Joining Fisher Ridge's two surveys leaves the Section dropped, since
        // nothing ties it to its Cave yet.
        SurveyTreeTestHelper::addShot(crystalCrawl, QStringLiteral("A2"), QStringLiteral("B1"));
        manager->waitToFinish();
        INFO("cavern log:\n" << manager->cavernLog().toStdString());
        REQUIRE_FALSE(manager->hasSolveError());
        CHECK_FALSE(unconnectedStationsEntry(entranceSurvey->errorModel()).has_value());

        const std::optional<cwError> sectionEntry = unconnectedStationsEntry(upperSurvey->errorModel());
        REQUIRE(sectionEntry.has_value());
        CHECK(sectionEntry->message()
              == QStringLiteral("2 stations in %1 are not tied to the cave").arg(upperSurvey->name()));
        CHECK(sectionEntry->detail() == QStringLiteral("d1, d2"));

        region->equates()->appendEquate(cwEquate({nativeHandle(section, QStringLiteral("D1")),
                                                  nativeHandle(sideCave, QStringLiteral("C2"))}));
        manager->waitToFinish();
        REQUIRE_FALSE(manager->hasSolveError());
        CHECK_FALSE(unconnectedStationsEntry(upperSurvey->errorModel()).has_value());
        for (const cwTrip* trip : region->rootNode()->allTrips()) {
            INFO("trip: " << trip->name().toStdString());
            CHECK_FALSE(unconnectedStationsEntry(trip->errorModel()).has_value());
        }
    }

    rootData->project()->waitSaveToFinish();
}

TEST_CASE("A cave's solved network holds only its own stations when a region equate ties it to another cave",
          "[LinePlotManager][NodeTree][FixStation]")
{
    // An equate puts two caves' stations on one coordinate, and each leg keeps
    // its own cave's names. The Fix Stations page measures a fix against the
    // cave's scoped network, and the export against the cave's own stations:
    // both agree on which names the cave holds.
    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    cwCavingRegion region;
    region.geoReference()->restore(cwGeoReference::Frozen, kFrameCS, {}, QString());
    cwCave* alpha = addEmptyCave(region, QStringLiteral("Alpha"));
    alpha->fixStations()->appendFixStation(fixAt(QStringLiteral("a1"), 0.0, 0.0));
    cwTrip* alphaTrip = addNativeTripWithShot(alpha, QStringLiteral("Alpha trip"),
                                              QStringLiteral("a1"), QStringLiteral("a2"));
    cwCave* beta = addEmptyCave(region, QStringLiteral("Beta"));
    // Beta's own fix agrees with the equate (b1 sits on a2, one shot north of
    // a1), so the export's fallback fix stays out of the picture.
    beta->fixStations()->appendFixStation(fixAt(QStringLiteral("b1"), 0.0, kShotLength));
    cwTrip* betaTrip = addNativeTripWithShot(beta, QStringLiteral("Beta trip"),
                                             QStringLiteral("b1"), QStringLiteral("b2"));
    for (cwTrip* trip : {alphaTrip, betaTrip}) {
        trip->calibrations()->setAutoDeclination(false);
    }
    region.equates()->appendEquate(cwEquate({nativeHandle(alpha, QStringLiteral("a2")),
                                             nativeHandle(beta, QStringLiteral("b1"))}));

    cwLinePlotManager manager;
    solveRegion(manager, region, {});
    INFO("driver:\n" << manager.driverSource().toStdString());
    REQUIRE_FALSE(manager.hasSolveError());

    // The equate puts a2 and b1 on one point, but each leg keeps the names of
    // the cave that shot it: no edge joins the two caves.
    const cwSurveyNetwork regionNetwork = manager.regionNetwork();
    INFO("region network stations: " << regionNetwork.stations().join(QStringLiteral(", ")).toStdString());
    const auto regionNeighbors = [&regionNetwork](const QString& station) {
        QStringList neighbors = regionNetwork.neighbors(station);
        neighbors.sort();
        return neighbors.join(QStringLiteral(", ")).toStdString();
    };
    CHECK(regionNeighbors(QStringLiteral("alpha.a1")) == "alpha.a2");
    CHECK(regionNeighbors(QStringLiteral("alpha.a2")) == "alpha.a1");
    CHECK(regionNeighbors(QStringLiteral("beta.b1")) == "beta.b2");
    CHECK(regionNeighbors(QStringLiteral("beta.b2")) == "beta.b1");

    const QHash<cwCave*, QSet<QString>> ownStations = {
        {alpha, {QStringLiteral("a1"), QStringLiteral("a2")}},
        {beta, {QStringLiteral("b1"), QStringLiteral("b2")}},
    };
    const QHash<cwCave*, QStringList> otherCaveStations = {
        {alpha, {QStringLiteral("beta.b1"), QStringLiteral("beta.b2")}},
        {beta, {QStringLiteral("alpha.a1"), QStringLiteral("alpha.a2")}},
    };

    for (auto it = ownStations.constBegin(); it != ownStations.constEnd(); ++it) {
        cwCave* cave = it.key();
        INFO("cave: " << cave->name().toStdString());
        const cwSurveyNetwork network = cave->scopedNetwork();
        REQUIRE_FALSE(network.isEmpty());

        QStringList foreignStations;
        for (const QString& station : network.stations()) {
            if (!it.value().contains(station)) {
                foreignStations.append(station);
            }
        }
        INFO("scoped network stations: " << network.stations().join(QStringLiteral(", ")).toStdString());
        CHECK(foreignStations.isEmpty());

        // The other cave's names, and any foreign name the network holds, are
        // ones the export rejects for this cave, so the Fix Stations page has
        // to reject them too.
        QStringList rejectedStations = otherCaveStations.value(cave);
        for (const QString& foreign : std::as_const(foreignStations)) {
            if (!rejectedStations.contains(foreign)) {
                rejectedStations.append(foreign);
            }
        }
        for (const QString& foreign : std::as_const(rejectedStations)) {
            INFO("foreign station: " << foreign.toStdString());

            // The fix goes into the snapshot the tree and the plan are built
            // from, since the plan validates each node's fixes by its id.
            cwCavingRegionData regionData = region.data();
            const auto caveData = std::find_if(regionData.caves.begin(), regionData.caves.end(),
                                               [cave](const cwCaveData& node) {
                                                   return node.id == cave->id();
                                               });
            REQUIRE(caveData != regionData.caves.end());
            caveData->fixStations = {fixAt(foreign, 0.0, 0.0)};
            const cwSurvexExporterCaveTask::DriverTree tree(regionData.caves,
                                                            cwScopeLabels(regionData));
            const cwSurvexExporterCaveTask::AnchorPlan plan(tree, {});
            cwSurvexExporterCaveTask task;
            QString driver;
            QTextStream stream(&driver);
            task.writeNode(stream, *caveData, tree, plan);
            CHECK(task.errors().contains(
                QStringLiteral("Fix references unknown station: \"%1\"").arg(foreign)));

            CHECK(cwFixStationDiagnostics::classifyStationReference(
                      foreign, network, cwFixStationDiagnostics::SurveyData::Stations)
                  == cwFixStationDiagnostics::StationReference::Unknown);
        }
    }
}
