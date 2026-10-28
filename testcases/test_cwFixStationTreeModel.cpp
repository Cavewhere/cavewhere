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
#include "cwFixStationTreeFilterModel.h"
#include "cwGeoReference.h"
#include "cwLinePlotManager.h"
#include "cwNodeWarningModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSurveyNode.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"

// Async
#include <asyncfuture.h>

// Qt
#include <QAbstractItemModel>
#include <QPointer>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QUuid>

// Std
#include <memory>

namespace {

// ridge.svx fixes R0 itself and includes three files, each one leaf *begin
// block and so one derived trip: entrances (E1 reference, E2, both bare),
// deep/lower (L1 under *cs UTM16N), and deep/empty (no fix).
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

// compass_fix_units/UNITS.MAK fixes F1 in feet, [f,1234.5,678.9,10.0], and
// M1 in meters, [m,500.25,600.5,20.0]; it names no datum or zone.
const QString kMakUnitsFixture = QStringLiteral("compass_fix_units/UNITS.MAK");
const QString kFeetDat = QStringLiteral("FEET.DAT");
const QString kMetersDat = QStringLiteral("METERS.DAT");
const QString kFeetFix = QStringLiteral("feet.F1");
const QString kMetersFix = QStringLiteral("meters.M1");
// The values cavern solves with: feet times 0.3048, meters as written.
const QString kFeetFixMeters = QStringLiteral("376.2756 206.92872 3.048");
const QString kMetersFixMeters = QStringLiteral("500.25 600.5 20");
constexpr int kMakUnitsFixCount = 2;

// walls_two_caves/TWO.WPJ: the EAST book's E1.SRV fixes E1, the WEST book's
// W1.SRV fixes W1.
const QString kWallsFixture = QStringLiteral("walls_two_caves/TWO.WPJ");
const QString kEastBook = QStringLiteral("EAST");
const QString kWestBook = QStringLiteral("WEST");
constexpr int kWallsFixCount = 2;

// fix_tree_sort_filter/sorted.svx fixes st2, st1, st10 on the source root, in
// that order and with no *cs, and includes north.svx, whose *begin north
// fixes n10 then n9 under *cs UTM16N; north is a leaf, so a trip row.
const QString kSortFilterFixture = QStringLiteral("fix_tree_sort_filter/sorted.svx");
const QString kSortFilterNorthName = QStringLiteral("north");
const QString kSt1 = QStringLiteral("st1");
const QString kSt2 = QStringLiteral("st2");
const QString kSt3 = QStringLiteral("st3");
const QString kSt10 = QStringLiteral("st10");
constexpr double kSt10Easting = 100.0;
constexpr double kSt10Northing = 10.0;
constexpr double kSt10Elevation = 1.0;
const QString kNorthN9 = QStringLiteral("north.n9");
const QString kNorthN10 = QStringLiteral("north.n10");
constexpr int kSortFilterFixCount = 5;
// How childLabels() writes a node row, so it reads apart from a station.
const QString kNodeLabel = QStringLiteral("[%1]");

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
            listed += static_cast<int>(node->listedAttachedFixes().size());
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

//! \a node's trip named \a name.
const cwTrip* tripNamed(const cwSurveyNode* node, const QString& name)
{
    for (const cwTrip* trip : node->trips()) {
        if (trip->name() == name) {
            return trip;
        }
    }
    INFO("trip " << name.toStdString());
    FAIL("No trip has that name");
    return nullptr;
}

//! The fixes \a trip lists, by station.
QStringList tripFixStations(const cwTrip* trip)
{
    QStringList stations;
    for (const cwAttachedFix& fix : trip->attachedFixes()->attachedFixes()) {
        stations.append(fix.station);
    }
    return stations;
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

//! The trip row named \a name anywhere below \a parent, depth-first.
QModelIndex tripRow(const QAbstractItemModel& model, const QString& name,
                    const QModelIndex& parent = QModelIndex())
{
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex child = model.index(row, 0, parent);
        if (rowKind(child) == cwFixStationTreeModel::TripRow
            && child.data(cwFixStationTreeModel::NodeNameRole).toString() == name) {
            return child;
        }
        const QModelIndex found = tripRow(model, name, child);
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

//! The rows directly under \a parent in row order: a fix row by its station
//! name, a node or trip row by kNodeLabel around its name.
QStringList childLabels(const QAbstractItemModel& model, const QModelIndex& parent)
{
    const int stationRole = roleId(model, QByteArrayLiteral("stationName"));
    QStringList labels;
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex child = model.index(row, 0, parent);
        if (rowKind(child) == cwFixStationTreeModel::FixRow) {
            labels.append(child.data(stationRole).toString());
        } else {
            labels.append(kNodeLabel.arg(child.data(cwFixStationTreeModel::NodeNameRole).toString()));
        }
    }
    return labels;
}

//! Orders \a proxy's rows by \a role, through QSortFilterProxyModel's own sort
//! (cwSortFilterProxyModel's sort(Qt::SortOrder) hides it).
void sortBy(cwFixStationTreeFilterModel& proxy, int role, Qt::SortOrder order)
{
    proxy.setSortRole(role);
    QSortFilterProxyModel& base = proxy;
    base.sort(0, order);
}

//! A project with sorted.svx attached under the region's root, its tree model
//! on the source root and the filter model over the tree.
struct SortFilterFixture
{
    std::unique_ptr<SavedProjectFixture> project;
    cwCave* root = nullptr;
    cwFixStationTreeModel treeModel;
    cwFixStationTreeFilterModel proxy;

    explicit SortFilterFixture(const QString& projectFileBase) :
        project(makeProject(projectFileBase))
    {
        root = attachAndWaitForFixes(project.get(), kSortFilterFixture, kSortFilterFixCount);
        treeModel.setRootNode(root);
        proxy.setSourceModel(&treeModel);
    }

    QModelIndex rootRow() const { return proxy.index(0, 0); }
    QModelIndex northRow() const { return tripRow(proxy, kSortFilterNorthName); }
};

} // namespace

TEST_CASE("Attached fixes live on the derived trip whose leaf block holds them",
          "[FixStationTreeModel]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-rehome"));
    cwCave* root = attachSubtreeFixture(fixture.get());
    INFO("tree:\n" << describeTree(root));

    REQUIRE(root->name() == kRootName);
    CHECK(root->childNodeCount() == 0);
    // The root's own unit, then one trip per included leaf block.
    REQUIRE(tripNames(root) == QStringList({kRootName, kEntrancesName, kLowerName, kEmptyName}));
    const cwTrip* entrances = tripNamed(root, kEntrancesName);
    const cwTrip* lower = tripNamed(root, kLowerName);
    const cwTrip* empty = tripNamed(root, kEmptyName);
    CHECK(entrances->sourcePath() == kEntrancesName);
    CHECK(lower->sourcePath() == kLowerName);
    CHECK(empty->sourcePath() == kEmptyName);

    CHECK(attachedFixStations(root) == QStringList({kRootFix}));
    CHECK(tripFixStations(entrances) == QStringList({kEntranceReferenceFix, kEntrancePlainFix}));
    CHECK(tripFixStations(lower) == QStringList({kLowerFix}));
    CHECK(tripFixStations(empty).isEmpty());

    const QList<cwAttachedFix>& entranceFixes = entrances->attachedFixes()->attachedFixes();
    if (entranceFixes.size() == kEntrancesFixCount) {
        CHECK(entranceFixes.at(0).reference);
        CHECK_FALSE(entranceFixes.at(1).reference);
    }
    const QList<cwAttachedFix>& lowerFixes = lower->attachedFixes()->attachedFixes();
    if (lowerFixes.size() == 1) {
        CHECK(lowerFixes.at(0).coordinateSystem == kLowerSystem);
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

TEST_CASE("A Compass .mak fix row shows its coordinate in meters, numbers only",
          "[FixStationTreeModel][MakFixUnits]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-mak-units"));
    cwCave* root = attachAndWaitForFixes(fixture.get(), kMakUnitsFixture, kMakUnitsFixCount);
    INFO("tree:\n" << describeTree(root));

    const cwSurveyNode* feetNode = derivedNode(root, kFeetDat);
    const cwSurveyNode* metersNode = derivedNode(root, kMetersDat);
    REQUIRE(attachedFixStations(feetNode) == QStringList({kFeetFix}));
    REQUIRE(attachedFixStations(metersNode) == QStringList({kMetersFix}));

    const QString feetText = attachedFixFor(feetNode, kFeetFix).coordinate;
    const QString metersText = attachedFixFor(metersNode, kMetersFix).coordinate;
    CHECK(feetText.toStdString() == kFeetFixMeters.toStdString());
    CHECK(metersText.toStdString() == kMetersFixMeters.toStdString());

    cwFixStationTreeModel model;
    model.setRootNode(root);
    const int coordinateRole = roleId(model, QByteArrayLiteral("coordinateText"));
    const QModelIndex feetRow = fixRow(model, nodeRow(model, feetNode->name()), kFeetFix);
    const QModelIndex metersRow = fixRow(model, nodeRow(model, metersNode->name()), kMetersFix);
    REQUIRE(feetRow.isValid());
    REQUIRE(metersRow.isValid());
    CHECK(feetRow.data(coordinateRole).toString().toStdString()
          == kFeetFixMeters.toStdString());
    CHECK(metersRow.data(coordinateRole).toString().toStdString()
          == kMetersFixMeters.toStdString());
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

TEST_CASE("The fix station tree lists each node's fixes ahead of its trip rows",
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

    // Fix rows first, then a row per trip that holds fixes, in trip order;
    // empty fixes nothing, so it has no row.
    const QStringList expectedNames({kEntrancesName, kLowerName});
    REQUIRE(model.rowCount(rootRow) == 1 + expectedNames.size());
    const QModelIndex firstRow = model.index(0, 0, rootRow);
    CHECK(rowKind(firstRow) == cwFixStationTreeModel::FixRow);
    CHECK(fixRowStations(model, rootRow) == QStringList({kRootFix}));
    for (int i = 0; i < expectedNames.size(); ++i) {
        const QModelIndex child = model.index(1 + i, 0, rootRow);
        CHECK(rowKind(child) == cwFixStationTreeModel::TripRow);
        CHECK(child.data(cwFixStationTreeModel::NodeNameRole).toString() == expectedNames.at(i));
    }
    CHECK_FALSE(tripRow(model, kEmptyName).isValid());

    const QModelIndex entrancesRow = tripRow(model, kEntrancesName);
    REQUIRE(entrancesRow.isValid());
    CHECK(model.rowCount(entrancesRow) == kEntrancesFixCount);
    CHECK(entrancesRow.data(cwFixStationTreeModel::OwnFixCountRole).toInt() == kEntrancesFixCount);

    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeFixCountRole).toInt() == kFixtureFixCount);
    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeBareFixCountRole).toInt()
          == kFixtureBareFixCount);
    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeUnsetFixCountRole).toInt()
          == kFixtureBareFixCount);

    CHECK(rootRow.data(cwFixStationTreeModel::IsAttachmentOwnerRole).toBool());
    for (const QString& name : expectedNames) {
        INFO("trip " << name.toStdString());
        CHECK_FALSE(tripRow(model, name).data(cwFixStationTreeModel::IsAttachmentOwnerRole).toBool());
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
        const QModelIndex row = fixRow(model, tripRow(model, kEntrancesName), kEntranceReferenceFix);
        return row.data(roleId(model, QByteArrayLiteral("readInSystem"))).toString() == kUtm48N;
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
    REQUIRE(tripRow(model, kEntrancesName).isValid());
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    auto future = managerOf(fixture.get())->deleteSourceRoot(sourceRoot);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("delete: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());

    CHECK(removed.count() >= 1);
    CHECK_FALSE(nodeRow(model, kRootName).isValid());
    CHECK_FALSE(tripRow(model, kEntrancesName).isValid());
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

TEST_CASE("The region root's stored fix count is the row count the tree lists",
          "[FixStationTreeModel]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-project-parity"));
    cwSurveyNode* regionRoot = fixture->project->cavingRegion()->rootNode();
    cwCave* north = addChildNode(regionRoot, kNativeFolderName, cwSurveyNode::Kind::Folder);
    cwCave* pit = addChildNode(north, kNativeCaveName, cwSurveyNode::Kind::Cave);
    addNativeFix(pit, kNativeStation, kUtm48NFrame, kNativeFixEasting, kNativeFixNorthing,
                 kNativeFixElevation);
    cwCave* sourceRoot = attachSubtreeFixture(fixture.get());
    const int nativeFixes = nativeFixCount(regionRoot);
    REQUIRE(nativeFixes == 1);

    cwFixStationTreeModel model;
    model.setRootNode(regionRoot);

    CHECK(regionRoot->subtreeFixStationCount()
          == nativeFixes + kFixtureFixCount);
    CHECK(regionRoot->subtreeFixStationCount() == model.fixRowCount());
    CHECK(sourceRoot->subtreeFixStationCount() == kFixtureFixCount);
    CHECK(north->subtreeFixStationCount() == nativeFixes);

    //Deleting the source takes every attached fix with it.
    auto future = managerOf(fixture.get())->deleteSourceRoot(sourceRoot);
    REQUIRE(AsyncFuture::waitForFinished(future, kAttachWaitMs));
    INFO("delete: " << future.result().errorMessage().toStdString());
    REQUIRE_FALSE(future.result().hasError());
    drainPipelines(fixture.get());

    CHECK(regionRoot->subtreeFixStationCount() == nativeFixes);
    CHECK(regionRoot->subtreeFixStationCount() == model.fixRowCount());
}

TEST_CASE("A native fix on the owner leaves its system out of its file's include",
          "[FixStationTreeModel][BareFixCS][BareFixOrigin]")
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
    // The file's bare fix reads in CaveWhere's translation onto the project
    // origin rather than the native fix's system, and the project solves.
    CHECK_FALSE(linePlot->hasSolveError());
    CHECK_FALSE(linePlot->solveErrorSummary().contains(kInputProjectionError));
    CHECK(bareFixWarnings(root).size() == 1);
}

TEST_CASE("The fix station filter model orders sibling fixes by name, numbers by value",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-sort-name"));
    INFO("tree:\n" << describeTree(fixture.root));
    REQUIRE(childLabels(fixture.treeModel, fixture.treeModel.index(0, 0))
            == QStringList({kSt2, kSt1, kSt10, kNodeLabel.arg(kSortFilterNorthName)}));

    sortBy(fixture.proxy, cwFixStationModel::StationNameRole, Qt::AscendingOrder);

    REQUIRE(fixture.proxy.rowCount() == 1);
    CHECK(childLabels(fixture.proxy, fixture.rootRow())
          == QStringList({kSt1, kSt2, kSt10, kNodeLabel.arg(kSortFilterNorthName)}));
    REQUIRE(fixture.northRow().isValid());
    CHECK(childLabels(fixture.proxy, fixture.northRow()) == QStringList({kNorthN9, kNorthN10}));
}

TEST_CASE("A descending sort keeps node rows after their sibling fixes",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-sort-descending"));
    INFO("tree:\n" << describeTree(fixture.root));

    sortBy(fixture.proxy, cwFixStationModel::StationNameRole, Qt::DescendingOrder);

    REQUIRE(fixture.proxy.rowCount() == 1);
    CHECK(childLabels(fixture.proxy, fixture.rootRow())
          == QStringList({kSt10, kSt2, kSt1, kNodeLabel.arg(kSortFilterNorthName)}));
}

TEST_CASE("A coordinate sort orders file and native fixes by their numbers",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-sort-coordinate"));
    INFO("tree:\n" << describeTree(fixture.root));

    sortBy(fixture.proxy, cwFixStationModel::EastingRole, Qt::AscendingOrder);
    CHECK(childLabels(fixture.proxy, fixture.rootRow())
          == QStringList({kSt10, kSt2, kSt1, kNodeLabel.arg(kSortFilterNorthName)}));

    // st3's easting (500010) is past every file fix's, so it sorts after them.
    addNativeFix(fixture.root, kSt3, kUtm48NFrame, kNativeFixEasting, kNativeFixNorthing,
                 kNativeFixElevation);
    drainPipelines(fixture.project.get());
    CHECK(childLabels(fixture.proxy, fixture.rootRow())
          == QStringList({kSt10, kSt2, kSt1, kSt3, kNodeLabel.arg(kSortFilterNorthName)}));
}

TEST_CASE("A file fix row reads its coordinate's numbers through the tree",
          "[FixStationTreeModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-attached-components"));
    INFO("tree:\n" << describeTree(fixture.root));

    const QModelIndex st10 = fixRow(fixture.treeModel, fixture.treeModel.index(0, 0), kSt10);
    REQUIRE(st10.isValid());
    CHECK(st10.data(cwFixStationModel::CoordinateTextRole).toString() == QStringLiteral("100 10 1"));
    CHECK(st10.data(cwFixStationModel::EastingRole).toDouble() == kSt10Easting);
    CHECK(st10.data(cwFixStationModel::NorthingRole).toDouble() == kSt10Northing);
    CHECK(st10.data(cwFixStationModel::ElevationRole).toDouble() == kSt10Elevation);
}

TEST_CASE("A fix added while sorted takes its place by name",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-sort-live"));
    INFO("tree:\n" << describeTree(fixture.root));
    sortBy(fixture.proxy, cwFixStationModel::StationNameRole, Qt::AscendingOrder);

    addNativeFix(fixture.root, kSt3, kUtm48NFrame, kNativeFixEasting, kNativeFixNorthing,
                 kNativeFixElevation);
    drainPipelines(fixture.project.get());

    CHECK(childLabels(fixture.proxy, fixture.rootRow())
          == QStringList({kSt1, kSt2, kSt3, kSt10, kNodeLabel.arg(kSortFilterNorthName)}));
}

TEST_CASE("The filter keeps a matching fix and the node rows above it",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-filter-ancestors"));
    INFO("tree:\n" << describeTree(fixture.root));

    fixture.proxy.setFilterText(QStringLiteral("n9"));

    REQUIRE(fixture.proxy.rowCount() == 1);
    CHECK(childLabels(fixture.proxy, fixture.rootRow())
          == QStringList({kNodeLabel.arg(kSortFilterNorthName)}));
    const QModelIndex north = fixture.northRow();
    REQUIRE(north.isValid());
    CHECK(childLabels(fixture.proxy, north) == QStringList({kNorthN9}));
}

TEST_CASE("The filter matches station names as a case-insensitive substring",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-filter-case"));
    INFO("tree:\n" << describeTree(fixture.root));

    fixture.proxy.setFilterText(QStringLiteral("ST1"));

    REQUIRE(fixture.proxy.rowCount() == 1);
    CHECK(childLabels(fixture.proxy, fixture.rootRow()) == QStringList({kSt1, kSt10}));
    CHECK_FALSE(fixture.northRow().isValid());
}

TEST_CASE("A filter nothing matches keeps the page's own row, and clearing it restores every row",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-filter-none"));
    INFO("tree:\n" << describeTree(fixture.root));

    fixture.proxy.setFilterText(QStringLiteral("zzz"));
    REQUIRE(fixture.proxy.rowCount() == 1);
    CHECK(fixture.proxy.rowCount(fixture.rootRow()) == 0);

    fixture.proxy.setFilterText(QString());
    REQUIRE(fixture.proxy.rowCount() == 1);
    CHECK(childLabels(fixture.proxy, fixture.rootRow())
          == QStringList({kSt2, kSt1, kSt10, kNodeLabel.arg(kSortFilterNorthName)}));
    REQUIRE(fixture.northRow().isValid());
    CHECK(childLabels(fixture.proxy, fixture.northRow()) == QStringList({kNorthN10, kNorthN9}));
}

namespace {

const QByteArray kVisibleFixCountRoleName = QByteArrayLiteral("visibleFixCount");
const QString kNoMatchFilter = QStringLiteral("zzz");
constexpr int kSortFilterRootFixCount = 3;
constexpr int kSortFilterNorthFixCount = 2;
// "st1" matches st1 and st10; "n9" matches north.n9 alone.
constexpr int kSt1MatchCount = 2;
constexpr int kN9MatchCount = 1;

//! \a row's count of the fix rows under it that the filter shows.
int visibleFixCount(const QAbstractItemModel& model, const QModelIndex& row)
{
    return row.data(roleId(model, kVisibleFixCountRoleName)).toInt();
}

} // namespace

TEST_CASE("While filtering, a node row counts the fix rows the filter shows under it",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-filter-counts"));
    INFO("tree:\n" << describeTree(fixture.root));

    fixture.proxy.setFilterText(kSt1);

    const QModelIndex rootRow = fixture.rootRow();
    REQUIRE(rootRow.isValid());
    CHECK(childLabels(fixture.proxy, rootRow) == QStringList({kSt1, kSt10}));
    CHECK(rootRow.data(cwFixStationTreeModel::OwnFixCountRole).toInt() == kSortFilterRootFixCount);
    CHECK(visibleFixCount(fixture.proxy, rootRow) == kSt1MatchCount);
    CHECK_FALSE(fixture.northRow().isValid());
    CHECK(fixture.proxy.visibleFixRowCount() == kSt1MatchCount);
}

TEST_CASE("While filtering, a trip row counts its fix rows the filter shows",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-filter-trip-counts"));
    INFO("tree:\n" << describeTree(fixture.root));

    fixture.proxy.setFilterText(QStringLiteral("n9"));

    const QModelIndex rootRow = fixture.rootRow();
    REQUIRE(rootRow.isValid());
    CHECK(rootRow.data(cwFixStationTreeModel::OwnFixCountRole).toInt() == kSortFilterRootFixCount);
    CHECK(visibleFixCount(fixture.proxy, rootRow) == 0);
    const QModelIndex north = fixture.northRow();
    REQUIRE(north.isValid());
    CHECK(north.data(cwFixStationTreeModel::OwnFixCountRole).toInt() == kSortFilterNorthFixCount);
    CHECK(visibleFixCount(fixture.proxy, north) == kN9MatchCount);
    CHECK(fixture.proxy.visibleFixRowCount() == kN9MatchCount);
}

TEST_CASE("A filter nothing matches shows no fix rows, and clearing it counts every one again",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-filter-count-none"));
    INFO("tree:\n" << describeTree(fixture.root));
    CHECK(fixture.proxy.visibleFixRowCount() == kSortFilterFixCount);

    fixture.proxy.setFilterText(kNoMatchFilter);
    CHECK(fixture.proxy.visibleFixRowCount() == 0);
    CHECK(visibleFixCount(fixture.proxy, fixture.rootRow()) == 0);

    fixture.proxy.setFilterText(QString());
    CHECK(fixture.proxy.visibleFixRowCount() == kSortFilterFixCount);
    CHECK(visibleFixCount(fixture.proxy, fixture.rootRow()) == kSortFilterRootFixCount);
    REQUIRE(fixture.northRow().isValid());
    CHECK(visibleFixCount(fixture.proxy, fixture.northRow()) == kSortFilterNorthFixCount);
}

TEST_CASE("The visible fix count announces itself once per filter change",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-filter-count-signal"));
    QSignalSpy changed(&fixture.proxy, &cwFixStationTreeFilterModel::visibleFixRowCountChanged);

    fixture.proxy.setFilterText(kSt1);
    CHECK(changed.count() == 1);

    fixture.proxy.setFilterText(kNoMatchFilter);
    CHECK(changed.count() == 2);

    fixture.proxy.setFilterText(QString());
    CHECK(changed.count() == 3);
}

TEST_CASE("A fix added while filtering joins the counts when it matches",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    SortFilterFixture fixture(QStringLiteral("fix-tree-filter-count-live"));
    INFO("tree:\n" << describeTree(fixture.root));
    // "st" matches the root's st2, st1 and st10, and neither of north's fixes.
    constexpr int kStMatchCount = 3;

    fixture.proxy.setFilterText(QStringLiteral("st"));
    REQUIRE(fixture.proxy.visibleFixRowCount() == kStMatchCount);

    addNativeFix(fixture.root, kSt3, kUtm48NFrame, kNativeFixEasting, kNativeFixNorthing,
                 kNativeFixElevation);
    drainPipelines(fixture.project.get());

    CHECK(fixture.proxy.visibleFixRowCount() == kStMatchCount + 1);
    CHECK(visibleFixCount(fixture.proxy, fixture.rootRow()) == kStMatchCount + 1);
}

TEST_CASE("The bare-fix system set through a filtered row lands on that row's node",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-filter-set-data"));
    cwSurveyNode* regionRoot = fixture->project->cavingRegion()->rootNode();
    cwCave* north = addChildNode(regionRoot, kNativeFolderName, cwSurveyNode::Kind::Folder);
    cwCave* pit = addChildNode(north, kNativeCaveName, cwSurveyNode::Kind::Cave);
    addNativeFix(pit, kNativeStation, kUtm48NFrame, kNativeFixEasting, kNativeFixNorthing,
                 kNativeFixElevation);
    cwCave* attached = attachAndWaitForFixes(fixture.get(), kSortFilterFixture, kSortFilterFixCount);
    INFO("tree:\n" << describeTree(regionRoot));

    cwFixStationTreeModel treeModel;
    treeModel.setRootNode(regionRoot);
    cwFixStationTreeFilterModel proxy;
    proxy.setSourceModel(&treeModel);

    const QModelIndex sourceNorth = nodeRow(treeModel, kNativeFolderName);
    const QModelIndex sourceAttached = nodeRow(treeModel, attached->name());
    REQUIRE(sourceNorth.isValid());
    REQUIRE(sourceAttached.isValid());
    REQUIRE(sourceNorth.parent() == sourceAttached.parent());
    REQUIRE(sourceNorth.row() < sourceAttached.row());

    proxy.setFilterText(QStringLiteral("st"));
    CHECK_FALSE(nodeRow(proxy, kNativeFolderName).isValid());
    const QModelIndex proxyAttached = nodeRow(proxy, attached->name());
    REQUIRE(proxyAttached.isValid());
    REQUIRE(proxyAttached.row() != sourceAttached.row());

    CHECK(proxy.setData(proxyAttached, kUtm48N, cwFixStationTreeModel::FixCoordinateSystemRole));
    CHECK(attached->externalCenterline().fixCoordinateSystem() == kUtm48N);
    CHECK(sourceAttached.data(cwFixStationTreeModel::FixCoordinateSystemRole).toString() == kUtm48N);
    CHECK(sourceNorth.data(cwFixStationTreeModel::FixCoordinateSystemRole).toString().isEmpty());
    CHECK(north->externalCenterline().fixCoordinateSystem().isEmpty());
    drainPipelines(fixture.get());
}

namespace {

// survex_block_rows/leaf.svx: one leaf block, simmons_hall, whose bare *fix
// st1 is the derived trip's.
const QString kLeafFixture = QStringLiteral("survex_block_rows/leaf.svx");
const QString kLeafTripName = QStringLiteral("simmons_hall");
const QString kLeafFix = QStringLiteral("simmons_hall.st1");

// survex_block_rows/mixed.svx: block cave fixes c1 among its own shots and
// nests the leaf block inner, which fixes i2.
const QString kMixedFixture = QStringLiteral("survex_block_rows/mixed.svx");
const QString kMixedNodeName = QStringLiteral("cave");
const QString kMixedTripName = QStringLiteral("inner");
const QString kMixedNodeFix = QStringLiteral("cave.c1");
const QString kMixedTripFix = QStringLiteral("cave.inner.i2");
constexpr int kMixedFixCount = 2;

// survex_block_rows/shared_scopes.svx: shared.svx, which fixes s1, included
// from block one and from block two.
const QString kSharedFixture = QStringLiteral("survex_block_rows/shared_scopes.svx");
constexpr int kSharedFixCount = 2;

// survex_block_rows/order.svx: beta (a node: it nests gamma) fixes b1 in
// UTM48N, then the leaf alpha fixes a1 with no system.
const QString kOrderFixture = QStringLiteral("survex_block_rows/order.svx");
const QString kOrderBareFix = QStringLiteral("alpha.a1");
const QString kOrderTripName = QStringLiteral("alpha");
constexpr int kOrderFixCount = 2;

const QString kTripKindLabel = QStringLiteral("Trip");

//! Attaches \a source under the region's root, points \a model at the new
//! root, and waits until the model lists \a fixCount fix rows.
cwSurveyNode* attachForTree(SavedProjectFixture* fixture, cwFixStationTreeModel& model,
                            const QString& source, int fixCount)
{
    cwSurveyNode* root = attachSourceUnderRegion(fixture, fixturePath(source));
    model.setRootNode(root);
    const bool listed = tryWait(kAttachWaitMs, [&model, fixCount]() {
        return model.fixRowCount() == fixCount;
    });
    drainPipelines(fixture);
    INFO("fix rows listed: " << model.fixRowCount());
    CHECK(listed);
    return root;
}

} // namespace

TEST_CASE("A derived trip holding fixes is a trip row under its node, over its fix rows",
          "[FixStationTreeModel][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-trip-row"));
    georeference(fixture.get());
    cwFixStationTreeModel model;
    cwSurveyNode* root = attachForTree(fixture.get(), model, kLeafFixture, 1);

    const cwTrip* trip = nullptr;
    for (const cwTrip* candidate : root->trips()) {
        if (candidate->name() == kLeafTripName) {
            trip = candidate;
        }
    }
    REQUIRE(trip != nullptr);

    REQUIRE(model.rowCount() == 1);
    const QModelIndex rootRow = model.index(0, 0);
    CHECK(childLabels(model, rootRow) == QStringList({kNodeLabel.arg(kLeafTripName)}));

    const QModelIndex tripIndex = tripRow(model, kLeafTripName);
    REQUIRE(tripIndex.isValid());
    CHECK(tripIndex.parent() == rootRow);
    CHECK(tripIndex.data(cwFixStationTreeModel::NodeIdRole).toUuid() == trip->id());
    CHECK(tripIndex.data(cwFixStationTreeModel::NodeKindLabelRole).toString() == kTripKindLabel);
    CHECK(tripIndex.data(cwFixStationTreeModel::NodeObjectRole).value<cwSurveyNode*>() == root);
    CHECK(tripIndex.data(cwFixStationTreeModel::OwnFixCountRole).toInt() == 1);
    CHECK(tripIndex.data(cwFixStationTreeModel::SubtreeFixCountRole).toInt() == 1);
    CHECK(tripIndex.data(cwFixStationTreeModel::SubtreeBareFixCountRole).toInt() == 1);
    CHECK(tripIndex.data(cwFixStationTreeModel::SubtreeUnsetFixCountRole).toInt() == 1);
    CHECK_FALSE(tripIndex.data(cwFixStationTreeModel::IsAttachmentOwnerRole).toBool());
    CHECK(tripIndex.data(cwFixStationTreeModel::FixRowRole).toInt() == -1);
    CHECK_FALSE(model.flags(tripIndex).testFlag(Qt::ItemIsEditable));
    CHECK_FALSE(model.setData(tripIndex, kUtm48N, cwFixStationTreeModel::FixCoordinateSystemRole));

    CHECK(fixRowStations(model, tripIndex) == QStringList({kLeafFix}));
    const QModelIndex fixIndex = fixRow(model, tripIndex, kLeafFix);
    REQUIRE(fixIndex.isValid());
    CHECK(fixIndex.data(cwFixStationTreeModel::FixRowRole).toInt() == 0);
    CHECK(model.indexOfFixRow(0) == fixIndex);
    CHECK(fixIndex.data(cwFixStationTreeModel::NodeObjectRole).value<cwSurveyNode*>() == root);

    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeFixCountRole).toInt() == 1);
    CHECK(rootRow.data(cwFixStationTreeModel::AttachmentBareFixCountRole).toInt() == 1);

    // The owner's choice reaches the trip row as the system it inherits.
    REQUIRE(model.setData(rootRow, kUtm48N, cwFixStationTreeModel::FixCoordinateSystemRole));
    drainPipelines(fixture.get());
    CHECK(tripIndex.data(cwFixStationTreeModel::InheritedFixCoordinateSystemRole).toString()
          == kUtm48N);
    CHECK(tripIndex.data(cwFixStationTreeModel::InheritedFromNodeIdRole).toUuid() == root->id());
    CHECK(tryWait(kAttachWaitMs, [&]() {
        return tripIndex.data(cwFixStationTreeModel::SubtreeUnsetFixCountRole).toInt() == 0;
    }));
}

TEST_CASE("A node's trip rows sit after its own fixes, and page rows run through them",
          "[FixStationTreeModel][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-trip-row-order"));
    cwFixStationTreeModel model;
    attachForTree(fixture.get(), model, kMixedFixture, kMixedFixCount);

    const QModelIndex caveRow = nodeRow(model, kMixedNodeName);
    REQUIRE(caveRow.isValid());
    // cave's own trip holds no fix, so it has no row.
    CHECK(childLabels(model, caveRow)
          == QStringList({kMixedNodeFix, kNodeLabel.arg(kMixedTripName)}));
    CHECK(caveRow.data(cwFixStationTreeModel::OwnFixCountRole).toInt() == 1);
    CHECK(caveRow.data(cwFixStationTreeModel::SubtreeFixCountRole).toInt() == kMixedFixCount);

    const QModelIndex innerRow = tripRow(model, kMixedTripName);
    REQUIRE(innerRow.isValid());
    CHECK(rowKind(innerRow) == cwFixStationTreeModel::TripRow);
    CHECK(innerRow.parent() == caveRow);
    CHECK(fixRowStations(model, innerRow) == QStringList({kMixedTripFix}));

    const QModelIndex innerFix = fixRow(model, innerRow, kMixedTripFix);
    REQUIRE(innerFix.isValid());
    CHECK(innerFix.data(cwFixStationTreeModel::FixRowRole).toInt() == 1);
    CHECK(model.indexOfFixRow(1) == innerFix);
}

TEST_CASE("A file included from two scopes gives each scope's trip row its own fix",
          "[FixStationTreeModel][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-shared-scopes"));
    cwFixStationTreeModel model;
    attachForTree(fixture.get(), model, kSharedFixture, kSharedFixCount);

    const QModelIndex rootRow = model.index(0, 0);
    CHECK(childLabels(model, rootRow) == QStringList({kNodeLabel.arg(QStringLiteral("one")),
                                                      kNodeLabel.arg(QStringLiteral("two"))}));
    CHECK(fixRowStations(model, tripRow(model, QStringLiteral("one")))
          == QStringList({QStringLiteral("one.s1")}));
    CHECK(fixRowStations(model, tripRow(model, QStringLiteral("two")))
          == QStringList({QStringLiteral("two.s1")}));
}

TEST_CASE("Warning 603 opens the bare fix a derived trip holds, at its page row",
          "[FixStationTreeModel][cwNodeWarningModel][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-trip-warning"));
    georeference(fixture.get());
    cwFixStationTreeModel model;
    cwSurveyNode* root = attachForTree(fixture.get(), model, kOrderFixture, kOrderFixCount);

    // A node's trip rows come ahead of its child nodes, so alpha's bare fix
    // is the page's first row although beta is written first.
    const QModelIndex rootRow = model.index(0, 0);
    CHECK(childLabels(model, rootRow) == QStringList({kNodeLabel.arg(kOrderTripName),
                                                      kNodeLabel.arg(QStringLiteral("beta"))}));

    REQUIRE(tryWait(kAttachWaitMs, [root]() { return bareFixWarnings(root).size() == 1; }));
    cwNodeWarningModel warnings;
    warnings.setNode(root);
    int warningRow = -1;
    for (int row = 0; row < warnings.count(); ++row) {
        const auto target = static_cast<cwNodeWarningModel::Target>(
            warnings.data(warnings.index(row), cwNodeWarningModel::TargetRole).toInt());
        if (target == cwNodeWarningModel::Target::FixStationRow) {
            warningRow = row;
        }
    }
    REQUIRE(warningRow >= 0);
    const int pageRow =
        warnings.data(warnings.index(warningRow), cwNodeWarningModel::FixStationRowRole).toInt();
    CHECK(pageRow == 0);

    const QModelIndex target = model.indexOfFixRow(pageRow);
    REQUIRE(target.isValid());
    CHECK(target.data(roleId(model, QByteArrayLiteral("stationName"))).toString() == kOrderBareFix);
    CHECK(rowKind(target.parent()) == cwFixStationTreeModel::TripRow);
}

namespace {

const QString kAheadNodeName = QStringLiteral("Ahead");
const QString kBehindNodeName = QStringLiteral("Behind");
const QString kAheadFirstFix = QStringLiteral("a0");
const QString kAheadSecondFix = QStringLiteral("a1");
const QString kBehindFirstFix = QStringLiteral("b0");
const QString kBehindSecondFix = QStringLiteral("b1");
constexpr int kLeavingFixRow = -1;

//! Each fix row a FixRowRole dataChanged named, by station, with the page row
//! it read while the announcement was being emitted, in emission order.
struct FixRowAnnouncements
{
    QList<std::pair<QString, int>> rows;
    QObject context;

    explicit FixRowAnnouncements(const QAbstractItemModel& model)
    {
        const int stationRole = roleId(model, QByteArrayLiteral("stationName"));
        QObject::connect(&model, &QAbstractItemModel::dataChanged, &context,
                         [this, stationRole](const QModelIndex& topLeft,
                                             const QModelIndex& bottomRight,
                                             const QList<int>& roles) {
            if (!roles.contains(cwFixStationTreeModel::FixRowRole)) {
                return;
            }
            for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                const QModelIndex index = topLeft.siblingAtRow(row);
                rows.append({index.data(stationRole).toString(),
                             index.data(cwFixStationTreeModel::FixRowRole).toInt()});
            }
        });
    }

    //! The page rows \a station was announced with, in emission order.
    QList<int> announcedRows(const QString& station) const
    {
        QList<int> announced;
        for (const auto& [name, fixRow] : rows) {
            if (name == station) {
                announced.append(fixRow);
            }
        }
        return announced;
    }
};

//! Two sibling nodes under the region's root: Ahead fixes a0, Behind fixes
//! b0 and b1, so Behind's fix rows follow Ahead's on the page.
struct TwoNodeFixes
{
    std::unique_ptr<SavedProjectFixture> project;
    cwSurveyNode* regionRoot = nullptr;
    cwCave* ahead = nullptr;
    cwCave* behind = nullptr;
    cwFixStationTreeModel model;

    explicit TwoNodeFixes(const QString& projectFileBase, const QStringList& behindFixes) :
        project(makeProject(projectFileBase))
    {
        regionRoot = project->project->cavingRegion()->rootNode();
        ahead = addChildNode(regionRoot, kAheadNodeName, cwSurveyNode::Kind::Cave);
        behind = addChildNode(regionRoot, kBehindNodeName, cwSurveyNode::Kind::Cave);
        REQUIRE(ahead->fixStations()->addFixStation(kAheadFirstFix) >= 0);
        for (const QString& station : behindFixes) {
            REQUIRE(behind->fixStations()->addFixStation(station) >= 0);
        }
        model.setRootNode(regionRoot);
    }

    int pageRowOf(const QString& nodeName, const QString& station) const
    {
        return fixRow(model, nodeRow(model, nodeName), station)
            .data(cwFixStationTreeModel::FixRowRole)
            .toInt();
    }
};

} // namespace

TEST_CASE("A fix added ahead of other nodes' fixes announces their new page rows",
          "[FixStationTreeModel]")
{
    TwoNodeFixes fixes(QStringLiteral("fix-tree-row-added"), {kBehindFirstFix, kBehindSecondFix});
    REQUIRE(fixes.pageRowOf(kBehindNodeName, kBehindFirstFix) == 1);
    REQUIRE(fixes.pageRowOf(kBehindNodeName, kBehindSecondFix) == 2);

    FixRowAnnouncements announcements(fixes.model);
    REQUIRE(fixes.ahead->fixStations()->addFixStation(kAheadSecondFix) >= 0);

    CHECK(announcements.announcedRows(kBehindFirstFix) == QList<int>({2}));
    CHECK(announcements.announcedRows(kBehindSecondFix) == QList<int>({3}));
    CHECK(fixes.pageRowOf(kBehindNodeName, kBehindFirstFix) == 2);
    CHECK(fixes.pageRowOf(kBehindNodeName, kBehindSecondFix) == 3);
}

TEST_CASE("A fix row on its way out reads page row -1 while its removal is announced",
          "[FixStationTreeModel]")
{
    TwoNodeFixes fixes(QStringLiteral("fix-tree-row-leaving"), {kBehindFirstFix, kBehindSecondFix});

    FixRowAnnouncements announcements(fixes.model);
    fixes.ahead->fixStations()->removeAt(0);

    CHECK(announcements.announcedRows(kAheadFirstFix) == QList<int>({kLeavingFixRow}));
    CHECK(announcements.announcedRows(kBehindFirstFix) == QList<int>({0}));
    CHECK(announcements.announcedRows(kBehindSecondFix) == QList<int>({1}));
}

TEST_CASE("Removing a node announces the page rows of the fixes after it",
          "[FixStationTreeModel]")
{
    TwoNodeFixes fixes(QStringLiteral("fix-tree-node-removed"), {kBehindFirstFix});
    REQUIRE(fixes.pageRowOf(kBehindNodeName, kBehindFirstFix) == 1);

    FixRowAnnouncements announcements(fixes.model);
    fixes.regionRoot->removeNode(fixes.regionRoot->indexOfNode(fixes.ahead));

    CHECK_FALSE(nodeRow(fixes.model, kAheadNodeName).isValid());
    CHECK(announcements.announcedRows(kBehindFirstFix) == QList<int>({0}));
    CHECK(fixes.pageRowOf(kBehindNodeName, kBehindFirstFix) == 0);
}

namespace {

const QByteArray kBareFixOwnerTripsRoleName = QByteArrayLiteral("bareFixOwnerTrips");
const QString kSyntheticEntryFile = QStringLiteral("synthetic.svx");
const QString kTripName = QStringLiteral("t%1");
constexpr int kOwnerFixtureTripCount = 40;
constexpr int kFirstBareOwner = 3;
constexpr int kSecondBareOwner = 17;
//! Its attachment's only fix names its own system.
constexpr int kSystemOwner = 25;
//! Holds fixes of its own, so it has a trip row, and attaches nothing.
constexpr int kTripRowTrip = 30;

//! A fix \a owner's attachment lists at \a line, in \a system (bare when empty).
cwAttachedFix ownerFix(const cwTrip* owner, int line, const QString& system = QString())
{
    cwAttachedFix fix;
    fix.station = QStringLiteral("s%1").arg(line);
    fix.coordinate = QStringLiteral("0 0 0");
    fix.coordinateSystem = system;
    fix.fileName = kSyntheticEntryFile;
    fix.filePath = kSyntheticEntryFile;
    fix.line = line;
    fix.ownerId = owner->id();
    return fix;
}

//! The trips \a index's bareFixOwnerTrips role lists, in order.
QList<cwTrip*> ownerTrips(const QModelIndex& index, int role)
{
    return index.data(role).value<QList<cwTrip*>>();
}

//! The names of the trips \a index's bareFixOwnerTrips role lists, in order.
QStringList ownerTripNames(const QModelIndex& index, int role)
{
    QStringList names;
    for (const cwTrip* trip : ownerTrips(index, role)) {
        names.append(trip->name());
    }
    return names;
}

//! The dataChanged emissions \a spy caught that cover \a row and announce \a role.
int roleAnnouncements(const QSignalSpy& spy, const QModelIndex& row, int role)
{
    int count = 0;
    for (const QList<QVariant>& emission : spy) {
        const QModelIndex topLeft = emission.at(0).toModelIndex();
        const QModelIndex bottomRight = emission.at(1).toModelIndex();
        const QList<int> roles = emission.at(2).value<QList<int>>();
        const bool coversRow = topLeft.parent() == row.parent()
                               && topLeft.row() <= row.row() && row.row() <= bottomRight.row();
        if (coversRow && (roles.isEmpty() || roles.contains(role))) {
            ++count;
        }
    }
    return count;
}

//! A standalone node, outside any project so no scan replaces its attached
//! fixes, with kOwnerFixtureTripCount trips named t0, t1, …. Trips
//! kFirstBareOwner and kSecondBareOwner attach a file holding a bare fix,
//! kSystemOwner one whose fix names its system; the node lists those fixes.
struct BareFixOwnerFixture
{
    cwCave node;
    cwFixStationTreeModel model;

    BareFixOwnerFixture()
    {
        for (int i = 0; i < kOwnerFixtureTripCount; ++i) {
            node.addTrip();
            node.trip(i)->setName(kTripName.arg(i));
        }
        for (const int owner : {kFirstBareOwner, kSecondBareOwner, kSystemOwner}) {
            node.trip(owner)->setExternalCenterline(cwExternalCenterline(kSyntheticEntryFile));
        }
        node.setAttachedFixes({ownerFix(trip(kFirstBareOwner), 1),
                               ownerFix(trip(kSecondBareOwner), 2),
                               ownerFix(trip(kSystemOwner), 3, kUtm48N)});
        model.setRootNode(&node);
    }

    cwTrip* trip(int i) const { return node.trip(i); }
    QModelIndex rootRow() const { return model.index(0, 0); }
    int role() const { return roleId(model, kBareFixOwnerTripsRoleName); }
    QStringList ownerNames() const { return ownerTripNames(rootRow(), role()); }
};

} // namespace

TEST_CASE("A node row lists the trips whose attachment holds a fix with no coordinate system",
          "[FixStationTreeModel]")
{
    BareFixOwnerFixture fixture;
    fixture.trip(kTripRowTrip)->setAttachedFixes({ownerFix(fixture.trip(kTripRowTrip), 4)});
    const int role = fixture.role();

    CHECK(fixture.ownerNames()
          == QStringList({kTripName.arg(kFirstBareOwner), kTripName.arg(kSecondBareOwner)}));

    const QModelIndex root = fixture.rootRow();
    int tripRows = 0;
    for (int row = 0; row < fixture.model.rowCount(root); ++row) {
        const QModelIndex child = fixture.model.index(row, 0, root);
        if (rowKind(child) == cwFixStationTreeModel::TripRow) {
            ++tripRows;
        }
        INFO("row " << row << " kind " << rowKind(child));
        CHECK(ownerTripNames(child, role).isEmpty());
    }
    CHECK(tripRows == 1);
}

TEST_CASE("A node row announces its bare-fix owner trips only when the list changes",
          "[FixStationTreeModel]")
{
    BareFixOwnerFixture fixture;
    const int role = fixture.role();
    QSignalSpy spy(&fixture.model, &QAbstractItemModel::dataChanged);

    // Committing a system: the rescan moves only what the fixes read in.
    QList<cwAttachedFix> readIn = fixture.node.attachedFixes()->attachedFixes();
    for (cwAttachedFix& fix : readIn) {
        fix.readInSystem = kUtm48N;
    }
    fixture.node.setAttachedFixes(readIn);
    cwExternalCenterline chosen = fixture.trip(kFirstBareOwner)->externalCenterline();
    chosen.setFixCoordinateSystem(kUtm48N);
    fixture.trip(kFirstBareOwner)->setExternalCenterline(chosen);
    CHECK(roleAnnouncements(spy, fixture.rootRow(), role) == 0);
    CHECK(fixture.ownerNames()
          == QStringList({kTripName.arg(kFirstBareOwner), kTripName.arg(kSecondBareOwner)}));

    spy.clear();
    QList<cwAttachedFix> withThirdOwner = readIn;
    withThirdOwner.append(ownerFix(fixture.trip(kSystemOwner), 5));
    fixture.node.setAttachedFixes(withThirdOwner);
    CHECK(roleAnnouncements(spy, fixture.rootRow(), role) == 1);
    CHECK(fixture.ownerNames()
          == QStringList({kTripName.arg(kFirstBareOwner), kTripName.arg(kSecondBareOwner),
                          kTripName.arg(kSystemOwner)}));

    spy.clear();
    fixture.trip(kSecondBareOwner)->setExternalCenterline(cwExternalCenterline());
    CHECK(roleAnnouncements(spy, fixture.rootRow(), role) == 1);
    CHECK(fixture.ownerNames()
          == QStringList({kTripName.arg(kFirstBareOwner), kTripName.arg(kSystemOwner)}));
}

TEST_CASE("A node row announces its subtree fix count when a trip with attached fixes arrives or leaves",
          "[FixStationTreeModel]")
{
    cwCave node;
    cwFixStationTreeModel model;
    model.setRootNode(&node);
    const QModelIndex rootRow = model.index(0, 0);
    QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);

    auto* arriving = new cwTrip();
    arriving->setAttachedFixes({ownerFix(arriving, 1), ownerFix(arriving, 2)});
    node.addTrip(arriving);
    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeFixCountRole).toInt() == 2);
    CHECK(roleAnnouncements(spy, rootRow, cwFixStationTreeModel::SubtreeFixCountRole) >= 1);

    spy.clear();
    node.removeTrip(0);
    CHECK(rootRow.data(cwFixStationTreeModel::SubtreeFixCountRole).toInt() == 0);
    CHECK(roleAnnouncements(spy, rootRow, cwFixStationTreeModel::SubtreeFixCountRole) >= 1);
}

TEST_CASE("A trip removed from its node leaves the node row's bare-fix owners before it is deleted",
          "[FixStationTreeModel]")
{
    BareFixOwnerFixture fixture;
    const int role = fixture.role();
    cwTrip* const leaving = fixture.trip(kFirstBareOwner);

    QList<cwTrip*> ownersAfterRemoval;
    QObject::connect(&fixture.node, &cwSurveyNode::removedTrips, &fixture.model, [&]() {
        ownersAfterRemoval = ownerTrips(fixture.rootRow(), role);
    });
    fixture.node.removeTrip(kFirstBareOwner);

    CHECK_FALSE(ownersAfterRemoval.contains(leaving));
    CHECK(ownersAfterRemoval.size() == 1);
    CHECK(fixture.ownerNames() == QStringList({kTripName.arg(kSecondBareOwner)}));
}

namespace {

const QByteArray kDelegateKindRoleName = QByteArrayLiteral("delegateKind");
const QString kKindEntryFile = QStringLiteral("kinds.svx");
const QString kKindTripName = QStringLiteral("held");
// Named so a sort by station interleaves the node's own fixes with its
// attached ones.
const QString kOwnFirst = QStringLiteral("k1");
const QString kAttachedSecond = QStringLiteral("k2");
const QString kOwnThird = QStringLiteral("k3");
const QString kAttachedFourth = QStringLiteral("k4");
const QString kTripHeldFix = QStringLiteral("k5");
constexpr int kAttachedSecondLine = 1;
constexpr int kAttachedFourthLine = 2;
constexpr int kTripHeldFixLine = 3;

//! A fix \a ownerId's attachment lists at \a line, as \a station.
cwAttachedFix kindFix(const QString& station, const QUuid& ownerId, int line)
{
    cwAttachedFix fix;
    fix.station = station;
    fix.coordinate = QStringLiteral("0 0 0");
    fix.fileName = kKindEntryFile;
    fix.filePath = kKindEntryFile;
    fix.line = line;
    fix.ownerId = ownerId;
    return fix;
}

int delegateKind(const QModelIndex& index)
{
    return index.data(cwFixStationTreeModel::DelegateKindRole).toInt();
}

//! A standalone node, outside any project so no scan replaces its attached
//! fixes: two fixes of its own, two its attachment lists, and one trip whose
//! attachment lists one more, so the trip has a row.
struct DelegateKindFixture
{
    cwCave node;
    cwFixStationTreeModel model;
    cwFixStationTreeFilterModel proxy;

    DelegateKindFixture()
    {
        node.addTrip();
        cwTrip* trip = node.trip(0);
        trip->setName(kKindTripName);
        node.fixStations()->addFixStation(kOwnFirst);
        node.fixStations()->addFixStation(kOwnThird);
        node.setAttachedFixes({kindFix(kAttachedSecond, node.id(), kAttachedSecondLine),
                               kindFix(kAttachedFourth, node.id(), kAttachedFourthLine)});
        trip->setAttachedFixes({kindFix(kTripHeldFix, trip->id(), kTripHeldFixLine)});
        model.setRootNode(&node);
        proxy.setSourceModel(&model);
    }
};

//! The delegate kind each fix station of DelegateKindFixture's rows answers.
QHash<QString, int> expectedFixKinds()
{
    return {
        {kOwnFirst, cwFixStationTreeModel::EditableFix},
        {kOwnThird, cwFixStationTreeModel::EditableFix},
        {kAttachedSecond, cwFixStationTreeModel::ReadOnlyFix},
        {kAttachedFourth, cwFixStationTreeModel::ReadOnlyFix},
        {kTripHeldFix, cwFixStationTreeModel::ReadOnlyFix}
    };
}

//! Checks every row of \a model: a node row answers NodeDelegate, a trip row
//! TripDelegate, and a fix row the kind \a expected names for its station,
//! with RowKindRole still FixRow. Returns the stations of the fix rows seen.
QStringList checkDelegateKinds(const QAbstractItemModel& model, const QModelIndex& parent,
                               const QHash<QString, int>& expected)
{
    const int stationRole = roleId(model, QByteArrayLiteral("stationName"));
    QStringList stations;
    for (int row = 0; row < model.rowCount(parent); ++row) {
        const QModelIndex child = model.index(row, 0, parent);
        const int kind = rowKind(child);
        INFO("row " << row << " rowKind " << kind);
        switch (kind) {
        case cwFixStationTreeModel::NodeRow:
            CHECK(delegateKind(child) == cwFixStationTreeModel::NodeDelegate);
            break;
        case cwFixStationTreeModel::TripRow:
            CHECK(delegateKind(child) == cwFixStationTreeModel::TripDelegate);
            break;
        case cwFixStationTreeModel::FixRow: {
            const QString station = child.data(stationRole).toString();
            INFO("station " << station.toStdString());
            REQUIRE(expected.contains(station));
            CHECK(delegateKind(child) == expected.value(station));
            stations.append(station);
            break;
        }
        default:
            FAIL("A row answers no RowKind");
        }
        stations.append(checkDelegateKinds(model, child, expected));
    }
    return stations;
}

} // namespace

TEST_CASE("Own fix rows and attached fix rows answer separate delegate kinds",
          "[FixStationTreeModel][FixStationTreeFilterModel]")
{
    DelegateKindFixture fixture;
    const QHash<QString, int> expected = expectedFixKinds();
    QStringList allStations = expected.keys();
    allStations.sort();

    SECTION("through the tree model")
    {
        CHECK(roleId(fixture.model, kDelegateKindRoleName) == cwFixStationTreeModel::DelegateKindRole);
        const QModelIndex root = fixture.model.index(0, 0);
        CHECK(rowKind(root) == cwFixStationTreeModel::NodeRow);
        CHECK(delegateKind(root) == cwFixStationTreeModel::NodeDelegate);

        const QModelIndex trip = tripRow(fixture.model, kKindTripName);
        REQUIRE(trip.isValid());
        CHECK(delegateKind(trip) == cwFixStationTreeModel::TripDelegate);
        const QModelIndex tripFix = fixRow(fixture.model, trip, kTripHeldFix);
        REQUIRE(tripFix.isValid());
        CHECK(rowKind(tripFix) == cwFixStationTreeModel::FixRow);
        CHECK(delegateKind(tripFix) == cwFixStationTreeModel::ReadOnlyFix);

        const QModelIndex own = fixRow(fixture.model, root, kOwnFirst);
        const QModelIndex attached = fixRow(fixture.model, root, kAttachedSecond);
        REQUIRE(own.isValid());
        REQUIRE(attached.isValid());
        CHECK(rowKind(own) == cwFixStationTreeModel::FixRow);
        CHECK(rowKind(attached) == cwFixStationTreeModel::FixRow);
        CHECK(delegateKind(own) == cwFixStationTreeModel::EditableFix);
        CHECK(delegateKind(attached) == cwFixStationTreeModel::ReadOnlyFix);

        QStringList seen = checkDelegateKinds(fixture.model, QModelIndex(), expected);
        seen.sort();
        CHECK(seen == allStations);
    }

    SECTION("through the filter proxy, unsorted and sorted by station")
    {
        CHECK(roleId(fixture.proxy, kDelegateKindRoleName) == cwFixStationTreeModel::DelegateKindRole);
        QStringList seen = checkDelegateKinds(fixture.proxy, QModelIndex(), expected);
        seen.sort();
        CHECK(seen == allStations);

        sortBy(fixture.proxy, roleId(fixture.proxy, QByteArrayLiteral("stationName")),
               Qt::AscendingOrder);
        const QModelIndex root = fixture.proxy.index(0, 0);
        const QStringList rootLabels = childLabels(fixture.proxy, root);
        INFO("root rows " << rootLabels.join(QStringLiteral(", ")).toStdString());
        REQUIRE(rootLabels.mid(0, 4)
                == QStringList({kOwnFirst, kAttachedSecond, kOwnThird, kAttachedFourth}));
        seen = checkDelegateKinds(fixture.proxy, QModelIndex(), expected);
        seen.sort();
        CHECK(seen == allStations);
    }
}

namespace {

const QString kAddCaveName = QStringLiteral("Spring Cave");
const QString kAddFolderName = QStringLiteral("Hills");
const QString kAddSourceRootName = QStringLiteral("Ridge File");
const QString kAddDerivedName = QStringLiteral("deep");
const QString kAddNestedFolderName = QStringLiteral("Notes");
const QString kAddDerivedSourcePath = QStringLiteral("deep");
const QString kAddCaveFix = QStringLiteral("c1");
const QByteArray kCanHoldFixesRoleName = QByteArrayLiteral("canHoldFixes");

//! A region whose nodes are marked by hand rather than by an attach: a native
//! cave and folder at the top, a source root holding one derived node and one
//! native folder, and the tree model on the region's root. The cave lists
//! kAddCaveFix.
struct AddTargetFixture
{
    cwCavingRegion region;
    cwSurveyNode* cave = nullptr;
    cwSurveyNode* folder = nullptr;
    cwSurveyNode* sourceRoot = nullptr;
    cwSurveyNode* derived = nullptr;
    cwSurveyNode* nestedFolder = nullptr;
    cwFixStationTreeModel model;

    AddTargetFixture()
    {
        cave = region.addNode(nullptr, cwSurveyNode::Kind::Cave, kAddCaveName);
        folder = region.addNode(nullptr, cwSurveyNode::Kind::Folder, kAddFolderName);
        sourceRoot = region.addNode(nullptr, cwSurveyNode::Kind::Cave, kAddSourceRootName);
        derived = region.addNode(sourceRoot, cwSurveyNode::Kind::Folder, kAddDerivedName);
        nestedFolder = region.addNode(sourceRoot, cwSurveyNode::Kind::Folder, kAddNestedFolderName);
        REQUIRE(cave != nullptr);
        REQUIRE(folder != nullptr);
        REQUIRE(sourceRoot != nullptr);
        REQUIRE(derived != nullptr);
        REQUIRE(nestedFolder != nullptr);

        const QUuid sourceId = QUuid::createUuid();
        sourceRoot->setSourceId(sourceId);
        sourceRoot->setReadOnly(true);
        derived->setSourceId(sourceId);
        derived->setSourcePath(kAddDerivedSourcePath);
        derived->setReadOnly(true);

        REQUIRE(cave->fixStations()->addFixStation(kAddCaveFix) == 0);
        model.setRootNode(region.rootNode());
    }

    QModelIndex rootRow() const { return model.index(0, 0); }
    QModelIndex row(const QString& name) const { return nodeRow(model, name); }
    QModelIndex caveFixRow() const { return fixRow(model, row(kAddCaveName), kAddCaveFix); }
    bool canHoldFixesAt(const QModelIndex& index) const
    {
        return index.data(roleId(model, kCanHoldFixesRoleName)).toBool();
    }
};

} // namespace

TEST_CASE("Only a native node or a source root can hold fixes of its own",
          "[FixStationTreeModel]")
{
    AddTargetFixture fixture;

    CHECK_FALSE(fixture.region.rootNode()->canHoldFixes());
    CHECK(fixture.cave->canHoldFixes());
    CHECK(fixture.folder->canHoldFixes());
    CHECK(fixture.sourceRoot->canHoldFixes());
    CHECK_FALSE(fixture.derived->canHoldFixes());
    CHECK(fixture.nestedFolder->canHoldFixes());
}

TEST_CASE("A node row and its fix rows answer whether the node can hold fixes",
          "[FixStationTreeModel]")
{
    AddTargetFixture fixture;

    CHECK_FALSE(fixture.canHoldFixesAt(fixture.rootRow()));
    CHECK(fixture.canHoldFixesAt(fixture.row(kAddCaveName)));
    CHECK(fixture.canHoldFixesAt(fixture.row(kAddFolderName)));
    CHECK(fixture.canHoldFixesAt(fixture.row(kAddSourceRootName)));
    CHECK_FALSE(fixture.canHoldFixesAt(fixture.row(kAddDerivedName)));
    CHECK(fixture.canHoldFixesAt(fixture.row(kAddNestedFolderName)));

    const QModelIndex caveFix = fixture.caveFixRow();
    REQUIRE(caveFix.isValid());
    CHECK(fixture.canHoldFixesAt(caveFix));

    // A fix set on the root directly, the way an older project held one,
    // answers for the root.
    REQUIRE(fixture.region.rootNode()->fixStations()->addFixStation(QStringLiteral("r1")) == 0);
    const QModelIndex rootFix = fixRow(fixture.model, fixture.rootRow(), QStringLiteral("r1"));
    REQUIRE(rootFix.isValid());
    CHECK_FALSE(fixture.canHoldFixesAt(rootFix));
}

TEST_CASE("Add Fix targets the current row's node, and the page's own node with no row",
          "[FixStationTreeModel]")
{
    AddTargetFixture fixture;
    cwSurveyNode* root = fixture.region.rootNode();

    CHECK(fixture.model.nodeRowIndex(root) == fixture.rootRow());
    CHECK(fixture.model.nodeRowIndex(fixture.cave) == fixture.row(kAddCaveName));
    CHECK(fixture.model.nodeRowIndex(fixture.derived) == fixture.row(kAddDerivedName));
    cwCave outside;
    CHECK_FALSE(fixture.model.nodeRowIndex(&outside).isValid());

    CHECK(fixture.model.addTargetAt(QModelIndex()) == root);
    CHECK(fixture.model.addTargetAt(fixture.rootRow()) == root);
    CHECK(fixture.model.addTargetAt(fixture.row(kAddCaveName)) == fixture.cave);
    CHECK(fixture.model.addTargetAt(fixture.caveFixRow()) == fixture.cave);
    CHECK(fixture.model.addTargetAt(fixture.row(kAddDerivedName)) == fixture.derived);
    CHECK(fixture.model.addTargetAt(fixture.row(kAddNestedFolderName)) == fixture.nestedFolder);

    SECTION("a cave page's tree targets the cave with no row") {
        cwFixStationTreeModel caveModel;
        caveModel.setRootNode(fixture.cave);
        CHECK(caveModel.addTargetAt(QModelIndex()) == fixture.cave);
    }
}

TEST_CASE("Add Fix refuses the region root and a derived node",
          "[FixStationTreeModel]")
{
    AddTargetFixture fixture;
    cwSurveyNode* root = fixture.region.rootNode();
    const int fixRows = fixture.model.fixRowCount();

    CHECK(fixture.model.addFixAt(QModelIndex()) == -1);
    CHECK(fixture.model.addFixAt(fixture.rootRow()) == -1);
    CHECK(fixture.model.addFixAt(fixture.row(kAddDerivedName)) == -1);

    CHECK(root->fixStations()->count() == 0);
    CHECK(fixture.derived->fixStations()->count() == 0);
    CHECK(fixture.model.fixRowCount() == fixRows);
}

TEST_CASE("Add Fix adds to a native node or a source root and returns the new page row",
          "[FixStationTreeModel]")
{
    AddTargetFixture fixture;

    const auto addAndCheck = [&fixture](const QModelIndex& current, cwSurveyNode* target) {
        INFO("target " << target->name().toStdString());
        const int before = target->fixStations()->count();
        const int fixRows = fixture.model.fixRowCount();
        const int pageRow = fixture.model.addFixAt(current);
        CHECK(target->fixStations()->count() == before + 1);
        CHECK(fixture.model.fixRowCount() == fixRows + 1);
        REQUIRE(pageRow >= 0);
        const QModelIndex added = fixture.model.indexOfFixRow(pageRow);
        REQUIRE(added.isValid());
        CHECK(added.data(cwFixStationTreeModel::NodeObjectRole).value<cwSurveyNode*>() == target);
        CHECK(added.data(cwFixStationTreeModel::FixRowRole).toInt() == pageRow);
        CHECK(added.row() == target->fixStations()->count() - 1);
    };

    addAndCheck(fixture.row(kAddCaveName), fixture.cave);
    addAndCheck(fixture.caveFixRow(), fixture.cave);
    addAndCheck(fixture.row(kAddFolderName), fixture.folder);
    addAndCheck(fixture.row(kAddSourceRootName), fixture.sourceRoot);
    addAndCheck(fixture.row(kAddNestedFolderName), fixture.nestedFolder);
    CHECK(fixture.region.rootNode()->fixStations()->count() == 0);
}

TEST_CASE("A node row announces whether it can hold fixes when its source changes",
          "[FixStationTreeModel]")
{
    AddTargetFixture fixture;
    const int role = roleId(fixture.model, kCanHoldFixesRoleName);
    REQUIRE(fixture.folder->fixStations()->addFixStation(QStringLiteral("h1")) == 0);
    const QModelIndex folderRow = fixture.row(kAddFolderName);
    const QModelIndex folderFix = fixRow(fixture.model, folderRow, QStringLiteral("h1"));
    REQUIRE(folderFix.isValid());
    CHECK(fixture.canHoldFixesAt(folderRow));

    QSignalSpy dataChanged(&fixture.model, &QAbstractItemModel::dataChanged);
    fixture.folder->setSourceId(fixture.sourceRoot->sourceId());
    fixture.folder->setSourcePath(QStringLiteral("hills"));

    CHECK(roleAnnouncements(dataChanged, folderRow, role) > 0);
    CHECK(roleAnnouncements(dataChanged, folderFix, role) > 0);
    CHECK_FALSE(fixture.canHoldFixesAt(folderRow));
    CHECK_FALSE(fixture.canHoldFixesAt(folderFix));
}

TEST_CASE("A trip row holds no fixes of its own, and Add on it targets its node",
          "[FixStationTreeModel][SurvexBlockRows]")
{
    auto fixture = makeProject(QStringLiteral("fix-tree-trip-row-add"));
    cwFixStationTreeModel model;
    cwSurveyNode* root = attachForTree(fixture.get(), model, kLeafFixture, 1);
    REQUIRE(root->isSourceRoot());
    const int role = roleId(model, kCanHoldFixesRoleName);

    const QModelIndex tripIndex = tripRow(model, kLeafTripName);
    REQUIRE(tripIndex.isValid());
    const QModelIndex tripFix = fixRow(model, tripIndex, kLeafFix);
    REQUIRE(tripFix.isValid());

    CHECK_FALSE(tripIndex.data(role).toBool());
    CHECK(tripFix.data(role).toBool());
    CHECK(model.addTargetAt(tripIndex) == root);
    CHECK(model.addTargetAt(tripFix) == root);

    const int pageRow = model.addFixAt(tripFix);
    CHECK(root->fixStations()->count() == 1);
    REQUIRE(pageRow >= 0);
    CHECK(model.indexOfFixRow(pageRow).data(cwFixStationTreeModel::NodeObjectRole)
              .value<cwSurveyNode*>() == root);
}

TEST_CASE("A native fix row reads and writes its kind and label through the tree",
          "[FixStationTreeModel][cwFixStationTreeModel]")
{
    const QString northSink = QStringLiteral("North sink");
    const QString sump = QStringLiteral("Sump");

    auto fixture = makeProject(QStringLiteral("fix-tree-kind-label"));
    cwSurveyNode* regionRoot = fixture->project->cavingRegion()->rootNode();
    cwCave* north = addChildNode(regionRoot, kNativeFolderName, cwSurveyNode::Kind::Folder);
    cwCave* pit = addChildNode(north, kNativeCaveName, cwSurveyNode::Kind::Cave);
    addNativeFix(pit, kNativeStation, kUtm48NFrame, kNativeFixEasting, kNativeFixNorthing,
                 kNativeFixElevation);
    cwFixStationModel* fixes = pit->fixStations();
    REQUIRE(fixes->setData(fixes->index(0), static_cast<int>(cwFixStation::Well),
                           cwFixStationModel::KindRole));
    REQUIRE(fixes->setData(fixes->index(0), northSink, cwFixStationModel::LabelRole));

    cwFixStationTreeModel model;
    model.setRootNode(regionRoot);

    CHECK(model.roleNames().value(cwFixStationModel::KindRole) == "kind");
    CHECK(model.roleNames().value(cwFixStationModel::LabelRole) == "label");

    const QModelIndex pitRow = nodeRow(model, kNativeCaveName);
    REQUIRE(pitRow.isValid());
    const QModelIndex row = fixRow(model, pitRow, kNativeStation);
    REQUIRE(row.isValid());
    CHECK(row.data(cwFixStationModel::KindRole).toInt() == cwFixStation::Well);
    CHECK(row.data(cwFixStationModel::LabelRole).toString() == northSink);

    QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);
    REQUIRE(model.setData(row, static_cast<int>(cwFixStation::Other), cwFixStationModel::KindRole));
    CHECK(fixes->fixStationAt(0).kind() == cwFixStation::Other);
    CHECK(roleAnnouncements(spy, row, cwFixStationModel::KindRole) == 1);

    spy.clear();
    REQUIRE(model.setData(row, sump, cwFixStationModel::LabelRole));
    CHECK(fixes->fixStationAt(0).label() == sump);
    CHECK(roleAnnouncements(spy, row, cwFixStationModel::LabelRole) == 1);
}
