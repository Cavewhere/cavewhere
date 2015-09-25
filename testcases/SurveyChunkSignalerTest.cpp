/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "catch.hpp"

//Our includes
#include "cwSurveyChunkSignaler.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"

//Qt includes
#include <QPair>

//Testcase helpers
#include "SurveyChunkSignalerSlotHelper.h"

/**
 * @brief TEST_CASE
 *
 * This tests makes sure SurveyChunkSignaler connects caves, surveyChunks, and trips to a reciever's
 * slot. It test this by changing the cave, trip, or surveyChunk data and sees if the slot
 * is called in the SurveyChunkSignalerSlotHelper.
 */



TEST_CASE( "Connection are created between senders and recievers", "[SurveyChunkSignaler]" ) {

    cwCavingRegion region;
    cwCave* cave = new cwCave();

    region.addCave(cave);

    cwSurveyChunkSignaler signaler;
    signaler.setRegion(&region);

    SurveyChunkSignalerSlotHelper slotHelper;

    SECTION("Slot helper is initialized correctly") {
        REQUIRE(slotHelper.caveNameChanged() == nullptr);
        REQUIRE(slotHelper.tripNameChanged() == nullptr);
        REQUIRE(slotHelper.chunkSender() == nullptr);
        REQUIRE(slotHelper.chunkStationAddedIndexes() == BeginEndPair(0, 0));
    }

    SECTION("Add cave connections") {
        signaler.addConnectionToCaves(SIGNAL(nameChanged()), &slotHelper, SLOT(caveNameChangedCalled()));

        cave->setName("Sauce");

        //Check that the slots where called
        CHECK(slotHelper.caveNameChanged() == cave);

        //Tests that an added cave is automastically connected
        cwCave* cave2 = new cwCave();
        region.addCave(cave2);
        cave2->setName("Llama");
        CHECK(slotHelper.caveNameChanged() == cave2);
    }

    cwTrip* trip1 = new cwTrip();
    cave->addTrip(trip1);

    SECTION("Add trip connections") {
        signaler.addConnectionToTrips(SIGNAL(nameChanged()), &slotHelper, SLOT(tripNameChangedCalled()));

        trip1->setName("Trip 1");

        //Check that the slots where called
        CHECK(slotHelper.tripNameChanged() == trip1);

        //Tests that an added trip is automastically connected
        cwTrip* trip2 = new cwTrip();
        cave->addTrip(trip2);
        trip2->setName("Trip 2");
        CHECK(slotHelper.tripNameChanged() == trip2);
    }

    cwSurveyChunk* chunk1 = new cwSurveyChunk();
    trip1->addChunk(chunk1);

    SECTION("Add Survey Chunk connections") {
        signaler.addConnectionToChunks(SIGNAL(stationsAdded(int, int)), &slotHelper, SLOT(chunkStationAdded(int, int)));

        chunk1->appendNewShot();
        chunk1->setData(cwSurveyChunk::StationNameRole, 0, "a1");
        chunk1->setData(cwSurveyChunk::StationNameRole, 1, "a2");

        //Check that the slot was called
        CHECK(slotHelper.chunkSender() == chunk1);
        CHECK(slotHelper.chunkStationAddedIndexes() == BeginEndPair(1,1));

        //Tests that an added trip is automastically connected
        cwSurveyChunk* chunk2 = new cwSurveyChunk();
        trip1->addChunk(chunk2);

        chunk2->appendNewShot();
        chunk2->setData(cwSurveyChunk::StationNameRole, 0, "b1");
        chunk2->setData(cwSurveyChunk::StationNameRole, 1, "b2");
        chunk2->appendNewShot();
        chunk2->setData(cwSurveyChunk::StationNameRole, 2, "b3");

        CHECK(slotHelper.chunkSender() == chunk2);
        CHECK(slotHelper.chunkStationAddedIndexes() == BeginEndPair(2,2));
    }
}

TEST_CASE( "Disconnection happens when senders are removed", "[SurveyChunkSignaler]" ) {
    cwCavingRegion region;
    cwCave* cave1 = new cwCave();
    cwCave* cave2 = new cwCave();
    cwCave* cave3 = new cwCave();

    region.addCave(cave1);
    region.addCave(cave2);
    region.addCave(cave3);

    cwTrip* trip1 = new cwTrip();
    cwTrip* trip2 = new cwTrip();
    cwTrip* trip3 = new cwTrip();

    cwSurveyChunk* chunk1 = new cwSurveyChunk();
    cwSurveyChunk* chunk2 = new cwSurveyChunk();
    cwSurveyChunk* chunk3 = new cwSurveyChunk();

    cave1->addTrip(trip1);
    cave1->addTrip(trip2);
    cave1->addTrip(trip3);

    trip3->addChunk(chunk1);
    trip3->addChunk(chunk2);
    trip3->addChunk(chunk3);

    cwSurveyChunkSignaler signaler;
    signaler.setRegion(&region);

    SurveyChunkSignalerSlotHelper slotHelper;

    signaler.addConnectionToCaves(SIGNAL(nameChanged()), &slotHelper, SLOT(caveNameChangedCalled()));
    signaler.addConnectionToTrips(SIGNAL(nameChanged()), &slotHelper, SLOT(tripNameChangedCalled()));
    signaler.addConnectionToChunks(SIGNAL(stationsAdded(int, int)), &slotHelper, SLOT(chunkStationAdded(int, int)));

    SECTION("Disconnection happen when chunks are removed from the trip") {
        trip3->removeChunk(chunk2);

        //Chunk 2 should be diconnected
        chunk2->appendNewShot();
        CHECK(slotHelper.chunkSender() == nullptr);
        CHECK(slotHelper.chunkStationAddedIndexes() == BeginEndPair(0, 0));
    }

    SECTION("Disconnection happen when trips are removed from the cave") {
        cave1->removeTrip(2);

        //chunk1, chunk3 should be diconnected
        chunk1->appendNewShot();
        CHECK(slotHelper.chunkSender() == nullptr);
        CHECK(slotHelper.chunkStationAddedIndexes() == BeginEndPair(0, 0));

        chunk3->appendNewShot();
        CHECK(slotHelper.chunkSender() == nullptr);
        CHECK(slotHelper.chunkStationAddedIndexes() == BeginEndPair(0, 0));

        //trip3 should be diconnected
        trip3->setName("Trip3");
        CHECK(slotHelper.tripNameChanged() == nullptr);
    }


    SECTION("Disconnection happen when caves are removed from the region") {
        region.removeCave(0);

        //trip1, trip2 should be disconnected
        trip1->setName("trip 1");
        CHECK(slotHelper.tripNameChanged() == nullptr);

        trip2->setName("trip 2");
        CHECK(slotHelper.tripNameChanged() == nullptr);

        //cave1 should be disconnectd
        cave1->setName("cave 1");
        CHECK(slotHelper.caveNameChanged() == nullptr);

        //Other caves should still be connected
        cave2->setName("cave 2");
        CHECK(slotHelper.caveNameChanged() == cave2);
    }


}


