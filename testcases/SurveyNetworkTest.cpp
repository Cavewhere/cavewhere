/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include "catch.hpp"

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

    QStringList a1Neigbors;
    a1Neigbors << "A2" << "A3";

    CHECK(network.neighbors("A1") == a1Neigbors);

    QStringList a2Neigbors;
    a2Neigbors << "A1" << "A3" << "A4" << "A5";

    CHECK(network.neighbors("a2") == a2Neigbors);

    QStringList a3Neigbors;
    a3Neigbors << "A2" << "A1";

    CHECK(network.neighbors("a3") == a3Neigbors);

    QStringList a4Neigbors;
    a4Neigbors << "A2";

    CHECK(network.neighbors("a4") == a4Neigbors);

    QStringList a5Neigbors;
    a5Neigbors << "A2";

    CHECK(network.neighbors("a5") == a5Neigbors);

    SECTION("Clear the network") {
        network.clear();

        CHECK(network.neighbors("A1") == QStringList());
        CHECK(network.neighbors("A2") == QStringList());
        CHECK(network.neighbors("a3") == QStringList());
        CHECK(network.neighbors("a4") == QStringList());
        CHECK(network.neighbors("a5") == QStringList());
    }
}

