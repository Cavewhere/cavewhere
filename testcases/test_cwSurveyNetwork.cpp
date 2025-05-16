//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwSurveyNetwork.h"

//Qt include
#include <QStringList>
#include <QSet>


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

    testStationNeigbors("A1", QStringList() << "a2" << "a3");
    testStationNeigbors("A2", QStringList() << "a1" << "a3" << "a4" << "a5");
    testStationNeigbors("A3", QStringList() << "a1" << "a2");
    testStationNeigbors("A4", QStringList() << "a2");
    testStationNeigbors("A5", QStringList() << "a2");

    SECTION("Add invalid shot") {
        network.addShot("", "a5");
        network.addShot("a1", "");

        testStationNeigbors("a1", QStringList() << "a2" << "a3");
        testStationNeigbors("a5", QStringList() << "a2");
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


TEST_CASE("cwSurveyNetwork should compare changedStation correctly", "[cwSurveyNetwork]") {
    cwSurveyNetwork n1;
    n1.addShot("a1", "a2");
    n1.addShot("a1", "a3");
    n1.addShot("a2", "a4");
    n1.addShot("a4", "a3");

    cwSurveyNetwork n2 = n1;

    CHECK(cwSurveyNetwork::changedStations(n1, n2).isEmpty() == true);

    //Need to detach to do a direct lookup
    n2.clear();
    n2.addShot("a1", "a2");
    n2.addShot("a1", "a3");
    n2.addShot("a2", "a4");
    n2.addShot("a4", "a3");

    CHECK(cwSurveyNetwork::changedStations(n1, n2).isEmpty() == true);

    //Add a new shot
    n2.addShot("a1", "a5");

    CHECK(cwSurveyNetwork::changedStations(n1, n2) == QStringList({"a5", "a1"}));
    CHECK(cwSurveyNetwork::changedStations(n2, n1) == QStringList({"a5", "a1"}));

    n2.addShot("a2", "a3");

    QStringList testList({"a5", "a1", "a2", "a3"});
    QSet<QString> testSet(testList.begin(), testList.end());

    {
        auto changedStations = cwSurveyNetwork::changedStations(n1, n2);
        QSet<QString> networkSet(changedStations.begin(), changedStations.end());

        CHECK(networkSet == testSet);
    }

    {
        auto changedStations = cwSurveyNetwork::changedStations(n2, n1);
        QSet<QString> networkSet(changedStations.begin(), changedStations.end());

        CHECK(networkSet == testSet);
    }
}

TEST_CASE("cwSurveyNetwork position accessors", "[cwSurveyNetwork]") {
    cwSurveyNetwork net;

    // No positions have been set yet
    CHECK(net.hasPosition("any") == false);
    CHECK(net.position("any") == QVector3D());

    // Setting a position for station "A1"
    QVector3D expectedPos(1.0f, 2.0f, 3.0f);
    net.setPosition("a1", expectedPos);

    // hasPosition should be true (case‐insensitive)
    CHECK(net.hasPosition("A1") == true);
    CHECK(net.hasPosition("a1") == true);

    // position() should return the exact QVector3D we set
    CHECK(net.position("A1") == expectedPos);
    CHECK(net.position("a1") == expectedPos);

    // Setting another station without prior addShot()
    net.setPosition("b2", QVector3D(-4.5f, 0.0f, 7.2f));
    CHECK(net.hasPosition("B2") == true);
    CHECK(net.position("B2") == QVector3D(-4.5f, 0.0f, 7.2f));

    // Clearing the network should remove all positions
    net.clear();
    CHECK(net.hasPosition("A1") == false);
    CHECK(net.position("A1") == QVector3D());
    CHECK(net.hasPosition("B2") == false);
    CHECK(net.position("B2") == QVector3D());
}
