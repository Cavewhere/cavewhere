/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwNodePageModel.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSurveyTreeModel.h"
#include "cwTrip.h"
#include "ExternalCenterlineTestHelpers.h"
#include "SurveyTreeTestHelper.h"
#include "TestHelper.h"

// Qt
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QSignalSpy>

// Std
#include <optional>

namespace {

using SurveyTreeTestHelper::changesCovering;

const QString kCountsFixture = QStringLiteral("survex_station_counts/counts.svx");
constexpr int kEventSliceMs = 10;
constexpr int kRefreshWaitMs = 5000;

int stationCountOf(const cwSurveyTreeModel& model, QObject* object)
{
    const QModelIndex index = model.indexOf(object);
    REQUIRE(index.isValid());
    return model.data(index, cwSurveyTreeModel::StationCountRole).toInt();
}

int nodePageStationCount(cwSurveyNode* node)
{
    cwNodePageModel page;
    page.setNode(node);
    return page.stationCount();
}

int knownStationCount(const cwTrip* trip)
{
    return static_cast<int>(trip->knownStations().size());
}

//! True when \a spy saw a dataChanged covering \a rowIndex's Stations cell for StationCountRole.
bool sawStationsChange(const QSignalSpy& spy, const QModelIndex& rowIndex)
{
    for (const QList<QVariant>& arguments : spy) {
        const QModelIndex topLeft = arguments.at(0).value<QModelIndex>();
        const QModelIndex bottomRight = arguments.at(1).value<QModelIndex>();
        const QList<int> roles = arguments.at(2).value<QList<int>>();
        if (topLeft.parent() == rowIndex.parent()
            && topLeft.row() <= rowIndex.row()
            && bottomRight.row() >= rowIndex.row()
            && topLeft.column() <= cwSurveyTreeModel::Stations
            && bottomRight.column() >= cwSurveyTreeModel::Stations
            && (roles.isEmpty() || roles.contains(cwSurveyTreeModel::StationCountRole))) {
            return true;
        }
    }
    return false;
}

//! Spins the event loop until \a spy covers \a rowIndex's Stations cell, so a
//! queued refresh gets its turn.
bool waitForStationsChange(const QSignalSpy& spy, const QModelIndex& rowIndex)
{
    QDeadlineTimer deadline(kRefreshWaitMs);
    while (!deadline.hasExpired()) {
        if (sawStationsChange(spy, rowIndex)) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, kEventSliceMs);
    }
    return sawStationsChange(spy, rowIndex);
}

//! The count \a object's row shows: nothing while the row shows the
//! placeholder, the StationCountRole value otherwise.
std::optional<int> shownStationCount(const cwSurveyTreeModel& model, QObject* object)
{
    const QModelIndex index = model.indexOf(object);
    REQUIRE(index.isValid());
    if (!model.data(index, cwSurveyTreeModel::StationCountKnownRole).toBool()) {
        return std::nullopt;
    }
    return model.data(index, cwSurveyTreeModel::StationCountRole).toInt();
}

bool nodePageStationCountKnown(cwSurveyNode* node)
{
    cwNodePageModel page;
    page.setNode(node);
    return page.stationCountKnown();
}

} // namespace

TEST_CASE("Attached rows count the solved stations of their subtree at every level",
          "[SurveyTreeModel][Attach][StationCounts]")
{
    auto fixture = makeSavedProject(QStringLiteral("station-counts"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    cwCave* folder = addChildNode(region->rootNode(), QStringLiteral("Folder"),
                                  cwSurveyNode::Kind::Folder);

    cwSurveyTreeModel model;
    model.setRegion(region);
    QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);

    cwSurveyNode* root = attachSourceThroughManager(fixture.get(), folder, fixturePath(kCountsFixture));
    drainPipelines(fixture.get());

    cwSurveyNode* system = childNodeNamed(root, QStringLiteral("system"));
    REQUIRE(system != nullptr);
    cwSurveyNode* lower = childNodeNamed(system, QStringLiteral("lower"));
    REQUIRE(lower != nullptr);
    cwTrip* systemTrip = tripNamed(system, QStringLiteral("system"));
    cwTrip* upper = tripNamed(system, QStringLiteral("upper"));
    cwTrip* left = tripNamed(lower, QStringLiteral("left"));
    cwTrip* right = tripNamed(lower, QStringLiteral("right"));
    REQUIRE(systemTrip != nullptr);
    REQUIRE(upper != nullptr);
    REQUIRE(left != nullptr);
    REQUIRE(right != nullptr);

    // Each block's labels count once in their own block, so equated names
    // count apart: system s1-s3, upper u1-u4, left l1-l3, right r1-r2.
    constexpr int kSystemOwnStations = 3;
    constexpr int kUpperStations = 4;
    constexpr int kLeftStations = 3;
    constexpr int kRightStations = 2;
    constexpr int kLowerStations = kLeftStations + kRightStations;
    constexpr int kFileStations = kSystemOwnStations + kUpperStations + kLowerStations;

    SECTION("Every level reads the solved count")
    {
        CHECK(root->scopedPositions().positions().size() == kFileStations);
        CHECK(lower->scopedPositions().positions().size() == kLowerStations);
        CHECK(stationCountOf(model, root) == kFileStations);
        CHECK(stationCountOf(model, system) == kFileStations);
        CHECK(stationCountOf(model, lower) == kLowerStations);
        CHECK(stationCountOf(model, systemTrip) == kSystemOwnStations);
        CHECK(stationCountOf(model, upper) == kUpperStations);
        CHECK(stationCountOf(model, left) == kLeftStations);
        CHECK(stationCountOf(model, right) == kRightStations);
    }

    SECTION("A native Folder above the source root sums its sourced child")
    {
        CHECK(stationCountOf(model, folder) == kFileStations);
    }

    SECTION("An attached trip row reads its own known stations")
    {
        CHECK(stationCountOf(model, left) == knownStationCount(left));
        CHECK(stationCountOf(model, upper) == knownStationCount(upper));
    }

    SECTION("The node page and the tree row show one count")
    {
        CHECK(stationCountOf(model, root) == nodePageStationCount(root));
        CHECK(stationCountOf(model, lower) == nodePageStationCount(lower));
    }

    SECTION("A solve that moves positions announces the Stations cell of every attached level")
    {
        dataChanged.clear();
        QSignalSpy positionsChanged(region, &cwCavingRegion::positionsChanged);
        addNativeTripWithShot(fixture->cave, QStringLiteral("NativeShot"),
                              QStringLiteral("n1"), QStringLiteral("n2"));
        drainPipelines(fixture.get());
        REQUIRE(positionsChanged.count() > 0);

        CHECK(waitForStationsChange(dataChanged, model.indexOf(root)));
        CHECK(waitForStationsChange(dataChanged, model.indexOf(lower)));
        CHECK(waitForStationsChange(dataChanged, model.indexOf(left)));
    }

    SECTION("Moved member sets announce the Stations cell of every attached level")
    {
        dataChanged.clear();
        region->invalidateMemberViews();

        CHECK(waitForStationsChange(dataChanged, model.indexOf(root)));
        CHECK(waitForStationsChange(dataChanged, model.indexOf(lower)));
        CHECK(waitForStationsChange(dataChanged, model.indexOf(left)));
    }
}

TEST_CASE("An attach announces each ancestor row at most twice per event-loop turn",
          "[SurveyTreeModel][Attach][StationCounts][MainThreadBudget]")
{
    // The flat shape: many trips under one node, one event-loop turn of
    // inserts during the reconcile.
    constexpr int kBlocks = 200;
    constexpr int kShotsPerBlock = 3;
    constexpr int kMaxChangesPerRowPerTurn = 2;

    auto fixture = makeSavedProject(QStringLiteral("flat-coalesce"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    cwCave* folder = addChildNode(region->rootNode(), QStringLiteral("Folder"),
                                  cwSurveyNode::Kind::Folder);
    const QString source = writeFlatSurvex(tempSubdir(fixture->tempDir, QStringLiteral("flat")),
                                           kBlocks, kShotsPerBlock);

    cwSurveyTreeModel model;
    model.setRegion(region);
    drainPipelines(fixture.get());
    const QPersistentModelIndex folderIndex = model.indexOf(folder);
    REQUIRE(folderIndex.isValid());

    QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);
    auto future = managerOf(fixture.get())->attachSource(folder, source);

    cwSurveyNode* root = nullptr;
    int maxFolderChanges = 0;
    int maxRootChanges = 0;
    //One event-loop turn is the unit of measurement: a per-turn maximum is lost
    //by any wait that spans several turns.
    const auto stepOneTurn = [&]() {
        dataChanged.clear();
        QCoreApplication::processEvents();
        if (root == nullptr && folder->childNodeCount() > 0) {
            root = folder->childNode(0);
        }
        maxFolderChanges = std::max(maxFolderChanges, changesCovering(dataChanged, folderIndex));
        if (root != nullptr) {
            maxRootChanges = std::max(maxRootChanges, changesCovering(dataChanged, model.indexOf(root)));
        }
    };

    QDeadlineTimer deadline(kAttachWaitMs);
    while (!deadline.hasExpired()
           && !(future.isFinished() && root != nullptr
                && !root->scopedPositions().positions().isEmpty())) {
        stepOneTurn();
    }
    REQUIRE(waitForQueuedEvents());
    stepOneTurn();

    REQUIRE(future.isFinished());
    REQUIRE_FALSE(future.result().hasError());
    REQUIRE(root != nullptr);
    REQUIRE(root->tripCount() == kBlocks);
    REQUIRE_FALSE(root->scopedPositions().positions().isEmpty());

    CHECK(maxFolderChanges <= kMaxChangesPerRowPerTurn);
    CHECK(maxRootChanges <= kMaxChangesPerRowPerTurn);
}

TEST_CASE("Attached rows show the placeholder count until the first solve lands",
          "[SurveyTreeModel][Attach][StationCounts][MainThreadBudget]")
{
    auto fixture = makeSavedProject(QStringLiteral("station-count-placeholder"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    cwCave* folder = addChildNode(region->rootNode(), QStringLiteral("Folder"),
                                  cwSurveyNode::Kind::Folder);

    cwSurveyTreeModel model;
    model.setRegion(region);
    drainPipelines(fixture.get());

    cwSurveyNode* root = attachSourceThroughManager(fixture.get(), folder, fixturePath(kCountsFixture));
    cwSurveyNode* system = childNodeNamed(root, QStringLiteral("system"));
    REQUIRE(system != nullptr);
    cwSurveyNode* lower = childNodeNamed(system, QStringLiteral("lower"));
    REQUIRE(lower != nullptr);
    cwTrip* left = tripNamed(lower, QStringLiteral("left"));
    REQUIRE(left != nullptr);
    // The solve has not landed yet: the attach future completes first.
    REQUIRE(root->scopedPositions().positions().isEmpty());

    SECTION("Before the solve every sourced level and its native Folder show the placeholder")
    {
        const std::optional<int> rootCount = shownStationCount(model, root);
        const std::optional<int> lowerCount = shownStationCount(model, lower);
        const std::optional<int> leftCount = shownStationCount(model, left);
        const std::optional<int> folderCount = shownStationCount(model, folder);
        INFO("root " << rootCount.value_or(-1) << ", lower " << lowerCount.value_or(-1)
             << ", left " << leftCount.value_or(-1) << ", folder " << folderCount.value_or(-1));
        CHECK_FALSE(rootCount.has_value());
        CHECK_FALSE(lowerCount.has_value());
        CHECK_FALSE(leftCount.has_value());
        CHECK_FALSE(folderCount.has_value());
        CHECK_FALSE(nodePageStationCountKnown(root));
    }

    SECTION("After the solve every level shows the solved count")
    {
        drainPipelines(fixture.get());
        REQUIRE_FALSE(root->scopedPositions().positions().isEmpty());
        const int fileStations = static_cast<int>(root->scopedPositions().positions().size());

        CHECK(shownStationCount(model, root) == std::optional<int>(fileStations));
        CHECK(shownStationCount(model, lower)
              == std::optional<int>(static_cast<int>(lower->scopedPositions().positions().size())));
        CHECK(shownStationCount(model, left) == std::optional<int>(knownStationCount(left)));
        CHECK(shownStationCount(model, folder) == std::optional<int>(fileStations));
        CHECK(nodePageStationCountKnown(root));
    }
}

TEST_CASE("A source whose solve fails keeps the placeholder on its row and its native ancestor",
          "[SurveyTreeModel][Attach][StationCounts][MainThreadBudget]")
{
    auto fixture = makeSavedProject(QStringLiteral("station-count-failed-solve"),
                                    QStringLiteral("NativeCave"),
                                    QStringLiteral("NativeTrip"));
    cwCavingRegion* region = fixture->project->cavingRegion();
    cwCave* folder = addChildNode(region->rootNode(), QStringLiteral("Folder"),
                                  cwSurveyNode::Kind::Folder);

    cwSurveyTreeModel model;
    model.setRegion(region);

    cwSurveyNode* root = attachSourceThroughManager(
        fixture.get(), folder,
        fixturePath(QStringLiteral("survex_noisy_failure/noisy_failure.svx")));
    drainPipelines(fixture.get());

    // The file has survey content, and cavern never places it.
    REQUIRE(model.data(model.indexOf(root), cwSurveyTreeModel::HasSurveyContentRole).toBool());
    REQUIRE(root->scopedPositions().positions().isEmpty());

    const std::optional<int> rootCount = shownStationCount(model, root);
    const std::optional<int> folderCount = shownStationCount(model, folder);
    INFO("root " << rootCount.value_or(-1) << ", folder " << folderCount.value_or(-1));
    CHECK_FALSE(rootCount.has_value());
    CHECK_FALSE(folderCount.has_value());
    CHECK_FALSE(nodePageStationCountKnown(root));
}
