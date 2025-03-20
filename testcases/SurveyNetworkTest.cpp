/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwSurveyNetwork.h"

//Testcase includes
#include "TestHelper.h"

TEST_CASE("Build a survey network", "[SurveyNetwork]") {

    cwSurveyNetwork network;
    network.addShot("a1", "a2");
    network.addShot("a2", "a3");
    network.addShot("a3", "a1");
    network.addShot("a2", "a4");
    network.addShot("a2", "a5");

    auto testStationNeigbors = [=](QString stationName, QStringList neighbors) {
        auto neighborsAtStation = network.neighbors(stationName);
        auto foundNeighbors = QSet<QString>(neighborsAtStation.begin(), neighborsAtStation.end());
        auto checkNeigbbors = QSet<QString>(neighbors.begin(), neighbors.end());
        CHECK(foundNeighbors == checkNeigbbors);
    };

    testStationNeigbors("A1", QStringList() << "A2" << "A3");
    testStationNeigbors("A2", QStringList() << "A1" << "A3" << "A4" << "A5");
    testStationNeigbors("A3", QStringList() << "A1" << "A2");
    testStationNeigbors("A4", QStringList() << "A2");
    testStationNeigbors("A5", QStringList() << "A2");

    SECTION("Add invalid shot") {
        network.addShot("", "a5");
        network.addShot("a1", "");

        testStationNeigbors("a1", QStringList() << "A2" << "A3");
        testStationNeigbors("a5", QStringList() << "A2");
        testStationNeigbors("", QStringList());
    }

    SECTION("Clear the network") {
        network.clear();

        CHECK(network.neighbors("A1") == QStringList());
        CHECK(network.neighbors("A2") == QStringList());
        CHECK(network.neighbors("a3") == QStringList());
        CHECK(network.neighbors("a4") == QStringList());
        CHECK(network.neighbors("a5") == QStringList());
    }


}

