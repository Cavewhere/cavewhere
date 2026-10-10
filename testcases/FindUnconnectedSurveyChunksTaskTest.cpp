/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwFindUnconnectedSurveyChunks.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwCavingRegionData.h"
#include "cwEquate.h"
#include "cwFixStation.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"

using Results = QHash<QUuid, QList<cwFindUnconnectedSurveyChunks::Result>>;

namespace {

//! A chunk of one shot per consecutive pair of \a names.
cwSurveyChunkData chunkOf(const QStringList& names)
{
    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("10")));
    shot.setCompass(cwCompassReading(QStringLiteral("0")));
    shot.setClino(cwClinoReading(QStringLiteral("0")));

    cwSurveyChunkData chunk;
    chunk.id = QUuid::createUuid();
    for (const QString& name : names) {
        chunk.stations.append(cwStation(name));
    }
    for (int i = 1; i < names.size(); i++) {
        chunk.shots.append(shot);
    }
    return chunk;
}

cwTripData tripOf(const QString& name, const QList<cwSurveyChunkData>& chunks)
{
    cwTripData trip;
    trip.id = QUuid::createUuid();
    trip.name = name;
    trip.chunks = chunks;
    return trip;
}

cwCaveData nodeOf(const QString& name, const QList<cwTripData>& trips)
{
    cwCaveData node;
    node.id = QUuid::createUuid();
    node.name = name;
    node.trips = trips;
    return node;
}

cwFixStation fixAt(const QString& stationName)
{
    cwFixStation fix;
    fix.setStationName(stationName);
    fix.setCoordinate(0.0, 0.0, 0.0);
    return fix;
}

cwStationHandle nativeHandle(const cwCaveData& node, const QString& tail)
{
    return cwStationHandle(cwStationHandle::NativeCave, node.id, tail);
}

} // namespace

TEST_CASE("Test the find unconnected survey chunks", "[FindUnconnectedSurveyChunksTask]") {

    cwCavingRegion region;
    cwCave* cave = new cwCave();
    cave->setName(QStringLiteral("Alpha"));
    region.addCave(cave);
    cwTrip* trip = new cwTrip();

    cave->addTrip(trip);

    int numStations = 25;
    QList<cwStation> stations;
    for(int i = 0; i < numStations; i++) {
        cwStation station;
        station.setName(QString("%1").arg(i+1));
        station.setLeft(cwDistanceReading("0"));
        station.setRight(cwDistanceReading("0"));
        station.setUp(cwDistanceReading("0"));
        station.setDown(cwDistanceReading("0"));

        stations.append(station);
    }


    cwShot shot;
    shot.setDistance(cwDistanceReading("10"));
    shot.setCompass(cwCompassReading("0"));
    shot.setBackCompass(cwCompassReading("180"));
    shot.setClino(cwClinoReading("0"));
    shot.setBackClino(cwClinoReading("0"));

    auto runUnconnected = [&]() {
        const Results results = cwFindUnconnectedSurveyChunks::find(region.data());
        CHECK(results.size() <= 1);
        return results.value(cave->id());
    };

    SECTION("Single a single chunk should be connected to the cave") {

        cwSurveyChunk* chunk = new cwSurveyChunk();
        trip->addChunk(chunk);

        for(int i = 0; i < 10; i++) {
            chunk->appendShot(stations.at(i), stations.at(i+1), shot);
        }

        CHECK(runUnconnected().isEmpty() == true);
    }

    SECTION("The connection should be made, even if the station name case are different") {
        cwSurveyChunk* chunk1 = new cwSurveyChunk();
        trip->addChunk(chunk1);

        cwSurveyChunk* chunk2 = new cwSurveyChunk();
        trip->addChunk(chunk2);

        QList<cwStation> withCapitals;
        QList<cwStation> withoutCapitals;

        for(int i = 0; i < stations.size(); i++) {
            cwStation stationA;
            stationA = stations.at(i);
            stationA.setName(QString("A") + stationA.name());

            cwStation stationB;
            stationB = stations.at(i);
            stationB.setName(QString("b") + stationB.name());

            withCapitals.append(stationA);
            withoutCapitals.append(stationB);
        }

        for(int i = 0; i < 10; i++) {
            chunk1->appendShot(withCapitals.at(i), withCapitals.at(i+1), shot);
        }

        for(int i = 0; i < 10; i++) {
            chunk2->appendShot(withoutCapitals.at(i), withoutCapitals.at(i+1), shot);
        }

        CHECK(runUnconnected().size() == 1);

        cwStation newCaptical = withoutCapitals.first();
        newCaptical.setName(newCaptical.name().toUpper());

        chunk1->appendShot(chunk1->stations().last(), newCaptical, shot);

        CHECK(runUnconnected().size() == 0);
    }

    SECTION("Two chunk's that aren't connected") {

        cwSurveyChunk* chunk1 = new cwSurveyChunk();
        cwSurveyChunk* chunk2 = new cwSurveyChunk();

        trip->addChunk(chunk1);
        trip->addChunk(chunk2);

        for(int i = 0; i < 5; i++) {
            chunk1->appendShot(stations.at(i), stations.at(i+1), shot);
        }

        for(int i = 6; i < 10; i++) {
            chunk2->appendShot(stations.at(i), stations.at(i+1), shot);
        }

        {
            const auto results = runUnconnected();
            REQUIRE(results.size() == 1);
            CHECK(results.first().TripIndex == 0);
            CHECK(results.first().SurveyChunkIndex == 1);
            CHECK(results.first().Error.message() == "Survey leg isn't connect to the cave");
        }

        SECTION("Add another chunk that isn't connected") {
            cwSurveyChunk* chunk3 = new cwSurveyChunk();

            trip->addChunk(chunk3);

            for(int i = 11; i < 15; i++) {
                chunk3->appendShot(stations.at(i), stations.at(i+1), shot);
            }

            {
                const auto results = runUnconnected();
                REQUIRE(results.size() == 2);
                CHECK(results.at(0).TripIndex == 0);
                CHECK(results.at(0).SurveyChunkIndex == 1);
                CHECK(results.at(1).TripIndex == 0);
                CHECK(results.at(1).SurveyChunkIndex == 2);
            }

            SECTION("Add another chunk that connects all them together") {
                cwSurveyChunk* chunk4 = new cwSurveyChunk();
                trip->addChunk(chunk4);

                for(int i = 0; i < 16; i++) {
                    chunk4->appendShot(stations.at(i), stations.at(i+1), shot);
                }

                REQUIRE(runUnconnected().size() == 0);


                SECTION("Add two trips, first not corrected, second is connected") {
                    cwTrip* trip2 = new cwTrip(); //No connected
                    cwTrip* trip3 = new cwTrip(); //Connected

                    cave->addTrip(trip2);
                    cave->addTrip(trip3);

                    cwSurveyChunk* chunk5 = new cwSurveyChunk();
                    trip2->addChunk(chunk5);

                    for(int i = 17; i < 20; i++) {
                        chunk5->appendShot(stations.at(i), stations.at(i+1), shot);
                    }

                    //Prove that it isn't connected
                    {
                        const auto results = runUnconnected();
                        REQUIRE(results.size() == 1);
                        CHECK(results.first().TripIndex == 1);
                        CHECK(results.first().SurveyChunkIndex == 0);
                        CHECK(results.first().Error.message() == "Survey leg isn't connect to the cave");
                    }

                    //Now connect trip2 to the cave with trip3
                    cwSurveyChunk* chunk6 = new cwSurveyChunk();
                    trip3->addChunk(chunk6);

                    for(int i = 14; i < 18; i++) {
                        chunk6->appendShot(stations.at(i), stations.at(i+1), shot);
                    }

                    //All connected should have no errors
                    REQUIRE(runUnconnected().size() == 0);
                }

                SECTION("Add a trip with empty chunk") {
                    cwTrip* trip2 = new cwTrip();
                    cave->addTrip(trip2);

                    cwSurveyChunk* chunk5 = new cwSurveyChunk();
                    trip2->addChunk(chunk5);

                    chunk5->appendNewShot(); //Black shot

                    //All connected should have no errors
                    REQUIRE(runUnconnected().size() == 0);
                }
            }
        }
    }
}

TEST_CASE("A chunk tied in only through an equate to another node is connected",
          "[FindUnconnectedSurveyChunksTask][Equate]")
{
    cwCavingRegionData region;
    region.caves.append(nodeOf(QStringLiteral("Alpha"),
                               {tripOf(QStringLiteral("Main"), {chunkOf({QStringLiteral("A1"), QStringLiteral("A2")})})}));
    cwCaveData side = nodeOf(QStringLiteral("Side"),
                             {tripOf(QStringLiteral("Upper"), {chunkOf({QStringLiteral("B1"), QStringLiteral("B2")})}),
                              tripOf(QStringLiteral("Lower"), {chunkOf({QStringLiteral("A2"), QStringLiteral("C1")})})});
    region.caves.first().nodes.append(side);

    // Lower shares the name A2 with Alpha, but a name is joined only within its
    // own node's scope, so Lower reaches neither Upper nor Alpha.
    const Results untied = cwFindUnconnectedSurveyChunks::find(region);
    REQUIRE(untied.size() == 1);
    const QList<cwFindUnconnectedSurveyChunks::Result> sideResults = untied.value(side.id);
    REQUIRE(sideResults.size() == 1);
    CHECK(sideResults.first().TripIndex == 1);
    CHECK(sideResults.first().SurveyChunkIndex == 0);

    region.equates.append(cwEquate({nativeHandle(side, QStringLiteral("C1")),
                                    nativeHandle(region.caves.first(), QStringLiteral("A2"))}));
    CHECK(cwFindUnconnectedSurveyChunks::find(region).isEmpty());
}

TEST_CASE("A chunk with a fix is connected", "[FindUnconnectedSurveyChunksTask]")
{
    cwCavingRegionData region;
    cwCaveData cave = nodeOf(QStringLiteral("Alpha"),
                             {tripOf(QStringLiteral("First"), {chunkOf({QStringLiteral("A1"), QStringLiteral("A2")})}),
                              tripOf(QStringLiteral("Second"), {chunkOf({QStringLiteral("B1"), QStringLiteral("B2")})})});

    SECTION("Without a fix the first chunk anchors the node")
    {
        region.caves.append(cave);
        const QList<cwFindUnconnectedSurveyChunks::Result> results =
            cwFindUnconnectedSurveyChunks::find(region).value(cave.id);
        REQUIRE(results.size() == 1);
        CHECK(results.first().TripIndex == 1);
    }

    SECTION("A fix on the second chunk anchors it in place of the first")
    {
        cave.fixStations.append(fixAt(QStringLiteral("b2")));
        region.caves.append(cave);
        const QList<cwFindUnconnectedSurveyChunks::Result> results =
            cwFindUnconnectedSurveyChunks::find(region).value(cave.id);
        REQUIRE(results.size() == 1);
        CHECK(results.first().TripIndex == 0);
    }

    SECTION("A fix on each chunk anchors both")
    {
        cave.fixStations.append(fixAt(QStringLiteral("A1")));
        cave.fixStations.append(fixAt(QStringLiteral("B2")));
        region.caves.append(cave);
        CHECK(cwFindUnconnectedSurveyChunks::find(region).isEmpty());
    }

    SECTION("A fix naming no station of the node leaves the first chunk as the anchor")
    {
        cave.fixStations.append(fixAt(QStringLiteral("Z9")));
        region.caves.append(cave);
        const QList<cwFindUnconnectedSurveyChunks::Result> results =
            cwFindUnconnectedSurveyChunks::find(region).value(cave.id);
        REQUIRE(results.size() == 1);
        CHECK(results.first().TripIndex == 1);
    }
}

TEST_CASE("Two nodes each anchored by their own fix are both connected",
          "[FindUnconnectedSurveyChunksTask]")
{
    cwCavingRegionData region;
    cwCaveData alpha = nodeOf(QStringLiteral("Alpha"),
                              {tripOf(QStringLiteral("Stray"), {chunkOf({QStringLiteral("X1"), QStringLiteral("X2")})}),
                               tripOf(QStringLiteral("Main"), {chunkOf({QStringLiteral("A1"), QStringLiteral("A2")})})});
    alpha.fixStations.append(fixAt(QStringLiteral("A1")));
    cwCaveData beta = nodeOf(QStringLiteral("Beta"),
                             {tripOf(QStringLiteral("Stray"), {chunkOf({QStringLiteral("Y1"), QStringLiteral("Y2")})}),
                              tripOf(QStringLiteral("Main"), {chunkOf({QStringLiteral("B1"), QStringLiteral("B2")})})});
    beta.fixStations.append(fixAt(QStringLiteral("B2")));
    alpha.nodes.append(beta);
    region.caves.append(alpha);

    // Each node's fixed survey is placed and only its stray floats, nested
    // under an anchored parent or not.
    const Results results = cwFindUnconnectedSurveyChunks::find(region);
    REQUIRE(results.size() == 2);
    REQUIRE(results.value(alpha.id).size() == 1);
    CHECK(results.value(alpha.id).first().TripIndex == 0);
    REQUIRE(results.value(beta.id).size() == 1);
    CHECK(results.value(beta.id).first().TripIndex == 0);
}

TEST_CASE("A chunk tied to an attached file's station is connected",
          "[FindUnconnectedSurveyChunksTask][Equate]")
{
    cwTripData attached = tripOf(QStringLiteral("Attached"), {});
    attached.externalCenterline = cwExternalCenterline(QStringLiteral("survex_simple.svx"));
    attached.externalStations = {QStringLiteral("s1"), QStringLiteral("s2")};

    cwTripData native = tripOf(QStringLiteral("Native"),
                               {chunkOf({QStringLiteral("A1"), QStringLiteral("A2")}),
                                chunkOf({QStringLiteral("C1"), QStringLiteral("C2")})});

    cwCaveData cave = nodeOf(QStringLiteral("Alpha"), {native, attached});
    cave.fixStations.append(fixAt(QStringLiteral("A1")));

    cwCavingRegionData region;
    region.caves.append(cave);
    REQUIRE(cwFindUnconnectedSurveyChunks::find(region).value(cave.id).size() == 1);

    region.equates.append(cwEquate({nativeHandle(cave, QStringLiteral("C1")),
                                    cwStationHandle(cwStationHandle::Trip, attached.id, QStringLiteral("s2"))}));
    CHECK(cwFindUnconnectedSurveyChunks::find(region).isEmpty());

    SECTION("An excluded attachment anchors nothing")
    {
        const Results results = cwFindUnconnectedSurveyChunks::find(region, {attached.id});
        CHECK(results.value(cave.id).size() == 1);
    }
}
