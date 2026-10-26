/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

//Test includes
#include "SignalSpyChecker.h"
#include "SurveyEditorTestHelper.h"

//Qt includes
#include <QPointer>
#include <QStringList>

//Std includes
#include <memory>

namespace {

QList<QUuid> shotIds(const QList<cwShot>& shots)
{
    QList<QUuid> ids;
    for(const auto& shot : shots) {
        ids.append(shot.id());
    }
    return ids;
}

QStringList splayDistances(const cwStation& station)
{
    QStringList distances;
    for(const auto& splay : station.splays()) {
        distances.append(splay.distance.value());
    }
    return distances;
}

/**
 * A trip of three chunks: A1 to A3, then a3 to A5, then C1 to C2. The tie
 * station is A3 in the upper chunk and a3 in the lower one. The upper copy has
 * Left, Up and one splay; the lower copy has Left, Right, Down and two splays.
 */
struct MergeFixture {
    MergeFixture(const QString& upperTie = QStringLiteral("A3"),
                 const QString& lowerTie = QStringLiteral("a3"))
        : trip(std::make_unique<cwTrip>()),
          upper(chunkWithNames({"A1", "A2", upperTie})),
          lower(chunkWithNames({lowerTie, "A4", "A5"})),
          after(chunkWithNames({"C1", "C2"}))
    {
        auto upperData = upper->data();
        cwStation upperTieStation = upperData.stations.last();
        upperTieStation.setLeft(cwDistanceReading(QStringLiteral("1")));
        upperTieStation.setUp(cwDistanceReading(QStringLiteral("3")));
        upperTieStation.setSplays({splayWithDistance(QStringLiteral("5"))});
        upperData.stations.replace(upperData.stations.size() - 1, upperTieStation);
        upper->setData(upperData);

        auto lowerData = lower->data();
        cwStation lowerTieStation = lowerData.stations.first();
        lowerTieStation.setLeft(cwDistanceReading(QStringLiteral("9")));
        lowerTieStation.setRight(cwDistanceReading(QStringLiteral("2")));
        lowerTieStation.setDown(cwDistanceReading(QStringLiteral("4")));
        lowerTieStation.setSplays({splayWithDistance(QStringLiteral("6")), splayWithDistance(QStringLiteral("7"))});
        lowerData.stations.replace(0, lowerTieStation);
        lower->setData(lowerData);

        trip->addChunk(upper);
        trip->addChunk(lower);
        trip->addChunk(after);

        upperId = upper->id();
        upperStations = upper->stations();
        upperShots = upper->shots();
        lowerStations = lower->stations();
        lowerShots = lower->shots();
    }

    std::unique_ptr<cwTrip> trip;
    cwSurveyChunk* upper;
    cwSurveyChunk* lower;
    cwSurveyChunk* after;

    QUuid upperId;
    QList<cwStation> upperStations;
    QList<cwShot> upperShots;
    QList<cwStation> lowerStations;
    QList<cwShot> lowerShots;
};

}

TEST_CASE("Merging joins the lower chunk onto the upper one at a tie that differs only in case",
          "[cwTrip][MergeChunks]")
{
    MergeFixture fixture;
    const QPointer<cwSurveyChunk> lower = fixture.lower;

    CHECK(fixture.trip->canMergeChunks(0));

    fixture.trip->mergeChunks(0);

    REQUIRE(fixture.trip->chunkCount() == 2);
    CHECK(fixture.trip->chunk(0) == fixture.upper);
    CHECK(fixture.trip->chunk(1) == fixture.after);
    CHECK_FALSE(fixture.trip->chunks().contains(lower.data()));
    CHECK(fixture.upper->id() == fixture.upperId);

    CHECK(stationNames(fixture.upper) == QStringList({"A1", "A2", "A3", "A4", "A5"}));
    CHECK(fixture.upper->stationCount() == 5);
    CHECK(fixture.upper->shotCount() == 4);
    CHECK(shotIds(fixture.upper->shots()) == shotIds(fixture.upperShots + fixture.lowerShots));
    CHECK(fixture.upper->isValid());

    SECTION("Moved stations keep their ids") {
        CHECK(fixture.upper->station(3).id() == fixture.lowerStations.at(1).id());
        CHECK(fixture.upper->station(4).id() == fixture.lowerStations.at(2).id());
    }

    SECTION("The tie keeps the upper record and fills its empty LRUDs from the lower copy") {
        const cwStation tie = fixture.upper->station(2);
        CHECK(tie.id() == fixture.upperStations.at(2).id());
        CHECK(tie.name() == QStringLiteral("A3"));
        CHECK(tie.left().value() == QStringLiteral("1"));
        CHECK(tie.up().value() == QStringLiteral("3"));
        CHECK(tie.right().value() == QStringLiteral("2"));
        CHECK(tie.down().value() == QStringLiteral("4"));
    }

    SECTION("The lower copy's splays follow the upper copy's") {
        CHECK(splayDistances(fixture.upper->station(2)) == QStringList({"5", "6", "7"}));
    }
}

TEST_CASE("A tie whose names differ or are empty refuses the merge",
          "[cwTrip][MergeChunks]")
{
    //One merge that works, so the refusals below are the rule and not the stub
    MergeFixture working;
    working.trip->mergeChunks(0);
    REQUIRE(working.trip->chunkCount() == 2);

    struct Tie {
        QString upper;
        QString lower;
    };
    const QList<Tie> ties {
        {QStringLiteral("A3"), QStringLiteral("A9")},
        {QString(), QStringLiteral("A3")},
        {QStringLiteral("A3"), QString()},
        {QString(), QString()}
    };

    for(const auto& tie : ties) {
        INFO("upper tie '" << tie.upper.toStdString() << "', lower tie '" << tie.lower.toStdString() << "'");
        MergeFixture fixture(tie.upper, tie.lower);

        CHECK_FALSE(fixture.trip->canMergeChunks(0));

        SignalSpyChecker::SignalSpy addedSpy(fixture.upper, &cwSurveyChunk::added);
        SignalSpyChecker::SignalSpy dataChangedSpy(fixture.upper, &cwSurveyChunk::dataChanged);
        SignalSpyChecker::SignalSpy splaysChangedSpy(fixture.upper, &cwSurveyChunk::stationSplaysChanged);
        SignalSpyChecker::SignalSpy aboutToRemoveSpy(fixture.trip.get(), &cwTrip::chunksAboutToBeRemoved);
        SignalSpyChecker::SignalSpy countSpy(fixture.trip.get(), &cwTrip::numberOfChunksChanged);
        SignalSpyChecker::Constant checker {
            {&addedSpy, 0},
            {&dataChangedSpy, 0},
            {&splaysChangedSpy, 0},
            {&aboutToRemoveSpy, 0},
            {&countSpy, 0}
        };

        fixture.trip->mergeChunks(0);

        checker.checkSpies();
        CHECK(fixture.trip->chunkCount() == 3);
        CHECK(fixture.upper->stationCount() == 3);
        CHECK(fixture.lower->stationCount() == 3);
        CHECK(fixture.upper->station(2).splayCount() == 1);
    }
}

TEST_CASE("canMergeChunks answers only for an upper index that has a chunk below it",
          "[cwTrip][MergeChunks]")
{
    MergeFixture fixture;

    //C1 becomes A5, so the second pair ties too
    auto afterData = fixture.after->data();
    afterData.stations.replace(0, cwStation(QStringLiteral("A5")));
    fixture.after->setData(afterData);

    CHECK(fixture.trip->canMergeChunks(0));
    CHECK(fixture.trip->canMergeChunks(1));

    //The last chunk has nothing below it
    CHECK_FALSE(fixture.trip->canMergeChunks(2));
    CHECK_FALSE(fixture.trip->canMergeChunks(-1));
    CHECK_FALSE(fixture.trip->canMergeChunks(fixture.trip->chunkCount()));

    SignalSpyChecker::SignalSpy countSpy(fixture.trip.get(), &cwTrip::numberOfChunksChanged);
    SignalSpyChecker::Constant checker {
        {&countSpy, 0}
    };
    fixture.trip->mergeChunks(2);
    fixture.trip->mergeChunks(-1);
    fixture.trip->mergeChunks(fixture.trip->chunkCount());
    checker.checkSpies();
    CHECK(fixture.trip->chunkCount() == 3);

    SECTION("Merging the second pair keeps the first chunk") {
        fixture.trip->mergeChunks(1);
        REQUIRE(fixture.trip->chunkCount() == 2);
        CHECK(fixture.trip->chunk(0) == fixture.upper);
        CHECK(fixture.trip->chunk(1) == fixture.lower);
        CHECK(stationNames(fixture.lower) == QStringList({"a3", "A4", "A5", "C2"}));
    }
}

TEST_CASE("mergeChunks appends to the upper chunk before it removes the lower one",
          "[cwTrip][MergeChunks]")
{
    MergeFixture fixture;

    SignalSpyChecker::SignalSpy addedSpy(fixture.upper, &cwSurveyChunk::added);
    SignalSpyChecker::SignalSpy aboutToRemoveSpy(fixture.upper, &cwSurveyChunk::aboutToRemove);
    SignalSpyChecker::SignalSpy tripAboutToRemoveSpy(fixture.trip.get(), &cwTrip::chunksAboutToBeRemoved);
    SignalSpyChecker::SignalSpy tripRemovedSpy(fixture.trip.get(), &cwTrip::chunksRemoved);
    SignalSpyChecker::SignalSpy insertedSpy(fixture.trip.get(), &cwTrip::chunksInserted);
    SignalSpyChecker::SignalSpy countSpy(fixture.trip.get(), &cwTrip::numberOfChunksChanged);
    SignalSpyChecker::Constant checker {
        {&addedSpy, 0},
        {&aboutToRemoveSpy, 0},
        {&tripAboutToRemoveSpy, 0},
        {&tripRemovedSpy, 0},
        {&insertedSpy, 0},
        {&countSpy, 0}
    };

    QStringList order;
    QObject::connect(fixture.upper, &cwSurveyChunk::added, fixture.upper,
                     [&order]() { order.append(QStringLiteral("added")); });
    QObject::connect(fixture.trip.get(), &cwTrip::chunksAboutToBeRemoved, fixture.trip.get(),
                     [&order]() { order.append(QStringLiteral("chunksAboutToBeRemoved")); });
    QObject::connect(fixture.trip.get(), &cwTrip::chunksRemoved, fixture.trip.get(),
                     [&order]() { order.append(QStringLiteral("chunksRemoved")); });
    QObject::connect(fixture.trip.get(), &cwTrip::numberOfChunksChanged, fixture.trip.get(),
                     [&order]() { order.append(QStringLiteral("numberOfChunksChanged")); });

    fixture.trip->mergeChunks(0);

    checker[&addedSpy] = 1;
    checker[&tripAboutToRemoveSpy] = 1;
    checker[&tripRemovedSpy] = 1;
    checker[&countSpy] = 1;
    checker.checkSpies();

    //A4 and A5 land at stations 3..4, the two lower shots at 2..3
    const QList<QVariant> expectedAdded {3, 4, 2, 3};
    REQUIRE(addedSpy.size() == 1);
    CHECK(addedSpy.at(0) == expectedAdded);

    const QList<QVariant> expectedRemoved {1, 1};
    REQUIRE(tripAboutToRemoveSpy.size() == 1);
    CHECK(tripAboutToRemoveSpy.at(0) == expectedRemoved);
    CHECK(tripRemovedSpy.at(0) == expectedRemoved);

    CHECK(order == QStringList({"added", "chunksAboutToBeRemoved", "chunksRemoved", "numberOfChunksChanged"}));
}

TEST_CASE("appendTail appends shots and their to-stations behind one added",
          "[cwSurveyChunk][MergeChunks]")
{
    MergeFixture fixture;
    cwSurveyChunk* chunk = fixture.upper;
    const QList<cwStation> tailStations = fixture.lowerStations.mid(1);
    const QList<cwShot> tailShots = fixture.lowerShots;

    SignalSpyChecker::SignalSpy addedSpy(chunk, &cwSurveyChunk::added);
    SignalSpyChecker::Constant checker {
        {&addedSpy, 0}
    };

    SECTION("Two stations and two shots land at the end") {
        chunk->appendTail(tailStations, tailShots);

        checker[&addedSpy] = 1;
        checker.checkSpies();

        const QList<QVariant> expectedRange {3, 4, 2, 3};
        REQUIRE(addedSpy.size() == 1);
        CHECK(addedSpy.at(0) == expectedRange);

        CHECK(stationNames(chunk) == QStringList({"A1", "A2", "A3", "A4", "A5"}));
        CHECK(shotIds(chunk->shots()) == shotIds(fixture.upperShots + tailShots));
        CHECK(chunk->isValid());
    }

    SECTION("Lists of different sizes, empty lists, or an empty chunk change nothing") {
        //The same chunk does take a matching tail
        cwSurveyChunk accepting;
        accepting.setData(chunk->data());
        accepting.appendTail(tailStations, tailShots);
        REQUIRE(accepting.stationCount() == 5);

        chunk->appendTail(tailStations, tailShots.mid(1));
        chunk->appendTail(tailStations.mid(1), tailShots);
        chunk->appendTail({}, {});

        cwSurveyChunk empty;
        SignalSpyChecker::SignalSpy emptyAddedSpy(&empty, &cwSurveyChunk::added);
        empty.appendTail(tailStations, tailShots);
        CHECK(emptyAddedSpy.size() == 0);
        CHECK(empty.stationCount() == 0);

        checker.checkSpies();
        CHECK(chunk->stationCount() == 3);
        CHECK(chunk->shotCount() == 2);
    }
}
