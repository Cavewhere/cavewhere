/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwAttachedFixModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwExternalCenterline.h"
#include "cwExternalCenterlineManager.h"
#include "cwFixStationModel.h"
#include "cwFixStationTreeModel.h"
#include "cwGeoReference.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSurveyNode.h"
#include "ExternalCenterlineTestHelpers.h"

// Async
#include <asyncfuture.h>

// Qt
#include <QAbstractItemModel>
#include <QPointer>
#include <QSignalSpy>
#include <QStringList>
#include <QUuid>

// Std
#include <memory>

namespace {

// ridge.svx fixes R0 itself and includes three files, each one *begin block:
// entrances (E1 reference, E2, both bare), deep/lower (L1 under *cs UTM16N),
// and deep/empty (no fix).
const QString kSubtreeFixture = QStringLiteral("survex_subtree_fixes/ridge.svx");
const QString kRootName = QStringLiteral("ridge");
const QString kEntrancesName = QStringLiteral("entrances");
const QString kLowerName = QStringLiteral("lower");
const QString kEmptyName = QStringLiteral("empty");
const QString kRootFix = QStringLiteral("R0");
const QString kEntranceReferenceFix = QStringLiteral("entrances.E1");
const QString kEntrancePlainFix = QStringLiteral("entrances.E2");
const QString kLowerFix = QStringLiteral("lower.L1");
const QString kLowerSystem = QStringLiteral("UTM16N");
constexpr int kFixtureFixCount = 4;
constexpr int kFixtureBareFixCount = 3;
constexpr int kEntrancesFixCount = 2;

// WGS 84 / UTM zone 48N: the project's frame in the georeferenced cases.
const QString kUtm48NFrame = QStringLiteral("EPSG:32648");
const QString kUtm48N = QStringLiteral("UTM48N");

const QString kBareFixFixture = QStringLiteral("survex_bare_fix/bare_fix.svx");
const QString kBareFixFileName = QStringLiteral("bare_fix.svx");
// bare_fix.svx's leg A-B runs 10 m east, so this native fix agrees with A.
const QString kNativeFixOnBareFile = QStringLiteral("bare.B");
constexpr double kNativeFixEasting = 500010.0;
constexpr double kNativeFixNorthing = 3000000.0;
constexpr double kNativeFixElevation = 100.0;
// cavern's error 438, the one a bare *fix under a *cs out raises.
const QString kInputProjectionError = QStringLiteral("input projection isn't");

// compass_project/PROJECT.MAK fixes A1 on ALPHA.DAT's line and C1 on
// CHARLIE.DAT's; BRAVO.DAT fixes nothing.
const QString kCompassFixture = QStringLiteral("compass_project/PROJECT.MAK");
const QString kAlphaDat = QStringLiteral("ALPHA.DAT");
const QString kBravoDat = QStringLiteral("BRAVO.DAT");
const QString kCharlieDat = QStringLiteral("CHARLIE.DAT");
constexpr int kCompassFixCount = 2;

// walls_two_caves/TWO.WPJ: the EAST book's E1.SRV fixes E1, the WEST book's
// W1.SRV fixes W1.
const QString kWallsFixture = QStringLiteral("walls_two_caves/TWO.WPJ");
const QString kEastBook = QStringLiteral("EAST");
const QString kWestBook = QStringLiteral("WEST");
constexpr int kWallsFixCount = 2;

const QString kNativeFolderName = QStringLiteral("North");
const QString kNativeCaveName = QStringLiteral("Pit");
const QString kNativeStation = QStringLiteral("p1");

std::unique_ptr<SavedProjectFixture> makeProject(const QString& projectFileBase)
{
    return makeSavedProject(projectFileBase, QStringLiteral("NativeCave"),
                            QStringLiteral("NativeTrip"));
}

void georeference(SavedProjectFixture* fixture)
{
    fixture->project->cavingRegion()->geoReference()->restore(cwGeoReference::Frozen,
                                                              kUtm48NFrame, {}, QString());
}

//! Attaches \a source under the region's root and waits until the scan has
//! listed \a fixCount fixes somewhere in the project.
cwCave* attachAndWaitForFixes(SavedProjectFixture* fixture, const QString& source, int fixCount)
{
    cwCave* root = attachSourceUnderRegion(fixture, fixturePath(source));
    const bool scanned = tryWait(kAttachWaitMs, [fixture, fixCount]() {
        int listed = 0;
        for (const cwSurveyNode* node : fixture->project->cavingRegion()->rootNode()->allNodes()) {
            listed += node->attachedFixes()->count();
        }
        return listed == fixCount;
    });
    drainPipelines(fixture);
    REQUIRE(scanned);
    return root;
}

//! Attaches the subtree fixture under the region's root and waits until the
//! scan has listed every fix of the file tree somewhere in the project.
cwCave* attachSubtreeFixture(SavedProjectFixture* fixture)
{
    return attachAndWaitForFixes(fixture, kSubtreeFixture, kFixtureFixCount);
}

//! The node under \a root whose sourcePath is \a sourcePath.
const cwSurveyNode* derivedNode(const cwSurveyNode* root, const QString& sourcePath)
{
    for (const cwSurveyNode* node : root->allNodes()) {
        if (node->sourcePath() == sourcePath) {
            return node;
        }
    }
    INFO("sourcePath " << sourcePath.toStdString());
    FAIL("No derived node has that sourcePath");
    return nullptr;
}

//! The tree's whole-node layout, for INFO lines.
std::string describeTree(const cwSurveyNode* node, int depth = 0)
{
    std::string text = std::string(static_cast<size_t>(depth) * 2, ' ')
                       + node->name().toStdString()
                       + " [" + node->sourcePath().toStdString() + "] fixes: "
                       + attachedFixStations(node).join(QStringLiteral(", ")).toStdString() + "\n";
    for (const cwSurveyNode* child : node->childNodes()) {
        text += describeTree(child, depth + 1);
    }
    return text;
}

int roleId(const QAbstractItemModel& model, const QByteArray& name)
{
    const int role = model.roleNames().key(name, -1);
    INFO("role " << name.toStdString());
    REQUIRE(role >= 0);
    return role;
}

int rowKind(const QModelIndex& index)
{
    return index.data(cwFixStationTreeModel::RowKindRole).toInt();
}

//! The node row named \a name anywhere below \a parent, depth-first.
QModelIndex nodeRow(const QAbstractItemModel& model, const QString& name,
                    const QModelIndex& parent = QModelIndex())
{
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex child = model.index(row, 0, parent);
        if (rowKind(child) != cwFixStationTreeModel::NodeRow) {
            continue;
        }
        if (child.data(cwFixStationTreeModel::NodeNameRole).toString() == name) {
            return child;
        }
        const QModelIndex found = nodeRow(model, name, child);
        if (found.isValid()) {
            return found;
        }
    }
    return QModelIndex();
}

//! The fix rows directly under \a nodeIndex, by station name.
QStringList fixRowStations(const QAbstractItemModel& model, const QModelIndex& nodeIndex)
{
    const int stationRole = roleId(model, QByteArrayLiteral("stationName"));
    QStringList stations;
    for (int row = 0; row < model.rowCount(nodeIndex); ++row) {
        const QModelIndex child = model.index(row, 0, nodeIndex);
        if (rowKind(child) == cwFixStationTreeModel::FixRow) {
            stations.append(child.data(stationRole).toString());
        }
    }
    return stations;
}

QModelIndex fixRow(const QAbstractItemModel& model, const QModelIndex& nodeIndex,
                   const QString& station)
{
    const int stationRole = roleId(model, QByteArrayLiteral("stationName"));
    for (int row = 0; row < model.rowCount(nodeIndex); ++row) {
        const QModelIndex child = model.index(row, 0, nodeIndex);
        if (rowKind(child) == cwFixStationTreeModel::FixRow
            && child.data(stationRole).toString() == station) {
            return child;
        }
    }
    return QModelIndex();
}

int nativeFixCount(const cwSurveyNode* root)
{
    int count = 0;
    root->walk([&count](const cwSurveyNode* node) { count += node->fixStations()->rowCount(); });
    return count;
}

//! A native fix in the project frame on \a node, at \a station.
void addNativeFix(cwSurveyNode* node, const QString& station, const QString& system,
                  double easting, double northing, double elevation)
{
    cwFixStationModel* fixes = node->fixStations();
    const int row = fixes->addFixStation(station);
    REQUIRE(row >= 0);
    const QModelIndex index = fixes->index(row);
    REQUIRE(fixes->setData(index, system, cwFixStationModel::InputCSRole));
    REQUIRE(fixes->setData(index, easting, cwFixStationModel::EastingRole));
    REQUIRE(fixes->setData(index, northing, cwFixStationModel::NorthingRole));
    REQUIRE(fixes->setData(index, elevation, cwFixStationModel::ElevationRole));
}

} // namespace

TEST_CASE("Attached fixes live on the derived node whose file holds them",
          "[FixStationTreeModel]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-rehome"));
    cwCave* root = attachSubtreeFixture(fixture.get());
    INFO("tree:\n" << describeTree(root));

    REQUIRE(root->name() == kRootName);
    REQUIRE(childNodeNames(root) == QStringList({kEntrancesName, kLowerName, kEmptyName}));
    const cwSurveyNode* entrances = childNodeNamed(root, kEntrancesName);
    const cwSurveyNode* lower = childNodeNamed(root, kLowerName);
    const cwSurveyNode* empty = childNodeNamed(root, kEmptyName);
    CHECK(entrances->sourcePath() == kEntrancesName);
    CHECK(lower->sourcePath() == kLowerName);
    CHECK(empty->sourcePath() == kEmptyName);

    CHECK(attachedFixStations(root) == QStringList({kRootFix}));
    CHECK(attachedFixStations(entrances) == QStringList({kEntranceReferenceFix, kEntrancePlainFix}));
    CHECK(attachedFixStations(lower) == QStringList({kLowerFix}));
    CHECK(attachedFixStations(empty).isEmpty());

    if (attachedFixStations(entrances).size() == kEntrancesFixCount) {
        CHECK(attachedFixFor(entrances, kEntranceReferenceFix).reference);
        CHECK_FALSE(attachedFixFor(entrances, kEntrancePlainFix).reference);
    }
    if (attachedFixStations(lower).size() == 1) {
        CHECK(attachedFixFor(lower, kLowerFix).coordinateSystem == kLowerSystem);
    }
}

TEST_CASE("A Compass project's fixes live on the .dat file each .mak line fixes",
          "[FixStationTreeModel]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-compass"));
    cwCave* root = attachAndWaitForFixes(fixture.get(), kCompassFixture, kCompassFixCount);
    INFO("tree:\n" << describeTree(root));

    CHECK(attachedFixStations(root).isEmpty());
    CHECK(attachedFixStations(derivedNode(root, kAlphaDat)) == QStringList({QStringLiteral("alpha.A1")}));
    CHECK(attachedFixStations(derivedNode(root, kBravoDat)).isEmpty());
    CHECK(attachedFixStations(derivedNode(root, kCharlieDat))
          == QStringList({QStringLiteral("charlie.C1")}));

    cwFixStationTreeModel model;
    model.setRootNode(root);
    REQUIRE(model.rowCount() == 1);
    CHECK(model.index(0, 0).data(cwFixStationTreeModel::SubtreeFixCountRole).toInt()
          == kCompassFixCount);
}

TEST_CASE("A Walls project's fixes live on the book whose survey file holds them",
          "[FixStationTreeModel]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-walls"));
    cwCave* root = attachAndWaitForFixes(fixture.get(), kWallsFixture, kWallsFixCount);
    INFO("tree:\n" << describeTree(root));

    CHECK(attachedFixStations(root).isEmpty());
    CHECK(attachedFixStations(derivedNode(root, kEastBook)) == QStringList({QStringLiteral("E1")}));
    CHECK(attachedFixStations(derivedNode(root, kWestBook)) == QStringList({QStringLiteral("W1")}));

    cwFixStationTreeModel model;
    model.setRootNode(root);
    REQUIRE(model.rowCount() == 1);
    const QModelIndex rootRow = model.index(0, 0);
    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeFixCountRole).toInt() == kWallsFixCount);
    // The page rows run depth-first: EAST's fix, then WEST's.
    const int stationRole = roleId(model, QByteArrayLiteral("stationName"));
    REQUIRE(model.fixRowCount() == kWallsFixCount);
    CHECK(model.indexOfFixRow(0).data(stationRole).toString() == QStringLiteral("E1"));
    CHECK(model.indexOfFixRow(1).data(stationRole).toString() == QStringLiteral("W1"));
    CHECK(model.indexOfFixRow(1).data(cwFixStationTreeModel::FixRowRole).toInt() == 1);
    CHECK_FALSE(model.indexOfFixRow(kWallsFixCount).isValid());
}

TEST_CASE("The fix station tree lists each node's fixes ahead of its child nodes",
          "[FixStationTreeModel]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-structure"));
    cwCave* root = attachSubtreeFixture(fixture.get());
    INFO("tree:\n" << describeTree(root));

    cwFixStationTreeModel model;
    model.setRootNode(root);

    REQUIRE(model.rowCount() == 1);
    const QModelIndex rootRow = model.index(0, 0);
    REQUIRE(rootRow.isValid());
    CHECK(rowKind(rootRow) == cwFixStationTreeModel::NodeRow);
    CHECK(rootRow.data(cwFixStationTreeModel::NodeNameRole).toString() == kRootName);
    CHECK(rootRow.data(cwFixStationTreeModel::NodeIdRole).toUuid() == root->id());

    // Fix rows first, then child node rows in tree order.
    const QStringList expectedNames({kEntrancesName, kLowerName, kEmptyName});
    REQUIRE(model.rowCount(rootRow) == 1 + expectedNames.size());
    const QModelIndex firstRow = model.index(0, 0, rootRow);
    CHECK(rowKind(firstRow) == cwFixStationTreeModel::FixRow);
    CHECK(fixRowStations(model, rootRow) == QStringList({kRootFix}));
    for (int i = 0; i < expectedNames.size(); ++i) {
        const QModelIndex child = model.index(1 + i, 0, rootRow);
        CHECK(rowKind(child) == cwFixStationTreeModel::NodeRow);
        CHECK(child.data(cwFixStationTreeModel::NodeNameRole).toString() == expectedNames.at(i));
    }

    const QModelIndex entrancesRow = nodeRow(model, kEntrancesName);
    REQUIRE(entrancesRow.isValid());
    CHECK(model.rowCount(entrancesRow) == kEntrancesFixCount);
    CHECK(entrancesRow.data(cwFixStationTreeModel::OwnFixCountRole).toInt() == kEntrancesFixCount);

    const QModelIndex emptyRow = nodeRow(model, kEmptyName);
    REQUIRE(emptyRow.isValid());
    CHECK(model.rowCount(emptyRow) == 0);
    CHECK(emptyRow.data(cwFixStationTreeModel::OwnFixCountRole).toInt() == 0);

    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeFixCountRole).toInt() == kFixtureFixCount);
    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeBareFixCountRole).toInt()
          == kFixtureBareFixCount);
    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeUnsetFixCountRole).toInt()
          == kFixtureBareFixCount);

    CHECK(rootRow.data(cwFixStationTreeModel::IsAttachmentOwnerRole).toBool());
    for (const QString& name : expectedNames) {
        INFO("node " << name.toStdString());
        CHECK_FALSE(nodeRow(model, name).data(cwFixStationTreeModel::IsAttachmentOwnerRole).toBool());
    }

    CHECK(model.setData(rootRow, kUtm48N, cwFixStationTreeModel::FixCoordinateSystemRole));
    CHECK(root->externalCenterline().fixCoordinateSystem() == kUtm48N);
    CHECK(rootRow.data(cwFixStationTreeModel::FixCoordinateSystemRole).toString() == kUtm48N);
    drainPipelines(fixture.get());

    CHECK(entrancesRow.data(cwFixStationTreeModel::InheritedFixCoordinateSystemRole).toString()
          == kUtm48N);
    CHECK(entrancesRow.data(cwFixStationTreeModel::InheritedFromNodeIdRole).toUuid() == root->id());

    const QModelIndex referenceRow = fixRow(model, entrancesRow, kEntranceReferenceFix);
    REQUIRE(referenceRow.isValid());
    CHECK(referenceRow.data(roleId(model, QByteArrayLiteral("reference"))).toBool());
    // Read in the owner's choice only where some *cs out is in force; this
    // project names none, so the georeferenced case is the one to read.
    georeference(fixture.get());
    drainPipelines(fixture.get());
    CHECK(tryWait(kAttachWaitMs, [&]() {
        return referenceRow.data(roleId(model, QByteArrayLiteral("readInSystem"))).toString()
               == kUtm48N;
    }));
    // Every bare fix now reads in the owner's choice, so none is left unset.
    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeBareFixCountRole).toInt()
          == kFixtureBareFixCount);
    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeUnsetFixCountRole).toInt() == 0);
}

TEST_CASE("The fix station tree follows added fixes and a detached source",
          "[FixStationTreeModel]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-live"));
    cwSurveyNode* regionRoot = fixture->project->cavingRegion()->rootNode();
    cwCave* north = addChildNode(regionRoot, kNativeFolderName, cwSurveyNode::Kind::Folder);
    cwCave* pit = addChildNode(north, kNativeCaveName, cwSurveyNode::Kind::Cave);
    cwCave* sourceRoot = attachSubtreeFixture(fixture.get());

    cwFixStationTreeModel model;
    model.setRootNode(regionRoot);

    REQUIRE(nodeRow(model, kNativeCaveName).isValid());
    CHECK(model.rowCount(nodeRow(model, kNativeCaveName)) == 0);

    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    addNativeFix(pit, kNativeStation, kUtm48NFrame, kNativeFixEasting, kNativeFixNorthing,
                 kNativeFixElevation);
    CHECK(inserted.count() >= 1);
    const QModelIndex pitRow = nodeRow(model, kNativeCaveName);
    REQUIRE(pitRow.isValid());
    CHECK(fixRowStations(model, pitRow) == QStringList({kNativeStation}));

    REQUIRE(nodeRow(model, kRootName).isValid());
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    auto future = managerOf(fixture.get())->deleteSourceRoot(sourceRoot);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("delete: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());

    CHECK(removed.count() >= 1);
    CHECK_FALSE(nodeRow(model, kRootName).isValid());
    CHECK_FALSE(nodeRow(model, kEntrancesName).isValid());
    CHECK(nodeRow(model, kNativeCaveName).isValid());
}

TEST_CASE("The region root's fix station tree counts every fix in the project",
          "[FixStationTreeModel]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-project"));
    cwSurveyNode* regionRoot = fixture->project->cavingRegion()->rootNode();
    cwCave* north = addChildNode(regionRoot, kNativeFolderName, cwSurveyNode::Kind::Folder);
    cwCave* pit = addChildNode(north, kNativeCaveName, cwSurveyNode::Kind::Cave);
    addNativeFix(pit, kNativeStation, kUtm48NFrame, kNativeFixEasting, kNativeFixNorthing,
                 kNativeFixElevation);
    attachSubtreeFixture(fixture.get());
    const int nativeFixes = nativeFixCount(regionRoot);
    REQUIRE(nativeFixes == 1);

    cwFixStationTreeModel model;
    model.setRootNode(regionRoot);

    REQUIRE(model.rowCount() == 1);
    const QModelIndex rootRow = model.index(0, 0);
    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeFixCountRole).toInt()
          == nativeFixes + kFixtureFixCount);
}

TEST_CASE("A native fix on the owner leaves no system in force at its file's include",
          "[FixStationTreeModel][BareFixCS]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-native-cs-scope"));
    georeference(fixture.get());
    cwCave* root = attachSourceUnderRegion(fixture.get(), fixturePath(kBareFixFixture));
    addNativeFix(root, kNativeFixOnBareFile, kUtm48NFrame, kNativeFixEasting, kNativeFixNorthing,
                 kNativeFixElevation);

    cwLinePlotManager* linePlot = fixture->rootData->linePlotManager();
    const bool solvedWithBoth = tryWait(kAttachWaitMs, [&]() {
        const QString driver = linePlot->driverSource();
        return driver.contains(kBareFixFileName)
               && driver.contains(QStringLiteral("*fix ") + kNativeFixOnBareFile);
    });
    drainPipelines(fixture.get());
    REQUIRE(solvedWithBoth);

    INFO("driver:\n" << linePlot->driverSource().toStdString());
    INFO("cavern log:\n" << linePlot->cavernLog().toStdString());
    CHECK(root->externalCenterline().fixCoordinateSystem().isEmpty());
    CHECK(linePlot->hasSolveError());
    CHECK(linePlot->solveErrorSummary().contains(kInputProjectionError));
    CHECK(bareFixWarnings(root).size() == 1);
}
