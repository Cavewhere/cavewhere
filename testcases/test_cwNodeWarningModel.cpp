/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwAttachedCenterlinesModel.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwError.h"
#include "cwErrorListModel.h"
#include "cwErrorModel.h"
#include "cwAttachedFixModel.h"
#include "cwFixStation.h"
#include "cwFixStationModel.h"
#include "cwNodeWarningModel.h"
#include "cwTrip.h"
#include "TestHelper.h"

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>
#include <QUndoStack>

namespace {

using Target = cwNodeWarningModel::Target;

cwFixStation utm13NFix(const QString& name, double easting)
{
    cwFixStation fix;
    fix.setStationName(name);
    fix.setInputCS(QStringLiteral("EPSG:32613"));
    fix.setCoordinate(easting, 4430000.0, 1655.0);
    return fix;
}

QVariant roleAt(const cwNodeWarningModel& model, int row, cwNodeWarningModel::Roles role)
{
    return model.data(model.index(row), role);
}

Target targetAt(const cwNodeWarningModel& model, int row)
{
    return static_cast<Target>(roleAt(model, row, cwNodeWarningModel::TargetRole).toInt());
}

cwTrip* tripAt(const cwNodeWarningModel& model, int row)
{
    return roleAt(model, row, cwNodeWarningModel::TripRole).value<cwTrip*>();
}

QString messageAt(const cwNodeWarningModel& model, int row)
{
    return roleAt(model, row, cwNodeWarningModel::MessageRole).toString();
}

QString detailAt(const cwNodeWarningModel& model, int row)
{
    return roleAt(model, row, cwNodeWarningModel::DetailRole).toString();
}

cwSurveyNode* nodeAt(const cwNodeWarningModel& model, int row)
{
    return roleAt(model, row, cwNodeWarningModel::NodeRole).value<cwSurveyNode*>();
}

constexpr QLatin1StringView kRollUpBareFix("no coordinate system");

void warnBareFix(cwSurveyNode* node)
{
    node->errorModel()->errors()->setTypedWarning(
        cwErrorTypeId::AttachedFileFixWithoutCS,
        QStringLiteral("%1.svx fixes p1 without a coordinate system.").arg(node->name()));
}

// Region root > Cave "Top" > Folder "Mid" > Folder "Deep".
struct NestedRegion
{
    cwCavingRegion region;
    QUndoStack undoStack;
    cwSurveyNode* top = nullptr;
    cwSurveyNode* mid = nullptr;
    cwSurveyNode* deep = nullptr;

    NestedRegion()
    {
        region.setUndoStack(&undoStack);
        top = region.addNode(nullptr, cwSurveyNode::Kind::Cave, QStringLiteral("Top"));
        mid = region.addNode(top, cwSurveyNode::Kind::Folder, QStringLiteral("Mid"));
        deep = region.addNode(mid, cwSurveyNode::Kind::Folder, QStringLiteral("Deep"));
    }
};

} // namespace

TEST_CASE("A node warning lists the node's warnings and where each is fixed",
          "[cwNodeWarningModel]")
{
    cwCavingRegion region;
    region.addCave();
    cwCave* cave = region.cave(0);
    cave->addTrip();
    cwTrip* trip = cave->trip(0);

    cwNodeWarningModel model;
    QSignalSpy countSpy(&model, &cwNodeWarningModel::countChanged);
    model.setNode(cave);
    CHECK(model.count() == 0);

    cwErrorListModel* errors = cave->errorModel()->errors();

    SECTION("a warning that names the node opens its source line")
    {
        errors->setTypedWarning(cwErrorTypeId::UnconnectedStations,
                                QStringLiteral("3 stations in a.dat are not tied to the cave"),
                                QStringLiteral("u1, u2, u3"), cave->id());
        REQUIRE(model.count() == 1);
        CHECK(countSpy.count() == 1);
        CHECK(messageAt(model, 0) == QStringLiteral("3 stations in a.dat are not tied to the cave"));
        CHECK(roleAt(model, 0, cwNodeWarningModel::DetailRole).toString() == QStringLiteral("u1, u2, u3"));
        CHECK(targetAt(model, 0) == Target::SourceLine);
        CHECK(tripAt(model, 0) == nullptr);
    }

    SECTION("a warning that names one of its trips opens the trip")
    {
        errors->setTypedWarning(cwErrorTypeId::UnconnectedStations,
                                QStringLiteral("2 stations in trip.svx are not tied to the cave"),
                                QString(), trip->id());
        REQUIRE(model.count() == 1);
        CHECK(targetAt(model, 0) == Target::TripPage);
        CHECK(tripAt(model, 0) == trip);
    }

    SECTION("a warning that names nothing is listed with no target")
    {
        errors->setTypedWarning(cwErrorTypeId::UnconnectedStations, QStringLiteral("Plain"));
        REQUIRE(model.count() == 1);
        CHECK(targetAt(model, 0) == Target::NoTarget);
    }

    SECTION("an attached file's bare fix opens the Fix Stations page on that fix's row")
    {
        cave->fixStations()->appendFixStation(utm13NFix(QStringLiteral("G"), 478000.0));
        cave->setAttachedFixes(
            {cwAttachedFix{QStringLiteral("a.p1"), QStringLiteral("1 2 3"),
                           QStringLiteral("EPSG:32613"), QStringLiteral("a.svx")},
             cwAttachedFix{QStringLiteral("a.p2"), QStringLiteral("0 0 0"), QString(),
                           QStringLiteral("a.svx")}});
        errors->setTypedWarning(cwErrorTypeId::AttachedFileFixWithoutCS,
                                QStringLiteral("a.svx fixes a.p2 without a coordinate system."));
        REQUIRE(model.count() == 1);
        CHECK(targetAt(model, 0) == Target::FixStationRow);
        CHECK(tripAt(model, 0) == nullptr);

        // The node's own fix comes first, then the attached fix that has a system.
        const int bareRow = cave->fixStationTable()
                                ->mapFromSource(cave->attachedFixes()->index(1))
                                .row();
        CHECK(bareRow == 2);
        CHECK(roleAt(model, 0, cwNodeWarningModel::FixStationRowRole).toInt() == bareRow);

        // A node fix inserted ahead of the attached rows moves the target with them.
        cave->fixStations()->appendFixStation(utm13NFix(QStringLiteral("H"), 478010.0));
        CHECK(roleAt(model, 0, cwNodeWarningModel::FixStationRowRole).toInt() == 3);

        // With no bare fix left the entry opens the page with no row.
        cave->setAttachedFixes({});
        CHECK(roleAt(model, 0, cwNodeWarningModel::FixStationRowRole).toInt() == -1);
    }

    SECTION("an attached file with no fixed station opens the Fix Stations page with no row")
    {
        cave->fixStations()->appendFixStation(utm13NFix(QStringLiteral("G"), 478000.0));
        errors->setTypedWarning(cwErrorTypeId::AttachedFileUnfixed,
                                QStringLiteral("a.svx has no fixed station, so it sits at the "
                                               "origin — fix one of its stations."));
        REQUIRE(model.count() == 1);
        CHECK(targetAt(model, 0) == Target::FixStationRow);
        CHECK(roleAt(model, 0, cwNodeWarningModel::FixStationRowRole).toInt() == -1);
        CHECK(tripAt(model, 0) == nullptr);
    }

    SECTION("a suppressed warning leaves the list, and returns when unsuppressed")
    {
        errors->setTypedWarning(cwErrorTypeId::AttachedFileFixWithoutCS, QStringLiteral("Plain"));
        REQUIRE(model.count() == 1);

        const auto suppressed = static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole);
        errors->setData(errors->index(0), true, suppressed);
        CHECK(model.count() == 0);

        errors->setData(errors->index(0), false, suppressed);
        CHECK(model.count() == 1);
    }

    SECTION("a trip's untied stations are listed and open the trip, its other warnings are not")
    {
        cwErrorListModel* tripErrors = trip->errorModel()->errors();
        tripErrors->append(cwError(QStringLiteral("Missing compass"), cwError::Warning));
        CHECK(model.count() == 0);

        tripErrors->setTypedWarning(cwErrorTypeId::UnconnectedStations,
                                    QStringLiteral("2 stations in Trip are not tied to the cave"),
                                    QStringLiteral("b1, b2"), trip->id());
        REQUIRE(model.count() == 1);
        CHECK(messageAt(model, 0) == QStringLiteral("2 stations in Trip are not tied to the cave"));
        CHECK(targetAt(model, 0) == Target::TripPage);
        CHECK(tripAt(model, 0) == trip);

        // The detail changes with no change to the message.
        tripErrors->setTypedWarning(cwErrorTypeId::UnconnectedStations,
                                    QStringLiteral("2 stations in Trip are not tied to the cave"),
                                    QStringLiteral("b1, b3"), trip->id());
        CHECK(roleAt(model, 0, cwNodeWarningModel::DetailRole).toString() == QStringLiteral("b1, b3"));

        // A removed trip takes its warning with it.
        cave->removeTrip(0);
        CHECK(model.count() == 0);
    }

    SECTION("a trip added later is watched")
    {
        cave->addTrip();
        cwTrip* laterTrip = cave->trip(1);
        laterTrip->errorModel()->errors()->setTypedWarning(
            cwErrorTypeId::UnconnectedStations,
            QStringLiteral("1 station in Later is not tied to the cave"),
            QStringLiteral("c1"), laterTrip->id());
        REQUIRE(model.count() == 1);
        CHECK(tripAt(model, 0) == laterTrip);
    }

    SECTION("clearing the node empties the list")
    {
        errors->setTypedWarning(cwErrorTypeId::AttachedFileFixWithoutCS, QStringLiteral("Plain"));
        REQUIRE(model.count() == 1);
        model.setNode(nullptr);
        CHECK(model.count() == 0);
    }
}

TEST_CASE("A node warning on a fix opens the fix's row, wherever the row moves",
          "[cwNodeWarningModel][cwFixStationValidator]")
{
    cwCavingRegion region;
    region.addCave();
    cwCave* cave = region.cave(0);
    cave->fixStations()->appendFixStation(utm13NFix(QStringLiteral("G"), 478000.0));
    cave->fixStations()->appendFixStation(utm13NFix(QStringLiteral("H"), 478010.0));
    // A transposed leading digit puts this fix outside UTM 13N's valid domain.
    cave->fixStations()->appendFixStation(utm13NFix(QStringLiteral("BAD"), 1478000.0));

    cwNodeWarningModel model;
    model.setNode(cave);
    REQUIRE(model.count() == 1);
    CHECK(targetAt(model, 0) == Target::FixStationRow);
    CHECK(roleAt(model, 0, cwNodeWarningModel::FixStationRowRole).toInt() == 2);

    cave->fixStations()->removeAt(0);
    REQUIRE(model.count() == 1);
    CHECK(roleAt(model, 0, cwNodeWarningModel::FixStationRowRole).toInt() == 1);
}

TEST_CASE("A node warning model empties when its node is destroyed",
          "[cwNodeWarningModel]")
{
    cwNodeWarningModel model;
    QSignalSpy nodeSpy(&model, &cwNodeWarningModel::nodeChanged);
    {
        cwCave cave;
        model.setNode(&cave);
        cave.errorModel()->errors()->setTypedWarning(cwErrorTypeId::AttachedFileFixWithoutCS,
                                                     QStringLiteral("Plain"));
        REQUIRE(model.count() == 1);
    }
    CHECK(model.node() == nullptr);
    CHECK(model.count() == 0);
    CHECK(nodeSpy.count() == 2);
}

TEST_CASE("A descendant's warning rolls up to every ancestor's list and opens the node that owns it",
          "[cwNodeWarningModel]")
{
    NestedRegion nested;
    REQUIRE(nested.deep != nullptr);
    warnBareFix(nested.deep);

    cwNodeWarningModel rootModel;
    rootModel.setNode(nested.region.rootNode());
    cwNodeWarningModel topModel;
    topModel.setNode(nested.top);
    cwNodeWarningModel deepModel;
    deepModel.setNode(nested.deep);

    // The owner lists its own warning in full and nothing rolled up.
    REQUIRE(deepModel.count() == 1);
    CHECK(targetAt(deepModel, 0) == Target::FixStationRow);
    CHECK_FALSE(messageAt(deepModel, 0).contains(kRollUpBareFix));

    // Each ancestor row's badge counts the warning below it.
    CHECK(nested.mid->errorModel()->warningCount() == 1);
    CHECK(nested.top->errorModel()->warningCount() == 1);
    CHECK(nested.region.rootNode()->errorModel()->warningCount() == 1);

    for (const cwNodeWarningModel* model : {&rootModel, &topModel}) {
        INFO("model on " << model->node()->name().toStdString());
        CHECK(waitUntil([model]() { return model->count() == 1; }));
        REQUIRE(model->count() == 1);
        CHECK(targetAt(*model, 0) == Target::NodePage);
        CHECK(nodeAt(*model, 0) == nested.deep);
        INFO("message: " << messageAt(*model, 0).toStdString());
        CHECK(messageAt(*model, 0).contains(kRollUpBareFix));
        CHECK(detailAt(*model, 0).contains(QStringLiteral("Deep")));
    }
}

TEST_CASE("A roll-up counts every node below with the warning and opens the first",
          "[cwNodeWarningModel]")
{
    NestedRegion nested;
    cwSurveyNode* later = nested.region.addNode(nested.mid, cwSurveyNode::Kind::Folder,
                                                QStringLiteral("Later"));
    REQUIRE(later != nullptr);
    warnBareFix(later);
    warnBareFix(nested.deep);

    cwNodeWarningModel rootModel;
    rootModel.setNode(nested.region.rootNode());
    CHECK(nested.region.rootNode()->errorModel()->warningCount() == 2);

    CHECK(waitUntil([&rootModel]() { return rootModel.count() == 1; }));
    REQUIRE(rootModel.count() == 1);
    const QString message = messageAt(rootModel, 0);
    INFO("message: " << message.toStdString());
    CHECK(message.contains(kRollUpBareFix));
    CHECK(message.contains(QStringLiteral("2 ")));
    CHECK(detailAt(rootModel, 0).contains(QStringLiteral("Deep")));
    CHECK(detailAt(rootModel, 0).contains(QStringLiteral("Later")));
    // Deep comes first in the tree, though Later warned first.
    CHECK(nodeAt(rootModel, 0) == nested.deep);
}

TEST_CASE("A roll-up leaves when the descendant's warning is suppressed or its node removed",
          "[cwNodeWarningModel]")
{
    NestedRegion nested;
    warnBareFix(nested.deep);

    cwNodeWarningModel rootModel;
    rootModel.setNode(nested.region.rootNode());
    REQUIRE(waitUntil([&rootModel]() { return rootModel.count() == 1; }));

    SECTION("suppressing the warning drops the roll-up, unsuppressing brings it back")
    {
        cwErrorListModel* errors = nested.deep->errorModel()->errors();
        const auto suppressed = static_cast<int>(cwErrorListModel::ErrorRoles::SuppressedRole);
        errors->setData(errors->index(0), true, suppressed);
        CHECK(waitUntil([&rootModel]() { return rootModel.count() == 0; }));

        errors->setData(errors->index(0), false, suppressed);
        CHECK(waitUntil([&rootModel]() { return rootModel.count() == 1; }));
        CHECK(nodeAt(rootModel, 0) == nested.deep);
    }

    SECTION("removing the branch drops the roll-up and the badge, undo restores both")
    {
        cwErrorModel* topErrors = nested.top->errorModel();
        REQUIRE(topErrors->warningCount() == 1);

        nested.top->removeNode(nested.top->indexOfNode(nested.mid));
        CHECK(waitUntil([&rootModel]() { return rootModel.count() == 0; }));
        CHECK(topErrors->warningCount() == 0);

        nested.undoStack.undo();
        CHECK(waitUntil([&rootModel]() { return rootModel.count() == 1; }));
        CHECK(nodeAt(rootModel, 0) == nested.deep);
        CHECK(topErrors->warningCount() == 1);
    }
}

TEST_CASE("A descendant's file error rolls up and opens the node that holds the file",
          "[cwNodeWarningModel]")
{
    NestedRegion nested;
    cwAttachedCenterlinesModel attachedCenterlines;

    cwNodeWarningModel rootModel;
    rootModel.setAttachedCenterlines(&attachedCenterlines);
    rootModel.setNode(nested.region.rootNode());
    REQUIRE(rootModel.count() == 0);

    cwAttachedCenterlinesModel::Row row;
    row.ownerName = QStringLiteral("Deep");

    SECTION("a node's own file")
    {
        row.ownerId = nested.deep->id();
    }

    SECTION("a file attached to one of the node's trips")
    {
        nested.deep->addTrip();
        row.ownerId = nested.deep->trip(0)->id();
    }

    row.error = QStringLiteral("deep.svx:3: error: Unknown command \"*bogus\"");
    attachedCenterlines.setRows({row});

    CHECK(waitUntil([&rootModel]() { return rootModel.count() == 1; }));
    REQUIRE(rootModel.count() == 1);
    CHECK(targetAt(rootModel, 0) == Target::NodePage);
    CHECK(nodeAt(rootModel, 0) == nested.deep);
    CHECK(detailAt(rootModel, 0).contains(QStringLiteral("Deep")));

    attachedCenterlines.setRows({});
    CHECK(waitUntil([&rootModel]() { return rootModel.count() == 0; }));
}
