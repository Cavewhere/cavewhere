//catch includes
#include "catch.hpp"

//Our includes
#include "cwRegionLoadTask.h"
#include "cwRegionSaveTask.h"
#include "cwRootData.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwSurveyNetwork.h"

//std includes
#include <memory>

//Qt includes
#include <QUuid>

TEST_CASE("Save / Load should work with cwSurveyNetwork", "[ProtoSaveLoad]") {

    auto root = std::make_unique<cwRootData>();

    cwCave* cave = new cwCave();
    cwTrip* trip = new cwTrip();

    cave->addTrip(trip);
    root->region()->addCave(cave);

    cwSurveyChunk* chunk = new cwSurveyChunk;

    cwStation a1("a1");
    cwStation a2("a2");
    cwStation a3("a3");

    cwShot a1_to_a2("100", "0.0", "0.0", "0.0", "0.0");
    cwShot a2_to_a3("50", "0.0", "0.0", "0.0", "0.0");

    chunk->appendShot(a1, a2, a1_to_a2);
    chunk->appendShot(a2, a3, a2_to_a3);

    trip->addChunk(chunk);

    root->linePlotManager()->waitToFinish();

    CHECK(root->project()->isTemporaryProject() == true);

    auto filename = QString("test_cwSurveyNetwork-") + QUuid::createUuid().toString().remove(QRegularExpression("{|}|-")) + ".cw";
    root->project()->saveAs(filename);
    root->project()->waitSaveToFinish();

    root->project()->newProject();
    root->project()->waitSaveToFinish();

    CHECK(root->region()->caveCount() == 0);

    root->project()->loadFile(filename);
    root->project()->waitLoadToFinish();
    REQUIRE(root->region()->caveCount() == 1);

    auto loadCave = root->region()->cave(0);
    auto network = loadCave->network();

    auto testStationNeigbors = [=](QString stationName, QStringList neighbors) {
        auto foundNeighbors = network.neighbors(stationName).toSet();
        auto checkNeigbbors = neighbors.toSet();
        CHECK(foundNeighbors == checkNeigbbors);
    };

    testStationNeigbors("a1", {"A2"});
    testStationNeigbors("a2", {"A1", "A3"});
    testStationNeigbors("a3", {"A2"});
}
