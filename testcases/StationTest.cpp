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
        CHECK(station.left().value().toStdString() == "");
        CHECK(station.right().value().toStdString() == "");
        CHECK(station.up().value().toStdString() == "");
        CHECK(station.down().value().toStdString() == "");
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
        station.setLeft(QString("1.0"));
        station.setRight(QString("2.0"));
        station.setUp(QString("3.0"));
        station.setDown(QString("4.0"));

        CHECK(stationNameOkay == true);
        CHECK(station.name() == "a2");
        CHECK(station.left().value() == "1.0");
        CHECK(station.right().value() == "2.0");
        CHECK(station.up().value() == "3.0");
        CHECK(station.down().value() == "4.0");
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


