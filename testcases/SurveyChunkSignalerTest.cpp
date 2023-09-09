/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include <catch2/catch.hpp>

//Our includes
#include "cwSurveyChunkSignaler.h"
#include "cwCave.h"
#include "cwCavingRegion.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"

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

    SECTION("Add calibration connections") {
        signaler.addConnectionToTripCalibrations(SIGNAL(calibrationsChanged()), &slotHelper, SLOT(calibrationChangedCalled()));

        trip1->calibrations()->setTapeCalibration(20.0);

        CHECK(slotHelper.calibrationSender() == trip1->calibrations());
    }
}
