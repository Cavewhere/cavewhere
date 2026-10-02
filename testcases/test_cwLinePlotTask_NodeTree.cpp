/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// The line plot at depth N: cavern's names come back as label paths, every
// station lands in its node's lookup and in each ancestor's (a node's slice is
// its subtree), and a node's length and depth fold over its subtree.

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
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwGeoReference.h"
#include "cwLength.h"
#include "cwLinePlotGeometry.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwStationHandle.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"

// Test helpers
#include "ExternalCenterlineTestHelpers.h"
#include "LoadProjectHelper.h"
#include "SurveyTreeTestHelper.h"
#include "TestHelper.h"

// Qt
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>

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
// fixes itself at: the .3d names a leg's endpoints by coordinate, so two surveys
// sharing a point would trade legs in the solved network.
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
    const QStringList keys = node->stationPositionLookup().positions().keys();
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

cwTrip* tripNamed(const cwSurveyNode* node, const QString& name)
{
    const QList<cwTrip*> trips = node->trips();
    for (cwTrip* trip : trips) {
        if (trip->name() == name) {
            return trip;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("A nested tree solves into one lookup per node, each holding its subtree",
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
    // it and the station's own node.
    const QVector3D u2 = tree.section->stationPositionLookup().position(QStringLiteral("u2"));
    CHECK(tree.sideCave->stationPositionLookup().position(QStringLiteral("upper_level.u2")) == u2);
    CHECK(tree.folder->stationPositionLookup().position(QStringLiteral("side_cave.upper_level.u2")) == u2);

    // The equates hold: u1 sits on s2, and s1 on k2.
    const QVector3D u1 = tree.section->stationPositionLookup().position(QStringLiteral("u1"));
    const QVector3D s2 = tree.sideCave->stationPositionLookup().position(QStringLiteral("s2"));
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
    const auto result = cwLinePlotGeometry::generate(region.data(), manager.regionNetwork());
    REQUIRE_FALSE(result.hasError());
    const cwLinePlotGeometry::Result geometry = result.value();

    for (const cwTrip* trip : tree.trips) {
        INFO("trip: " << trip->name().toStdString());
        CHECK(vertexCountOf(geometry, trip) > 0);
    }
    CHECK(movedTrips.contains(tree.section->trip(0)));
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

    // Hand-set solved positions: the geometry reads each node's own lookup.
    constexpr float kUpperFloor = 5.0f;
    constexpr float kLowerFloor = -20.0f;
    cwStationPositionLookup upperLookup;
    upperLookup.setPosition(QStringLiteral("a1"), QVector3D(0.0f, 0.0f, kUpperFloor));
    upperLookup.setPosition(QStringLiteral("a2"), QVector3D(0.0f, float(kShotLength), kUpperFloor));
    upper->setStationPositionLookup(upperLookup);

    cwStationPositionLookup lowerLookup;
    lowerLookup.setPosition(QStringLiteral("b1"), QVector3D(0.0f, 0.0f, kLowerFloor + float(kDropLength)));
    lowerLookup.setPosition(QStringLiteral("b2"), QVector3D(0.0f, 0.0f, kLowerFloor));
    lower->setStationPositionLookup(lowerLookup);

    const auto result = cwLinePlotGeometry::generate(region.data());
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

    const auto result = cwLinePlotGeometry::generate(region->data(), manager->regionNetwork());
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

    SECTION("The unconnected Entrance survey warns on its trip and rolls up to Fisher Ridge")
    {
        const std::optional<cwError> entry = unconnectedStationsEntry(entranceSurvey->errorModel());
        REQUIRE(entry.has_value());
        CHECK(entry->type() == cwError::Warning);
        CHECK(entry->message() == QStringLiteral("2 stations in Entrance survey are not tied to the cave"));
        CHECK(entry->detail() == QStringLiteral("a1, a2"));
        CHECK_FALSE(unconnectedStationsEntry(crystalCrawl->errorModel()).has_value());
        CHECK_FALSE(unconnectedStationsEntry(fisherRidge->errorModel()).has_value());

        // The node counts its trips' warnings, so silencing the trip's entry
        // takes exactly one off the node.
        cwErrorListModel* entranceErrors = entranceSurvey->errorModel()->errors();
        const int nodeWarnings = fisherRidge->errorModel()->warningCount();
        const int row = entranceErrors->indexOf(*entry);
        REQUIRE(row >= 0);
        REQUIRE(entranceErrors->setData(entranceErrors->index(row), true,
                                        static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole)));
        CHECK(fisherRidge->errorModel()->warningCount() == nodeWarnings - 1);
    }

    SECTION("Tying the surveys in clears each warning on the next solve")
    {
        // Joining Fisher Ridge lets cavern run, and cavern then drops the
        // Section, which nothing ties to its Cave yet.
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
