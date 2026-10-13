/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

// Catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Our
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwLength.h"
#include "cwProject.h"
#include "cwRootData.h"
#include "cwSurveyTreeModel.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwUnits.h"
#include "ExternalCenterlineTestHelpers.h"
#include "TestHelper.h"

// Qt
#include <QSignalSpy>

namespace {

const QString kCountsFixture = QStringLiteral("survex_station_counts/counts.svx");
constexpr double kSolvedLengthMarginMeters = 0.05;

// counts.svx, leg by leg: system s1-s2-s3 is two 10 m legs, upper three 5 m
// legs, left two 4 m legs and right one 3 m leg. Equates add no legs.
constexpr double kSystemTripMeters = 20.0;
constexpr double kUpperMeters = 15.0;
constexpr double kLeftMeters = 8.0;
constexpr double kRightMeters = 3.0;
constexpr double kLowerMeters = kLeftMeters + kRightMeters;
constexpr double kFileMeters = kSystemTripMeters + kUpperMeters + kLowerMeters;

cwTrip* tripNamed(const cwSurveyNode* node, const QString& name)
{
    for (cwTrip* trip : node->trips()) {
        if (trip->name() == name) {
            return trip;
        }
    }
    return nullptr;
}

//! The Length cell of \a trip's row, in meters. The model hands a trip row its
//! length in the unit the trip's calibration surveys in.
double tripRowMeters(const cwSurveyTreeModel& model, cwTrip* trip)
{
    const QModelIndex index = model.indexOf(trip);
    REQUIRE(index.isValid());
    const double value = model.data(index, cwSurveyTreeModel::LengthRole).toDouble();
    return cwUnits::convert(value, trip->calibrations()->distanceUnit(), cwUnits::Meters);
}

//! \a node's solved length in meters.
double nodeMeters(const cwSurveyNode* node)
{
    return node->length()->convertTo(cwUnits::Meters).value;
}

//! True when \a spy saw a dataChanged covering \a rowIndex's Length cell for LengthRole.
bool sawLengthChange(const QSignalSpy& spy, const QModelIndex& rowIndex)
{
    for (const QList<QVariant>& arguments : spy) {
        const QModelIndex topLeft = arguments.at(0).value<QModelIndex>();
        const QModelIndex bottomRight = arguments.at(1).value<QModelIndex>();
        const QList<int> roles = arguments.at(2).value<QList<int>>();
        if (topLeft.parent() == rowIndex.parent()
            && topLeft.row() <= rowIndex.row()
            && bottomRight.row() >= rowIndex.row()
            && topLeft.column() <= cwSurveyTreeModel::Length
            && bottomRight.column() >= cwSurveyTreeModel::Length
            && (roles.isEmpty() || roles.contains(cwSurveyTreeModel::LengthRole))) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("Attached trip rows read the length of their solved legs",
          "[SurveyTreeModel][Attach][TripLength]")
{
    auto fixture = makeSavedProject(QStringLiteral("trip-lengths"),
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

    SECTION("Each attached trip row reads the legs it draws")
    {
        CHECK(tripRowMeters(model, systemTrip) == Catch::Approx(kSystemTripMeters).margin(kSolvedLengthMarginMeters));
        CHECK(tripRowMeters(model, upper) == Catch::Approx(kUpperMeters).margin(kSolvedLengthMarginMeters));
        CHECK(tripRowMeters(model, left) == Catch::Approx(kLeftMeters).margin(kSolvedLengthMarginMeters));
        CHECK(tripRowMeters(model, right) == Catch::Approx(kRightMeters).margin(kSolvedLengthMarginMeters));
    }

    SECTION("A node's trips add up to the node")
    {
        // The nodes already fold every attached leg; the trips must agree.
        REQUIRE(nodeMeters(lower) == Catch::Approx(kLowerMeters).margin(kSolvedLengthMarginMeters));
        REQUIRE(nodeMeters(root) == Catch::Approx(kFileMeters).margin(kSolvedLengthMarginMeters));
        REQUIRE(nodeMeters(folder) == Catch::Approx(kFileMeters).margin(kSolvedLengthMarginMeters));
        CHECK(tripRowMeters(model, left) + tripRowMeters(model, right)
              == Catch::Approx(nodeMeters(lower)).margin(kSolvedLengthMarginMeters));
        CHECK(tripRowMeters(model, systemTrip) + tripRowMeters(model, upper) + nodeMeters(lower)
              == Catch::Approx(nodeMeters(root)).margin(kSolvedLengthMarginMeters));
    }

    SECTION("The solve announces an attached trip row's Length cell")
    {
        CHECK(waitUntil([&]() { return sawLengthChange(dataChanged, model.indexOf(left)); }));
        CHECK(waitUntil([&]() { return sawLengthChange(dataChanged, model.indexOf(upper)); }));
    }
}
