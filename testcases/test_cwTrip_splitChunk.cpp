/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

//Our includes
#include "cwShot.h"
#include "cwStation.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

//Test includes
#include "SignalSpyChecker.h"
#include "SurveyEditorTestHelper.h"

//Qt includes
#include <QStringList>

//Std includes
#include <memory>

namespace {

constexpr int kSplitShot = 2;

//A shot with a 0 distance and no directions, which only carries LRUDs
cwShot lrudOnlyShot()
{
    return cwShot(QStringLiteral("0"),
                  QString(),
                  QString(),
                  QString(),
                  QString());
}

QList<QUuid> shotIds(const QList<cwShot>& shots)
{
    QList<QUuid> ids;
    for(const auto& shot : shots) {
        ids.append(shot.id());
    }
    return ids;
}

/**
 * A trip of three chunks: B1 to B2, then A1 to A5, then C1 to C2. Shot 2 of the
 * middle chunk runs A3 to A4 and is the one the tests split at. A3 carries
 * LRUDs and two splays.
 */
struct SplitFixture {
    explicit SplitFixture(const cwShot& splitShot)
        : trip(std::make_unique<cwTrip>()),
          before(new cwSurveyChunk()),
          chunk(new cwSurveyChunk()),
          after(new cwSurveyChunk())
    {
        before->appendShot(cwStation(QStringLiteral("B1")), cwStation(QStringLiteral("B2")), readingShot());

        chunk->appendShot(cwStation(QStringLiteral("A1")), cwStation(QStringLiteral("A2")), readingShot());
        chunk->appendShot(cwStation(QStringLiteral("A2")), cwStation(QStringLiteral("A3")), readingShot());
        chunk->appendShot(cwStation(QStringLiteral("A3")), cwStation(QStringLiteral("A4")), readingShot());
        chunk->appendShot(cwStation(QStringLiteral("A4")), cwStation(QStringLiteral("A5")), readingShot());

        //appendShot needs readings to accept a shot, so the shot under test
        //goes in afterward
        auto data = chunk->data();
        data.shots.replace(kSplitShot, splitShot);
        cwStation a3 = data.stations.at(kSplitShot);
        a3.setLeft(cwDistanceReading(QStringLiteral("1")));
        a3.setRight(cwDistanceReading(QStringLiteral("2")));
        a3.setUp(cwDistanceReading(QStringLiteral("3")));
        a3.setDown(cwDistanceReading(QStringLiteral("4")));
        a3.setSplays({splayWithDistance(QStringLiteral("5")), splayWithDistance(QStringLiteral("6"))});
        data.stations.replace(kSplitShot, a3);
        chunk->setData(data);

        after->appendShot(cwStation(QStringLiteral("C1")), cwStation(QStringLiteral("C2")), readingShot());

        trip->addChunk(before);
        trip->addChunk(chunk);
        trip->addChunk(after);

        originalChunkId = chunk->id();
        originalStations = chunk->stations();
        originalShots = chunk->shots();
    }

    std::unique_ptr<cwTrip> trip;
    cwSurveyChunk* before;
    cwSurveyChunk* chunk;
    cwSurveyChunk* after;

    QUuid originalChunkId;
    QList<cwStation> originalStations;
    QList<cwShot> originalShots;
};

void checkNameOnly(const cwStation& station)
{
    CHECK(station.left().state() == cwDistanceReading::State::Empty);
    CHECK(station.right().state() == cwDistanceReading::State::Empty);
    CHECK(station.up().state() == cwDistanceReading::State::Empty);
    CHECK(station.down().state() == cwDistanceReading::State::Empty);
    CHECK(station.splayCount() == 0);
    CHECK(station.ignoredWarnings().isEmpty());
}

}

TEST_CASE("Splitting at a shot with readings moves the shot under a name-only copy of its from-station",
          "[cwTrip][SplitChunk]")
{
    SplitFixture fixture(readingShot());
    REQUIRE_FALSE(fixture.chunk->isShotDataEmpty(kSplitShot));

    CHECK(fixture.trip->canSplitChunk(fixture.chunk, kSplitShot));

    cwSurveyChunk* newChunk = fixture.trip->splitChunk(fixture.chunk, kSplitShot);
    REQUIRE(newChunk != nullptr);

    REQUIRE(fixture.trip->chunkCount() == 4);
    CHECK(fixture.trip->chunk(0) == fixture.before);
    CHECK(fixture.trip->chunk(1) == fixture.chunk);
    CHECK(fixture.trip->chunk(2) == newChunk);
    CHECK(fixture.trip->chunk(3) == fixture.after);
    CHECK(newChunk->parentTrip() == fixture.trip.get());
    CHECK(newChunk->parent() == fixture.trip.get());

    CHECK(stationNames(fixture.chunk) == QStringList({"A1", "A2", "A3"}));
    CHECK(shotIds(fixture.chunk->shots()) == shotIds(fixture.originalShots.mid(0, kSplitShot)));
    CHECK(stationNames(newChunk) == QStringList({"A3", "A4", "A5"}));
    CHECK(shotIds(newChunk->shots()) == shotIds(fixture.originalShots.mid(kSplitShot)));

    CHECK(fixture.chunk->isValid());
    CHECK(newChunk->isValid());

    SECTION("The upper chunk keeps its id and the new chunk gets a fresh one") {
        CHECK(fixture.chunk->id() == fixture.originalChunkId);
        CHECK_FALSE(newChunk->id().isNull());
        CHECK(newChunk->id() != fixture.originalChunkId);
        CHECK(newChunk->id() != fixture.before->id());
        CHECK(newChunk->id() != fixture.after->id());
    }

    SECTION("The split station stays whole in the upper chunk") {
        const cwStation upperA3 = fixture.chunk->station(kSplitShot);
        CHECK(upperA3.id() == fixture.originalStations.at(kSplitShot).id());
        CHECK(upperA3.left().value() == QStringLiteral("1"));
        CHECK(upperA3.down().value() == QStringLiteral("4"));
        CHECK(upperA3.splayCount() == 2);
    }

    SECTION("The copy carries the name only, with a station id of its own") {
        const cwStation copy = newChunk->station(0);
        CHECK(copy.name() == QStringLiteral("A3"));
        CHECK(copy.id() != fixture.originalStations.at(kSplitShot).id());
        CHECK_FALSE(copy.id().isNull());
        checkNameOnly(copy);
    }

    SECTION("Moved stations and shots keep their ids") {
        CHECK(newChunk->station(1).id() == fixture.originalStations.at(kSplitShot + 1).id());
        CHECK(newChunk->station(2).id() == fixture.originalStations.at(kSplitShot + 2).id());
        CHECK(newChunk->shot(0).id() == fixture.originalShots.at(kSplitShot).id());
        CHECK(newChunk->shot(0).distance().value() == QStringLiteral("10"));
    }
}

TEST_CASE("Splitting at an empty shot drops the shot and copies no station",
          "[cwTrip][SplitChunk]")
{
    SplitFixture fixture{cwShot()};
    REQUIRE(fixture.chunk->isShotDataEmpty(kSplitShot));

    CHECK(fixture.trip->canSplitChunk(fixture.chunk, kSplitShot));

    cwSurveyChunk* newChunk = fixture.trip->splitChunk(fixture.chunk, kSplitShot);
    REQUIRE(newChunk != nullptr);
    REQUIRE(fixture.trip->chunkCount() == 4);
    CHECK(fixture.trip->chunk(2) == newChunk);

    CHECK(stationNames(fixture.chunk) == QStringList({"A1", "A2", "A3"}));
    CHECK(fixture.chunk->shotCount() == kSplitShot);
    CHECK(stationNames(newChunk) == QStringList({"A4", "A5"}));
    CHECK(shotIds(newChunk->shots()) == shotIds(fixture.originalShots.mid(kSplitShot + 1)));
    CHECK(newChunk->station(0).id() == fixture.originalStations.at(kSplitShot + 1).id());

    CHECK(fixture.chunk->isValid());
    CHECK(newChunk->isValid());
    CHECK(fixture.chunk->id() == fixture.originalChunkId);
    CHECK(newChunk->id() != fixture.originalChunkId);
}

TEST_CASE("Splitting at an LRUD-only shot keeps the shot", "[cwTrip][SplitChunk]")
{
    SplitFixture fixture(lrudOnlyShot());
    REQUIRE_FALSE(fixture.chunk->isShotDataEmpty(kSplitShot));

    cwSurveyChunk* newChunk = fixture.trip->splitChunk(fixture.chunk, kSplitShot);
    REQUIRE(newChunk != nullptr);

    CHECK(stationNames(newChunk) == QStringList({"A3", "A4", "A5"}));
    REQUIRE(newChunk->shotCount() == 2);
    CHECK(newChunk->shot(0).id() == fixture.originalShots.at(kSplitShot).id());
    CHECK(newChunk->shot(0).distance().value() == QStringLiteral("0"));
    checkNameOnly(newChunk->station(0));
}

TEST_CASE("canSplitChunk answers only for a split that leaves both halves a shot",
          "[cwTrip][SplitChunk]")
{
    SplitFixture fixture(readingShot());
    cwSurveyChunk* chunk = fixture.chunk;
    const int lastShot = chunk->shotCount() - 1;

    CHECK(fixture.trip->canSplitChunk(chunk, 1));
    CHECK(fixture.trip->canSplitChunk(chunk, lastShot));

    CHECK_FALSE(fixture.trip->canSplitChunk(chunk, 0));
    CHECK_FALSE(fixture.trip->canSplitChunk(chunk, -1));
    CHECK_FALSE(fixture.trip->canSplitChunk(chunk, chunk->shotCount()));
    CHECK_FALSE(fixture.trip->canSplitChunk(nullptr, 1));

    SECTION("An empty last shot would leave the new chunk no shot") {
        auto data = chunk->data();
        data.shots.replace(lastShot, cwShot());
        chunk->setData(data);
        REQUIRE(chunk->isShotDataEmpty(lastShot));

        CHECK_FALSE(fixture.trip->canSplitChunk(chunk, lastShot));
        CHECK(fixture.trip->canSplitChunk(chunk, lastShot - 1));
    }

    SECTION("A chunk from another trip, or from none, is left alone") {
        SplitFixture otherFixture(readingShot());
        CHECK(otherFixture.trip->canSplitChunk(otherFixture.chunk, 1));
        CHECK_FALSE(fixture.trip->canSplitChunk(otherFixture.chunk, 1));

        cwSurveyChunk loose;
        loose.appendShot(cwStation(QStringLiteral("L1")), cwStation(QStringLiteral("L2")), readingShot());
        loose.appendShot(cwStation(QStringLiteral("L2")), cwStation(QStringLiteral("L3")), readingShot());
        CHECK_FALSE(fixture.trip->canSplitChunk(&loose, 1));
    }
}

TEST_CASE("splitChunk refuses a split canSplitChunk refuses", "[cwTrip][SplitChunk]")
{
    SplitFixture fixture(readingShot());
    SplitFixture otherFixture(readingShot());

    //One split that works, so the refusals below are the rule and not the stub
    REQUIRE(otherFixture.trip->splitChunk(otherFixture.chunk, 1) != nullptr);

    //A valid split point in its own trip, so only trip membership refuses it here
    SplitFixture foreignFixture(readingShot());
    REQUIRE(foreignFixture.trip->canSplitChunk(foreignFixture.chunk, 1));

    SignalSpyChecker::SignalSpy insertedSpy(fixture.trip.get(), &cwTrip::chunksInserted);
    SignalSpyChecker::SignalSpy aboutToRemoveSpy(fixture.chunk, &cwSurveyChunk::aboutToRemove);
    SignalSpyChecker::Constant checker {
        {&insertedSpy, 0},
        {&aboutToRemoveSpy, 0}
    };

    CHECK(fixture.trip->splitChunk(fixture.chunk, 0) == nullptr);
    CHECK(fixture.trip->splitChunk(fixture.chunk, fixture.chunk->shotCount()) == nullptr);
    CHECK(fixture.trip->splitChunk(nullptr, 1) == nullptr);
    CHECK(fixture.trip->splitChunk(otherFixture.before, 0) == nullptr);
    CHECK(fixture.trip->splitChunk(foreignFixture.chunk, 1) == nullptr);
    CHECK(foreignFixture.trip->chunkCount() == 3);
    CHECK(foreignFixture.chunk->shotCount() == 4);

    checker.checkSpies();
    CHECK(fixture.trip->chunkCount() == 3);
    CHECK(fixture.chunk->stationCount() == 5);
    CHECK(fixture.chunk->shotCount() == 4);
}

TEST_CASE("splitChunk inserts the new chunk before it trims the old one, behind one removal",
          "[cwTrip][SplitChunk]")
{
    const bool keepShot = GENERATE(true, false);
    INFO("split shot has readings: " << keepShot);
    SplitFixture fixture(keepShot ? readingShot() : cwShot());

    SignalSpyChecker::SignalSpy insertedSpy(fixture.trip.get(), &cwTrip::chunksInserted);
    SignalSpyChecker::SignalSpy tripAboutToRemoveSpy(fixture.trip.get(), &cwTrip::chunksAboutToBeRemoved);
    SignalSpyChecker::SignalSpy countSpy(fixture.trip.get(), &cwTrip::numberOfChunksChanged);
    SignalSpyChecker::SignalSpy aboutToRemoveSpy(fixture.chunk, &cwSurveyChunk::aboutToRemove);
    SignalSpyChecker::SignalSpy removedSpy(fixture.chunk, &cwSurveyChunk::removed);
    SignalSpyChecker::SignalSpy addedSpy(fixture.chunk, &cwSurveyChunk::added);
    SignalSpyChecker::Constant checker {
        {&insertedSpy, 0},
        {&tripAboutToRemoveSpy, 0},
        {&countSpy, 0},
        {&aboutToRemoveSpy, 0},
        {&removedSpy, 0},
        {&addedSpy, 0}
    };

    QStringList order;
    QObject::connect(fixture.trip.get(), &cwTrip::chunksInserted, fixture.trip.get(),
                     [&order]() { order.append(QStringLiteral("inserted")); });
    QObject::connect(fixture.chunk, &cwSurveyChunk::aboutToRemove, fixture.chunk,
                     [&order]() { order.append(QStringLiteral("aboutToRemove")); });

    cwSurveyChunk* newChunk = fixture.trip->splitChunk(fixture.chunk, kSplitShot);
    REQUIRE(newChunk != nullptr);

    checker[&insertedSpy] = 1;
    checker[&countSpy] = 1;
    checker[&aboutToRemoveSpy] = 1;
    checker[&removedSpy] = 1;
    checker.checkSpies();

    const int newIndex = 2;
    REQUIRE(insertedSpy.size() == 1);
    CHECK(insertedSpy.at(0).at(0).toInt() == newIndex);
    CHECK(insertedSpy.at(0).at(1).toInt() == newIndex);

    //Stations 3..4 (A4, A5) and shots 2..3 leave the upper chunk either way
    const QList<QVariant> expectedRange {kSplitShot + 1, 4, kSplitShot, 3};
    REQUIRE(aboutToRemoveSpy.size() == 1);
    REQUIRE(removedSpy.size() == 1);
    CHECK(aboutToRemoveSpy.at(0) == expectedRange);
    CHECK(removedSpy.at(0) == expectedRange);

    CHECK(order == QStringList({"inserted", "aboutToRemove"}));
}

TEST_CASE("removeTail drops the stations from an index on behind one removal",
          "[cwSurveyChunk][SplitChunk]")
{
    SplitFixture fixture(readingShot());
    cwSurveyChunk* chunk = fixture.chunk;

    SignalSpyChecker::SignalSpy aboutToRemoveSpy(chunk, &cwSurveyChunk::aboutToRemove);
    SignalSpyChecker::SignalSpy removedSpy(chunk, &cwSurveyChunk::removed);
    SignalSpyChecker::Constant checker {
        {&aboutToRemoveSpy, 0},
        {&removedSpy, 0}
    };

    SECTION("An index that would leave no shot, or that is past the end, changes nothing") {
        chunk->removeTail(1);
        chunk->removeTail(0);
        chunk->removeTail(-1);
        chunk->removeTail(chunk->stationCount());
        checker.checkSpies();
        CHECK(chunk->stationCount() == 5);
        CHECK(chunk->shotCount() == 4);

        //The same chunk does give up its tail from 2 on
        chunk->removeTail(2);
        CHECK(stationNames(chunk) == QStringList({"A1", "A2"}));
    }

    SECTION("Removing from station 2 keeps A1, A2 and the shot between them") {
        chunk->removeTail(2);

        checker[&aboutToRemoveSpy] = 1;
        checker[&removedSpy] = 1;
        checker.checkSpies();

        const QList<QVariant> expectedRange {2, 4, 1, 3};
        REQUIRE(aboutToRemoveSpy.size() == 1);
        CHECK(aboutToRemoveSpy.at(0) == expectedRange);
        CHECK(removedSpy.at(0) == expectedRange);

        CHECK(stationNames(chunk) == QStringList({"A1", "A2"}));
        REQUIRE(chunk->shotCount() == 1);
        CHECK(chunk->shot(0).id() == fixture.originalShots.at(0).id());
        CHECK(chunk->isValid());
    }

    SECTION("Removing only the last station takes the last shot with it") {
        chunk->removeTail(4);

        const QList<QVariant> expectedRange {4, 4, 3, 3};
        REQUIRE(removedSpy.size() == 1);
        CHECK(removedSpy.at(0) == expectedRange);
        CHECK(stationNames(chunk) == QStringList({"A1", "A2", "A3", "A4"}));
        CHECK(chunk->shotCount() == 3);
    }
}
