/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
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

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Qt includes
#include <QSignalSpy>

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
        errors->setTypedWarning(cwErrorTypeId::AttachedFixWithoutCS,
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

    SECTION("a suppressed warning leaves the list, and returns when unsuppressed")
    {
        errors->setTypedWarning(cwErrorTypeId::AttachedFixWithoutCS, QStringLiteral("Plain"));
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
        errors->setTypedWarning(cwErrorTypeId::AttachedFixWithoutCS, QStringLiteral("Plain"));
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
        cave.errorModel()->errors()->setTypedWarning(cwErrorTypeId::AttachedFixWithoutCS,
                                                     QStringLiteral("Plain"));
        REQUIRE(model.count() == 1);
    }
    CHECK(model.node() == nullptr);
    CHECK(model.count() == 0);
    CHECK(nodeSpy.count() == 2);
}
