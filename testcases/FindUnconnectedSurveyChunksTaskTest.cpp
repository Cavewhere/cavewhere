/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include "catch.hpp"

//Our includes
#include "cwFindUnconnectedSurveyChunksTask.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyChunk.h"

TEST_CASE("Test the find unconnected survey chunks") {

    cwCave* cave = new cwCave();
    cwTrip* trip = new cwTrip();

    cave->addTrip(trip);

    int numStations = 25;
    QList<cwStation> stations;
    for(int i = 0; i < numStations; i++) {
        cwStation station;
        station.setName(QString("%1").arg(i+1));
        station.setLeft("0");
        station.setRight("0");
        station.setUp("0");
        station.setDown("0");

        stations.append(station);
    }


    cwShot shot;
    shot.setDistance("10");
    shot.setCompass("0");
    shot.setBackCompass("180");
    shot.setClino("0");
    shot.setBackClino("0");

    cwFindUnconnectedSurveyChunksTask* task = new cwFindUnconnectedSurveyChunksTask();

    SECTION("Single a single chunk should be connected to the cave") {

        cwSurveyChunk* chunk = new cwSurveyChunk();
        trip->addChunk(chunk);

        for(int i = 0; i < 10; i++) {
            chunk->appendShot(stations.at(i), stations.at(i+1), shot);
        }

        task->setCave(cave);
        task->start();
        task->waitToFinish();

        CHECK(task->results().isEmpty() == true);
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

        task->setCave(cave);
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


