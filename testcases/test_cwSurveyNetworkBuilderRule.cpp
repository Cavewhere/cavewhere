// test_cwSurveyNetworkBuilderRule.cpp

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Qt includes
#include <QtTest/QSignalSpy>

//Our includes
#include "LoadProjectHelper.h"
#include "cwSurveyNetworkBuilderRule.h"
#include "cwSurveyDataArtifact.h"
#include "asyncfuture.h"

TEST_CASE("cwSurveyNetworkBuilderRule reports correct source/output names", "[SurveyNetworkBuilderRule]") {
    cwSurveyNetworkBuilderRule rule;
    auto sources = rule.sourcesNames();
    auto outputs = rule.outputsNames();

    REQUIRE(sources.size() == 1);
    CHECK(sources.at(0) == QByteArrayLiteral("surveyData"));

    REQUIRE(outputs.size() == 1);
    CHECK(outputs.at(0) == QByteArrayLiteral("surveyNetworkArtifact"));
}

TEST_CASE("setSurveyData emits surveyDataChanged only on actual change", "[SurveyNetworkBuilderRule]") {
    cwSurveyNetworkBuilderRule rule;
    cwSurveyDataArtifact data1;
    cwSurveyDataArtifact data2;

    QSignalSpy spy(&rule, &cwSurveyNetworkBuilderRule::surveyDataChanged);
    REQUIRE(spy.isValid());  // ensure spy hooked up

    // First time setting to &data1 → should emit once
    rule.setSurveyData(&data1);
    CHECK(spy.count() == 1);

    // Setting again to same pointer → no new signal
    rule.setSurveyData(&data1);
    CHECK(spy.count() == 1);

    // Changing to a different pointer → should emit again
    rule.setSurveyData(&data2);
    CHECK(spy.count() == 2);

    // Setting back to nullptr → should emit once more
    rule.setSurveyData(nullptr);
    CHECK(spy.count() == 3);
}

TEST_CASE("surveyNetworkArtifact is initialized by constructor", "[SurveyNetworkBuilderRule]") {
    cwSurveyNetworkBuilderRule rule;
    // We rely on the fact that the constructor creates m_surveyNetworkArtifact
    // and that the generated Q_PROPERTY getter exists.
    auto artifact = rule.property("surveyNetworkArtifact").value<cwSurveyNetworkArtifact*>();
    REQUIRE(artifact != nullptr);
}

// Helper to compare QVector3D with a small tolerance
static void CHECK_VEC3(const QVector3D &v, double x, double y, double z) {
    CHECK(v.x() == Catch::Approx(x).margin(1e-2));
    CHECK(v.y() == Catch::Approx(y).margin(1e-2));
    CHECK(v.z() == Catch::Approx(z).margin(1e-2));
}

TEST_CASE("cwSurvexExportRule should build the network correctly and position stations", "[SurveyNetworkBuilderRule]") {
    // Load project and get the caving region
    auto project = fileToProject("://datasets/test_cwProject/Phake Cave 3000.cw");
    auto cavingRegion = project->cavingRegion();

    // Create a survey data artifact and set the region
    cwSurveyDataArtifact surveyData;
    surveyData.setRegion(cavingRegion);

    cwSurveyNetworkBuilderRule rule;
    rule.setSurveyData(&surveyData);

    auto future = rule.surveyNetworkArtifact()->surveyNetwork();
    CHECK(AsyncFuture::waitForFinished(future, 2000));

    REQUIRE(future.result().hasError() == false);
    cwSurveyNetwork network = future.result().value();

    //Here's the survex dump from Phake Cave 3000.cw
    // *data normal from to tape compass clino
    //     ;From       To          Distance    Compass     Clino
    //     a1          a2          14.52       49          -3
    //     a2          a3          9.13        352         -17.3
    //     a3          a4          11.05       82          -4.5
    //     a4          a5          6.97        41          11.2
    //     a4          a6          16.54       147.5       -7.5
    //     a6          a7          9.81        43          26
    //     a7          a8          10.23       74          17.4
    //     a6          a9          23.44       -           DOWN
    //     a9          a10         5.85        230         -8.4
    //     a10         a11         10.56       172         -21.7
    //     a11         a12         9.17        200         -5.3
    //     a10         a13         18.28       116         -4.1
    //     a13         a14         6.99        91          -1.2

    // NODE 48.39 -5.99 -32.25 [phakecave3000.a14] UNDERGROUND
    // NODE 41.40 -5.87 -32.10 [phakecave3000.a13] UNDERGROUND
    // NODE 23.26 -16.17 -35.55 [phakecave3000.a12] UNDERGROUND
    // NODE 26.38 -7.59 -34.70 [phakecave3000.a11] UNDERGROUND
    // NODE 25.02 2.13 -30.80 [phakecave3000.a10] UNDERGROUND
    // NODE 29.45 5.85 -29.94 [phakecave3000.a9] UNDERGROUND
    // NODE 44.85 14.99 0.86 [phakecave3000.a8] UNDERGROUND
    // NODE 35.46 12.30 -2.20 [phakecave3000.a7] UNDERGROUND
    // NODE 29.45 5.85 -6.50 [phakecave3000.a6] UNDERGROUND
    // NODE 25.12 24.84 -2.99 [phakecave3000.a5] UNDERGROUND
    // NODE 20.64 19.68 -4.34 [phakecave3000.a4] UNDERGROUND
    // NODE 9.73 18.15 -3.47 [phakecave3000.a3] UNDERGROUND
    // NODE 10.94 9.51 -0.76 [phakecave3000.a2] UNDERGROUND
    // NODE 0.00 0.00 0.00 [phakecave3000.a1] UNDERGROUND FIXED

    // Utility to build the station key
    auto key = [&](int i){
        return QStringLiteral("phake cave 3000.a%1").arg(i);
    };

    // --- 1) Check positions against Survex dump ---
    CHECK_VEC3(network.position(key(1)),  0.00,  0.00,   0.00);
    CHECK_VEC3(network.position(key(2)), 10.94,  9.51,  -0.76);
    // CHECK_VEC3(network.position(key(3)),  9.73, 18.15,  -3.47);
    // CHECK_VEC3(network.position(key(4)), 20.64, 19.68,  -4.34);
    // CHECK_VEC3(network.position(key(5)), 25.12, 24.84,  -2.99);
    // CHECK_VEC3(network.position(key(6)), 29.45,  5.85,  -6.50);
    // CHECK_VEC3(network.position(key(7)), 35.46, 12.30,  -2.20);
    // CHECK_VEC3(network.position(key(8)), 44.85, 14.99,   0.86);
    // CHECK_VEC3(network.position(key(9)), 29.45,  5.85, -29.94);
    // CHECK_VEC3(network.position(key(10)),25.02,  2.13, -30.80);
    // CHECK_VEC3(network.position(key(11)),26.38, -7.59, -34.70);
    // CHECK_VEC3(network.position(key(12)),23.26,-16.17, -35.55);
    // CHECK_VEC3(network.position(key(13)),41.40, -5.87, -32.10);
    // CHECK_VEC3(network.position(key(14)),48.39, -5.99, -32.25);

    // --- 2) Check neighbor‐lists from the “from→to” table ---
    auto checkNeighbors = [&](int station, std::initializer_list<int> expectedNeighbors) {
        auto neighbors = network.neighbors(key(station));
        REQUIRE(neighbors.size() == expectedNeighbors.size());
        for (int e : expectedNeighbors) {
            INFO("Station:" << key(station).toStdString() << " Contains:" << key(e).toStdString() << " " << neighbors.contains(key(e)));
            CHECK(neighbors.contains(key(e)));
        }
    };

    checkNeighbors(1,  {2});
    checkNeighbors(2,  {1,3});
    checkNeighbors(3,  {2,4});
    checkNeighbors(4,  {3,5,6});
    checkNeighbors(5,  {4});
    checkNeighbors(6,  {4,7,9});
    checkNeighbors(7,  {6,8});
    checkNeighbors(8,  {7});
    checkNeighbors(9,  {6,10});
    checkNeighbors(10, {9,11,13});
    checkNeighbors(11, {10,12});
    checkNeighbors(12, {11});
    checkNeighbors(13, {10,14});
    checkNeighbors(14, {13});



}
