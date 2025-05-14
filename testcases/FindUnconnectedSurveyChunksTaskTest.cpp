/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwFindUnconnectedSurveyChunksTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"

TEST_CASE("Test the find unconnected survey chunks", "[FindUnconnectedSurveyChunksTask]") {

    auto cave = std::make_unique<cwCave>();
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

    auto task = std::make_unique<cwFindUnconnectedSurveyChunksTask>();

    SECTION("Single a single chunk should be connected to the cave") {

        cwSurveyChunk* chunk = new cwSurveyChunk();
        trip->addChunk(chunk);

        for(int i = 0; i < 10; i++) {
            chunk->appendShot(stations.at(i), stations.at(i+1), shot);
        }

        task->setCave(cave.get());
        task->start();
        task->waitToFinish();

        CHECK(task->results().isEmpty() == true);
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

        task->setCave(cave.get());
        task->start();
        task->waitToFinish();

        CHECK(task->results().size() == 1);

        cwStation newCaptical = withoutCapitals.first();
        newCaptical.setName(newCaptical.name().toUpper());

        chunk1->appendShot(chunk1->stations().last(), newCaptical, shot);

        task->start();
        task->waitToFinish();
        CHECK(task->results().size() == 0);
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

        task->setCave(cave.get());
        task->start();
        task->waitToFinish();

        REQUIRE(task->results().size() == 1);
        CHECK(task->results().first().TripIndex == 0);
        CHECK(task->results().first().SurveyChunkIndex == 1);
        CHECK(task->results().first().Error.message() == "Survey leg isn't connect to the cave");

        SECTION("Add another chunk that isn't connected") {
            cwSurveyChunk* chunk3 = new cwSurveyChunk();

            trip->addChunk(chunk3);

            for(int i = 11; i < 15; i++) {
                chunk3->appendShot(stations.at(i), stations.at(i+1), shot);
            }

            task->start();
            task->waitToFinish();

            REQUIRE(task->results().size() == 2);
            CHECK(task->results().at(0).TripIndex == 0);
            CHECK(task->results().at(0).SurveyChunkIndex == 1);
            CHECK(task->results().at(1).TripIndex == 0);
            CHECK(task->results().at(1).SurveyChunkIndex == 2);

            SECTION("Add another chunk that connects all them together") {
                cwSurveyChunk* chunk4 = new cwSurveyChunk();
                trip->addChunk(chunk4);

                for(int i = 0; i < 16; i++) {
                    chunk4->appendShot(stations.at(i), stations.at(i+1), shot);
                }

                task->start();
                task->waitToFinish();

                REQUIRE(task->results().size() == 0);


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

                    task->start();
                    task->waitToFinish();

                    //Prove that it isn't connected
                    REQUIRE(task->results().size() == 1);
                    CHECK(task->results().first().TripIndex == 1);
                    CHECK(task->results().first().SurveyChunkIndex == 0);
                    CHECK(task->results().first().Error.message() == "Survey leg isn't connect to the cave");

                    //Now connect trip2 to the cave with trip3
                    cwSurveyChunk* chunk6 = new cwSurveyChunk();
                    trip3->addChunk(chunk6);

                    for(int i = 14; i < 18; i++) {
                        chunk6->appendShot(stations.at(i), stations.at(i+1), shot);
                    }

                    task->start();
                    task->waitToFinish();

                    //All connected should have no errors
                    REQUIRE(task->results().size() == 0);
                }

                SECTION("Add a trip with empty chunk") {
                    cwTrip* trip2 = new cwTrip();
                    cave->addTrip(trip2);

                    cwSurveyChunk* chunk5 = new cwSurveyChunk();
                    trip2->addChunk(chunk5);

                    chunk5->appendNewShot(); //Black shot

                    //All connected should have no errors
                    REQUIRE(task->results().size() == 0);
                }
            }
        }
    }
}


