//Catch includes
#include "catch.hpp"

//Our includes
#include "cwCavingRegion.h"
#include "cwRootData.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTaskManagerModel.h"
#include "cwFutureManagerModel.h"
#include "cwLinePlotManager.h"
#include "cwFixedStationModel.h"

//Qt includes
#include <QSignalSpy>

TEST_CASE("cwCavingRegion should handle fixed stations correctly", "[cwCavingRegion]") {
    cwRootData rootData;
    auto region = rootData.region();

    QSignalSpy originSpy(region, &cwCavingRegion::originChanged);

    cwCave* cave = new cwCave();
    cave->setName("cave1");
    region->addCave(cave);

    CHECK(originSpy.size() == 1);

    cwTrip* trip = new cwTrip();
    cave->addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk;

    cwStation a1("a1");
    cwStation a2("a2");

    cwShot a1_to_a2("100", "0.0", "180.0", "0.0", "0.0");

    chunk->appendShot(a1, a2, a1_to_a2);
    trip->addChunk(chunk);

    rootData.linePlotManager()->waitToFinish();
    rootData.taskManagerModel()->waitForTasks();
    rootData.futureManagerModel()->waitForFinished();

    CHECK(cave->stationPositionLookup().hasPosition("a1"));
    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));

    CHECK(cave->stationPositionLookup().hasPosition("a2"));
    CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 100.0, 0.0));

    SECTION("Set a fixed station") {
        cwFixedStation s1;
        s1.setLatitude("49.7001521");
        s1.setLongitude("-123.9309973");
        s1.setAltitude("1000");
        s1.setStationName("a1");
        cave->fixedStations()->append(s1);

        CHECK(originSpy.size() == 2);
        CHECK(region->origin() == s1);

        rootData.linePlotManager()->waitToFinish();
        rootData.taskManagerModel()->waitForTasks();
        rootData.futureManagerModel()->waitForFinished();

        CHECK(cave->stationPositionLookup().hasPosition("a1"));
        CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));

        CHECK(cave->stationPositionLookup().hasPosition("a2"));
        CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 100.0, 0.0));

        SECTION("Add second cave with fixed station") {
            cwCave* cave2 = new cwCave();
            cave2->setName("cave2");
            region->addCave(cave2);

            cwTrip* trip = new cwTrip();
            cave2->addTrip(trip);

            cwSurveyChunk* chunk = new cwSurveyChunk;

            cwStation x1("x1");
            cwStation x2("x2");

            cwShot x1_to_x2("150", "0.0", "180.0", "0.0", "0.0");

            chunk->appendShot(x1, x2, x1_to_x2);
            trip->addChunk(chunk);

            rootData.linePlotManager()->waitToFinish();
            rootData.taskManagerModel()->waitForTasks();
            rootData.futureManagerModel()->waitForFinished();

            //About 200m east of s1
            cwFixedStation s2;
            s2.setLatitude("49.700169");
            s2.setLongitude("-123.928201");
            s2.setAltitude("1300");
            s2.setStationName("x1");
            cave2->fixedStations()->append(s2);

            rootData.linePlotManager()->waitToFinish();
            rootData.taskManagerModel()->waitForTasks();
            rootData.futureManagerModel()->waitForFinished();

            CHECK(cave->stationPositionLookup().hasPosition("a1"));
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));

            CHECK(cave->stationPositionLookup().hasPosition("a2"));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 100.0, 0.0));

            CHECK(cave2->stationPositionLookup().hasPosition("x1"));
            CHECK(cave2->stationPositionLookup().position("x1") == QVector3D(201.7, 1.88, 300.0));

            CHECK(cave2->stationPositionLookup().hasPosition("x2"));
            CHECK(cave2->stationPositionLookup().position("x2") == QVector3D(201.7, 151.88, 300.0));

            CHECK(originSpy.size() == 2);
            CHECK(region->origin() == s1);

            SECTION("Remove first cave") {
                region->removeCave(0);

                CHECK(originSpy.size() == 3);
                CHECK(region->origin() == s2);

                SECTION("Update fixed station") {
                    auto firstStationIndex = cave2->fixedStations()->index(0, cave2->fixedStations()->column("Latitude"));
                    CHECK(cave2->fixedStations()->setData(firstStationIndex, "30.0", Qt::EditRole));

                    CHECK(originSpy.size() == 4);
                    s2.setLatitude("30.0");
                    CHECK(region->origin() == s2);

                    firstStationIndex = cave2->fixedStations()->index(0, cave2->fixedStations()->column("Longitude"));
                    CHECK(cave2->fixedStations()->setData(firstStationIndex, "31.0", Qt::EditRole));

                    CHECK(originSpy.size() == 5);
                    s2.setLongitude("31.0");
                    CHECK(region->origin() == s2);

                    firstStationIndex = cave2->fixedStations()->index(0, cave2->fixedStations()->column("Altitude"));
                    CHECK(cave2->fixedStations()->setData(firstStationIndex, "11.0", Qt::EditRole));

                    CHECK(originSpy.size() == 6);
                    s2.setAltitude("11.0");
                    CHECK(region->origin() == s2);

                    firstStationIndex = cave2->fixedStations()->index(0, cave2->fixedStations()->column("Station"));
                    CHECK(cave2->fixedStations()->setData(firstStationIndex, "x2", Qt::EditRole));

                    CHECK(originSpy.size() == 7);
                    s2.setStationName("x2");
                    CHECK(region->origin() == s2);
                }
            }
        }
    }
}
