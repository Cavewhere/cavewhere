//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwCave.h"
#include "cwTrip.h"

TEST_CASE("cwCave setData should reset trips", "[cwCave]") {
    cwCave cave;
    QUndoStack undoStack;

    SECTION("With undo") {
        cave.setUndoStack(&undoStack);
    }

    auto oldTrip1 = new cwTrip(&cave);
    oldTrip1->setName("Old Trip 1");
    QPointer<cwTrip> oldTrip1Ptr(oldTrip1);
    auto oldTrip2 = new cwTrip(&cave);
    oldTrip2->setName("Old Trip 2");
    QPointer<cwTrip> oldTrip2Ptr(oldTrip2);

    cave.addTrip(oldTrip1);
    cave.addTrip(oldTrip2);
    REQUIRE(cave.tripCount() == 2);

    cwTripData tripData;
    tripData.name = "New Trip";

    cwCaveData newData;
    newData.name = "New Cave";
    newData.trips.append(tripData);
    newData.stationPositionModel = cwStationPositionLookup();

    cave.setData(newData);

    CHECK(cave.name().toStdString() == "New Cave");
    REQUIRE(cave.tripCount() == 1);
    CHECK(cave.trip(0)->name().toStdString() == "New Trip");
    CHECK(cave.trip(0) != oldTrip1);
    CHECK(cave.trip(0) != oldTrip2);
    CHECK(cave.indexOf(oldTrip1) == -1);
    CHECK(cave.indexOf(oldTrip2) == -1);

    if(cave.undoStack() != nullptr) {
        REQUIRE(!oldTrip1Ptr.isNull());
        REQUIRE(!oldTrip2Ptr.isNull());
        CHECK(oldTrip1Ptr->parent() == &cave);
        CHECK(oldTrip2Ptr->parent() == &cave);
    } else {
        CHECK(oldTrip1Ptr.isNull());
        CHECK(oldTrip2Ptr.isNull());
    }
}

TEST_CASE("cwCave clearTrips should remove all trips", "[cwCave]") {
    cwCave cave;
    QUndoStack undoStack;

    SECTION("With undo") {
        cave.setUndoStack(&undoStack);
    }

    auto trip1 = new cwTrip(&cave);
    trip1->setName("Trip 1");
    QPointer<cwTrip> trip1Ptr(trip1);
    auto trip2 = new cwTrip(&cave);
    trip2->setName("Trip 2");
    QPointer<cwTrip> trip2Ptr(trip2);

    cave.addTrip(trip1);
    cave.addTrip(trip2);
    REQUIRE(cave.tripCount() == 2);

    cave.clearTrips();

    CHECK(cave.tripCount() == 0);
    CHECK(cave.indexOf(trip1) == -1);
    CHECK(cave.indexOf(trip2) == -1);

    if(cave.undoStack() != nullptr) {
        REQUIRE(!trip1Ptr.isNull());
        REQUIRE(!trip2Ptr.isNull());
        CHECK(trip1Ptr->parent() == &cave);
        CHECK(trip2Ptr->parent() == &cave);
    } else {
        CHECK(trip1Ptr.isNull());
        CHECK(trip2Ptr.isNull());
    }
}
