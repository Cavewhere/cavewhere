/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwNodePageModel.h"
#include "cwTrip.h"

//Qt includes
#include <QSignalSpy>

namespace {
    cwTrip* makeTrip(cwSurveyNode* node, const QString& name, const QDateTime& date)
    {
        auto* trip = new cwTrip();
        trip->setName(name);
        trip->setDate(date);
        node->addTrip(trip);
        return trip;
    }

    cwCave* makeChild(cwSurveyNode* parent, const QString& name)
    {
        auto* child = new cwCave();
        child->setName(name);
        parent->addNode(child);
        return child;
    }

    QDateTime day(int year, int month, int dayOfMonth)
    {
        return QDateTime(QDate(year, month, dayOfMonth), QTime(0, 0));
    }
}

TEST_CASE("cwNodePageModel counts the trips at every depth below its node", "[NodePageModel]")
{
    cwCave cave;
    cave.setName(QStringLiteral("Cave"));
    makeTrip(&cave, QStringLiteral("Entrance"), day(2023, 11, 12));

    cwNodePageModel model;
    QSignalSpy countSpy(&model, &cwNodePageModel::tripCountChanged);
    model.setNode(&cave);

    CHECK(model.tripCount() == 1);
    CHECK(countSpy.count() == 1);

    SECTION("a trip in a Section counts toward the cave") {
        cwCave* section = makeChild(&cave, QStringLiteral("Upper level"));
        makeTrip(section, QStringLiteral("Upper"), day(2024, 2, 1));
        CHECK(model.tripCount() == 2);

        cwCave* deeper = makeChild(section, QStringLiteral("Deeper"));
        makeTrip(deeper, QStringLiteral("Deep"), day(2024, 3, 1));
        CHECK(model.tripCount() == 3);

        SECTION("removing the Section takes its trips off the count") {
            cave.removeNode(cave.indexOfNode(section));
            CHECK(model.tripCount() == 1);
        }
    }

    SECTION("removing a direct trip lowers the count") {
        cave.removeTrip(0);
        CHECK(model.tripCount() == 0);
    }

    SECTION("a cleared node reads zero") {
        model.setNode(nullptr);
        CHECK(model.tripCount() == 0);
        CHECK_FALSE(model.lastSurvey().isValid());
    }
}

TEST_CASE("cwNodePageModel reports the latest trip date below its node", "[NodePageModel]")
{
    cwCave cave;
    cwTrip* entrance = makeTrip(&cave, QStringLiteral("Entrance"), day(2023, 11, 12));

    cwNodePageModel model;
    model.setNode(&cave);
    CHECK(model.lastSurvey().date() == QDate(2023, 11, 12));

    QSignalSpy lastSurveySpy(&model, &cwNodePageModel::lastSurveyChanged);

    SECTION("a newer trip in a Section moves the date") {
        cwCave* section = makeChild(&cave, QStringLiteral("Upper level"));
        cwTrip* upper = makeTrip(section, QStringLiteral("Upper"), day(2024, 2, 1));
        CHECK(model.lastSurvey().date() == QDate(2024, 2, 1));

        SECTION("editing that trip's date follows the edit") {
            upper->setDate(day(2022, 1, 1));
            CHECK(model.lastSurvey().date() == QDate(2023, 11, 12));
        }
    }

    SECTION("an older trip leaves the date alone") {
        makeTrip(&cave, QStringLiteral("Old"), day(2001, 5, 5));
        CHECK(model.lastSurvey().date() == QDate(2023, 11, 12));
        CHECK(lastSurveySpy.count() == 0);
    }

    SECTION("a trip without a date is passed over") {
        makeTrip(&cave, QStringLiteral("Undated"), QDateTime());
        cave.removeTrip(cave.indexOf(entrance));
        CHECK_FALSE(model.lastSurvey().isValid());
    }
}

TEST_CASE("cwNodePageModel lets go of a node that is destroyed", "[NodePageModel]")
{
    auto* cave = new cwCave();
    makeTrip(cave, QStringLiteral("Entrance"), day(2023, 11, 12));

    cwNodePageModel model;
    model.setNode(cave);
    QSignalSpy nodeSpy(&model, &cwNodePageModel::nodeChanged);

    delete cave;

    CHECK(model.node() == nullptr);
    CHECK(model.tripCount() == 0);
    CHECK_FALSE(model.lastSurvey().isValid());
    CHECK(nodeSpy.count() == 1);
}

TEST_CASE("cwNodePageModel names the node's kind as its chip does", "[NodePageModel]")
{
    cwCave cave;

    cwNodePageModel model;
    model.setNode(&cave);
    CHECK(model.kindLabel() == QStringLiteral("Cave"));

    QSignalSpy kindSpy(&model, &cwNodePageModel::kindLabelChanged);
    cave.setKind(cwSurveyNode::Kind::Folder);

    CHECK(model.kindLabel() == QStringLiteral("Folder"));
    CHECK(kindSpy.count() == 1);
}
