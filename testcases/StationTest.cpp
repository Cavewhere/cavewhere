/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "catch.hpp"

//Cavewhere includes
#include "cwStation.h"
#include "cwReadingStates.h"

TEST_CASE("Station can be created and data can be changed", "[station]") {
    cwStation station;

    SECTION("Check default constructor") {
        CHECK(station.name() == QString());
        CHECK(station.left() == 0.0);
        CHECK(station.right() == 0.0);
        CHECK(station.up() == 0.0);
        CHECK(station.down() == 0.0);
        CHECK(station.leftInputState() == cwDistanceStates::Empty);
        CHECK(station.rightInputState() == cwDistanceStates::Empty);
        CHECK(station.upInputState() == cwDistanceStates::Empty);
        CHECK(station.downInputState() == cwDistanceStates::Empty);
        CHECK(station.isValid() == false);
        CHECK(station == cwStation());
    }

    SECTION("Check station name constructor") {
        QString name("a1");
        cwStation stationName(name);
        CHECK(stationName.name() == name);
    }

    SECTION("Stations can have thier data changed") {
        bool stationNameOkay = station.setName("a2");
        station.setLeft(1.0);
        station.setRight(2.0);
        station.setUp(3.0);
        station.setDown(4.0);

        CHECK(stationNameOkay == true);
        CHECK(station.name() == "a2");
        CHECK(station.left() == 1.0);
        CHECK(station.right() == 2.0);
        CHECK(station.up() == 3.0);
        CHECK(station.down() == 4.0);
        CHECK(station.leftInputState() == cwDistanceStates::Valid);
        CHECK(station.rightInputState() == cwDistanceStates::Valid);
        CHECK(station.upInputState() == cwDistanceStates::Valid);
        CHECK(station.downInputState() == cwDistanceStates::Valid);
        CHECK(station.isValid() == true);
    }

    SECTION("Invalid station name's return false") {
        CHECK(station.setName("3") == true);
        CHECK(station.setName("123") == true);
        CHECK(station.setName("a1") == true);
        CHECK(station.setName("34b") == true);
        CHECK(station.setName("BUM") == true);
        CHECK(station.setName("S_A_Uce") == true);
        CHECK(station.setName("S-") == true);

        CHECK(station.setName("$") == false);
        CHECK(station.setName(":)") == false);
        CHECK(station.setName("|") == false);
        CHECK(station.setName("-5#") == false);

        CHECK(station.name() == "S-");
    }
}


