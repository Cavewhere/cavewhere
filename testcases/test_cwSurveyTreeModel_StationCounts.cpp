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

// Qt
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QSignalSpy>

namespace {

const QString kCountsFixture = QStringLiteral("survex_station_counts/counts.svx");
constexpr int kEventSliceMs = 10;
constexpr int kRefreshWaitMs = 5000;

cwTrip* tripNamed(const cwSurveyNode* node, const QString& name)
{
    for (cwTrip* trip : node->trips()) {
        if (trip->name() == name) {
            return trip;
        }
    }
    return nullptr;
}

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
