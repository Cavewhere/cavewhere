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
#include "cwCavingRegion.h"
#include "cwLinePlotManager.h"
#include "cwShot.h"
#include "cwSketchManager.h"
#include "cwStation.h"
#include "cwStationPositionLookup.h"
#include "cwSurveyChunk.h"
#include "cwTrip.h"

//Test includes
#include "SignalSpyChecker.h"
#include "SurveyEditorTestHelper.h"

//Qt includes
#include <QSignalSpy>
#include <QStringList>
#include <QVector3D>

//Std includes
#include <memory>

namespace {

constexpr int kSignalWaitMs = 5000;
constexpr double kShotLength = 10.0;

using ChunkList = QList<cwSurveyChunk*>;

/**
 * A trip of three chunks: A1 to A2, then B1 to B2, then C1 to C2.
 */
struct MoveFixture {
    MoveFixture()
        : trip(std::make_unique<cwTrip>()),
          a(chunkWithNames({"A1", "A2"})),
          b(chunkWithNames({"B1", "B2"})),
          c(chunkWithNames({"C1", "C2"}))
    {
        trip->addChunk(a);
        trip->addChunk(b);
        trip->addChunk(c);
        ids = {a->id(), b->id(), c->id()};
    }

    std::unique_ptr<cwTrip> trip;
    cwSurveyChunk* a;
    cwSurveyChunk* b;
    cwSurveyChunk* c;
    QList<QUuid> ids;
};

//A shot due north with no LRUD or backsights
cwShot northShot()
{
    cwShot shot;
    shot.setDistance(cwDistanceReading(QString::number(kShotLength)));
    shot.setCompass(cwCompassReading(QStringLiteral("0")));
    shot.setClino(cwClinoReading(QStringLiteral("0")));
    return shot;
}

/**
 * A cave with one trip of two chunks, a1 to a2 and then a2 to a3, both due
 * north. With no fix station, survex puts the first station of the first chunk
 * at the origin.
 */
struct LinePlotFixture {
    LinePlotFixture()
        : cave(new cwCave()),
          trip(new cwTrip()),
          first(new cwSurveyChunk()),
          second(new cwSurveyChunk())
    {
        cave->setName(QStringLiteral("Cave 1"));
        region.addCave(cave);
        trip->setName(QStringLiteral("Trip 1"));
        cave->addTrip(trip);

        first->appendShot(cwStation(QStringLiteral("a1")), cwStation(QStringLiteral("a2")), northShot());
        second->appendShot(cwStation(QStringLiteral("a2")), cwStation(QStringLiteral("a3")), northShot());
        trip->addChunk(first);
        trip->addChunk(second);
    }

    QVector3D position(const QString& stationName) const
    {
        return cave->scopedPositions().position(stationName);
    }

    cwCavingRegion region;
    cwCave* cave;
    cwTrip* trip;
    cwSurveyChunk* first;
    cwSurveyChunk* second;
};

}

TEST_CASE("Moving a chunk reorders the trip and keeps every chunk as it was",
          "[cwTrip][MoveChunk]")
{
    MoveFixture fixture;

    SECTION("Down one place") {
        CHECK(fixture.trip->canMoveChunk(0, 1));
        fixture.trip->moveChunk(0, 1);
        CHECK(fixture.trip->chunks() == ChunkList({fixture.b, fixture.a, fixture.c}));
    }

    SECTION("Up one place") {
        CHECK(fixture.trip->canMoveChunk(2, 1));
        fixture.trip->moveChunk(2, 1);
        CHECK(fixture.trip->chunks() == ChunkList({fixture.a, fixture.c, fixture.b}));
    }

    SECTION("Across the trip, with the index the chunk lands at") {
        CHECK(fixture.trip->canMoveChunk(0, 2));
        fixture.trip->moveChunk(0, 2);
        CHECK(fixture.trip->chunks() == ChunkList({fixture.b, fixture.c, fixture.a}));
    }

    CHECK(fixture.trip->chunkCount() == 3);
    for(cwSurveyChunk* chunk : fixture.trip->chunks()) {
        CHECK(chunk->parentTrip() == fixture.trip.get());
        CHECK(fixture.ids.contains(chunk->id()));
    }
    CHECK(fixture.a->id() == fixture.ids.at(0));
    CHECK(stationNames(fixture.a) == QStringList({"A1", "A2"}));
    CHECK(stationNames(fixture.b) == QStringList({"B1", "B2"}));
    CHECK(stationNames(fixture.c) == QStringList({"C1", "C2"}));
}

TEST_CASE("A move off the ends of the trip or onto the same place changes nothing",
          "[cwTrip][MoveChunk]")
{
    MoveFixture fixture;

    //One move that works, so the refusals below are the rule and not the stub
    MoveFixture working;
    working.trip->moveChunk(0, 1);
    REQUIRE(working.trip->chunk(0) == working.b);

    struct Move {
        int from;
        int to;
    };
    const QList<Move> refused {
        {0, -1},
        {2, 3},
        {1, 1},
        {-1, 0},
        {5, 0}
    };

    SignalSpyChecker::SignalSpy aboutToMoveSpy(fixture.trip.get(), &cwTrip::chunksAboutToBeMoved);
    SignalSpyChecker::SignalSpy movedSpy(fixture.trip.get(), &cwTrip::chunksMoved);
    SignalSpyChecker::Constant checker {
        {&aboutToMoveSpy, 0},
        {&movedSpy, 0}
    };

    for(const auto& move : refused) {
        INFO("from " << move.from << " to " << move.to);
        CHECK_FALSE(fixture.trip->canMoveChunk(move.from, move.to));
        fixture.trip->moveChunk(move.from, move.to);
        checker.checkSpies();
        CHECK(fixture.trip->chunks() == ChunkList({fixture.a, fixture.b, fixture.c}));
    }
}

TEST_CASE("moveChunk announces one move before and one after the reorder",
          "[cwTrip][MoveChunk]")
{
    MoveFixture fixture;

    SignalSpyChecker::SignalSpy aboutToMoveSpy(fixture.trip.get(), &cwTrip::chunksAboutToBeMoved);
    SignalSpyChecker::SignalSpy movedSpy(fixture.trip.get(), &cwTrip::chunksMoved);
    SignalSpyChecker::SignalSpy insertedSpy(fixture.trip.get(), &cwTrip::chunksInserted);
    SignalSpyChecker::SignalSpy aboutToRemoveSpy(fixture.trip.get(), &cwTrip::chunksAboutToBeRemoved);
    SignalSpyChecker::SignalSpy removedSpy(fixture.trip.get(), &cwTrip::chunksRemoved);
    SignalSpyChecker::SignalSpy countSpy(fixture.trip.get(), &cwTrip::numberOfChunksChanged);
    SignalSpyChecker::SignalSpy chunkAddedSpy(fixture.a, &cwSurveyChunk::added);
    SignalSpyChecker::SignalSpy chunkRemovedSpy(fixture.a, &cwSurveyChunk::removed);
    SignalSpyChecker::Constant checker {
        {&aboutToMoveSpy, 0},
        {&movedSpy, 0},
        {&insertedSpy, 0},
        {&aboutToRemoveSpy, 0},
        {&removedSpy, 0},
        {&countSpy, 0},
        {&chunkAddedSpy, 0},
        {&chunkRemovedSpy, 0}
    };

    //The about-to signal sees the old order, so a view can still find the rows
    QList<cwSurveyChunk*> orderAtAboutToMove;
    QList<cwSurveyChunk*> orderAtMoved;
    QObject::connect(fixture.trip.get(), &cwTrip::chunksAboutToBeMoved, fixture.trip.get(),
                     [&]() { orderAtAboutToMove = fixture.trip->chunks(); });
    QObject::connect(fixture.trip.get(), &cwTrip::chunksMoved, fixture.trip.get(),
                     [&]() { orderAtMoved = fixture.trip->chunks(); });

    fixture.trip->moveChunk(0, 1);

    checker[&aboutToMoveSpy] = 1;
    checker[&movedSpy] = 1;
    checker.checkSpies();

    const QList<QVariant> expected {0, 1};
    REQUIRE(aboutToMoveSpy.size() == 1);
    CHECK(aboutToMoveSpy.at(0) == expected);
    REQUIRE(movedSpy.size() == 1);
    CHECK(movedSpy.at(0) == expected);

    CHECK(orderAtAboutToMove == ChunkList({fixture.a, fixture.b, fixture.c}));
    CHECK(orderAtMoved == ChunkList({fixture.b, fixture.a, fixture.c}));
    CHECK(fixture.trip->chunkCount() == 3);
}

TEST_CASE("Moving the first chunk reruns the line plot from the new first station",
          "[cwTrip][MoveChunk][LinePlotManager]")
{
    LinePlotFixture fixture;

    cwLinePlotManager plotManager;
    plotManager.setRegion(&fixture.region);
    plotManager.waitToFinish();

    REQUIRE(fixture.position(QStringLiteral("a1")) == QVector3D(0.0, 0.0, 0.0));
    REQUIRE(fixture.position(QStringLiteral("a2")) == QVector3D(0.0, kShotLength, 0.0));

    fixture.trip->moveChunk(1, 0);
    REQUIRE(fixture.trip->chunk(0) == fixture.second);
    plotManager.waitToFinish();

    CHECK(fixture.position(QStringLiteral("a2")) == QVector3D(0.0, 0.0, 0.0));
    CHECK(fixture.position(QStringLiteral("a1")) == QVector3D(0.0, -kShotLength, 0.0));
    CHECK(fixture.position(QStringLiteral("a3")) == QVector3D(0.0, kShotLength, 0.0));
}

TEST_CASE("Moving the first chunk reruns the sketch line plot from the new anchor",
          "[cwTrip][MoveChunk][cwSketchManager]")
{
    LinePlotFixture fixture;

    cwSketchManager sketchManager;
    QSignalSpy firstRunSpy(&sketchManager, &cwSketchManager::linePlotUpdated);
    sketchManager.acquireLinePlot(fixture.trip);
    REQUIRE((firstRunSpy.count() > 0 || firstRunSpy.wait(kSignalWaitMs)));

    auto components = sketchManager.latestLinePlot(fixture.trip);
    REQUIRE(components.size() == 1);
    REQUIRE(components.first().canonicalAnchor == QStringLiteral("a1"));

    QSignalSpy updatedSpy(&sketchManager, &cwSketchManager::linePlotUpdated);
    fixture.trip->moveChunk(1, 0);
    REQUIRE(fixture.trip->chunk(0) == fixture.second);
    REQUIRE(updatedSpy.wait(kSignalWaitMs));

    components = sketchManager.latestLinePlot(fixture.trip);
    REQUIRE(components.size() == 1);
    CHECK(components.first().canonicalAnchor == QStringLiteral("a2"));

    sketchManager.releaseLinePlot(fixture.trip);
}
