//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTrip.h"
#include "cwCave.h"
#include "cwKeywordModel.h"
#include "cwKeyword.h"

//Qt inculdes
#include "cwSignalSpy.h"

TEST_CASE("cwTrip should strip the datestamp away from date", "[cwTrip]") {
    cwTrip trip;

    cwSignalSpy spy(&trip, &cwTrip::dateChanged);

    CHECK(trip.date().time().hour() == 0);
    CHECK(trip.date().time().minute() == 0);
    CHECK(trip.date().time().second() == 0);
    CHECK(trip.date().time().msec() == 0);
    CHECK(trip.date().date() == QDate::currentDate());

    trip.setDate(QDateTime::currentDateTime().addDays(-1));
    CHECK(trip.date().time().hour() == 0);
    CHECK(trip.date().time().minute() == 0);
    CHECK(trip.date().time().second() == 0);
    CHECK(trip.date().time().msec() == 0);
    CHECK(trip.date().date() == QDate::currentDate().addDays(-1));

    CHECK(spy.count() == 1);
}

TEST_CASE("cwTrip::linePlotKeywordModel carries Type=Line Plot and extends the trip model",
          "[cwTrip][keyword]")
{
    cwTrip trip;
    trip.setName(QStringLiteral("Trip A"));

    cwKeywordModel* linePlotModel = trip.linePlotKeywordModel();
    REQUIRE(linePlotModel != nullptr);

    SECTION("lazily created and stable across calls") {
        CHECK(trip.linePlotKeywordModel() == linePlotModel);
    }

    SECTION("aggregates Type=Line Plot and the trip's keywords via the extension") {
        // keywords() walks extensions, so the line plot model reports both its
        // own Type=Line Plot and the trip's keywords (Trip name, ...).
        const auto all = linePlotModel->keywords();
        CHECK(all.contains(cwKeyword(cwKeywordModel::TypeKey, QStringLiteral("Line Plot"))));
        CHECK(all.contains(cwKeyword(cwKeywordModel::TripNameKey, QStringLiteral("Trip A"))));
    }

    SECTION("the Type identity stays off the trip's own model") {
        // Scraps/notes/leads extend trip->keywordModel(); putting Type=Line Plot
        // there would make them wrongly inherit "Line Plot".
        CHECK_FALSE(trip.keywordModel()->keywords().contains(
            cwKeyword(cwKeywordModel::TypeKey, QStringLiteral("Line Plot"))));
    }
}

