//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwTrip.h"
#include "cwCave.h"
#include "cwKeywordModel.h"
#include "cwKeyword.h"
#include "cwSurveyChunk.h"

//Test includes
#include "SurveyEditorTestHelper.h"

//Qt inculdes
#include "cwSignalSpy.h"
#include <QSignalSpy>

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


TEST_CASE("cwTrip::chunk returns nullptr for an index outside the chunk list", "[cwTrip]")
{
    cwTrip trip;
    trip.addChunk(chunkWithNames({QStringLiteral("A1"), QStringLiteral("A2")}));
    trip.addChunk(chunkWithNames({QStringLiteral("B1"), QStringLiteral("B2")}));
    REQUIRE(trip.chunkCount() == 2);

    CHECK(trip.chunk(0) == trip.chunks().at(0));
    CHECK(trip.chunk(trip.chunkCount() - 1) == trip.chunks().last());
    CHECK(trip.chunk(trip.chunkCount()) == nullptr);
    CHECK(trip.chunk(-1) == nullptr);
}

TEST_CASE("cwTrip::removeChunks clamps an end past the last chunk to the last index", "[cwTrip]")
{
    cwTrip trip;
    cwSurveyChunk* a = chunkWithNames({QStringLiteral("A1"), QStringLiteral("A2")});
    trip.addChunk(a);
    trip.addChunk(chunkWithNames({QStringLiteral("B1"), QStringLiteral("B2")}));
    trip.addChunk(chunkWithNames({QStringLiteral("C1"), QStringLiteral("C2")}));
    REQUIRE(trip.chunkCount() == 3);

    QSignalSpy aboutToRemoveSpy(&trip, &cwTrip::chunksAboutToBeRemoved);
    QSignalSpy removedSpy(&trip, &cwTrip::chunksRemoved);

    SECTION("A range that starts inside the list removes through the last chunk") {
        trip.removeChunks(1, 5);

        CHECK(trip.chunks() == QList<cwSurveyChunk*>{a});
        REQUIRE(aboutToRemoveSpy.count() == 1);
        CHECK(aboutToRemoveSpy.at(0).at(0).toInt() == 1);
        CHECK(aboutToRemoveSpy.at(0).at(1).toInt() == 2);
        REQUIRE(removedSpy.count() == 1);
        CHECK(removedSpy.at(0).at(0).toInt() == 1);
        CHECK(removedSpy.at(0).at(1).toInt() == 2);
    }

    SECTION("A range that starts past the last chunk removes nothing") {
        trip.removeChunks(3, 5);

        CHECK(trip.chunkCount() == 3);
        CHECK(aboutToRemoveSpy.count() == 0);
        CHECK(removedSpy.count() == 0);
    }
}
