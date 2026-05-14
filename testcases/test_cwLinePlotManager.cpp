/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include <catch2/catch_test_macros.hpp>
#include "LoadProjectHelper.h"
#include <catch2/catch_approx.hpp>

//Cavewhere includes
#include "cwLinePlotManager.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwLength.h"
#include "cwTripCalibration.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwProject.h"
#include "cwFixStationModel.h"
#include "cwFixStation.h"

//Our includes
#include "TestHelper.h"
#include "SpyChecker.h"

//Qt includes
#include <QThread>
#include <QApplication>
#include "cwSignalSpy.h"

TEST_CASE("Survey network are returned", "[LinePlotManager]") {
    auto project = fileToProject(testcasesDatasetPath("network.cw"));

    REQUIRE(project->cavingRegion()->caveCount() == 1);

    cwCave* cave = project->cavingRegion()->cave(0);
    cave->setStationPositionLookup(cwStationPositionLookup());

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(project->cavingRegion());
    plotManager->waitToFinish();

    cwSurveyNetwork network = cave->network();

    auto testStationNeigbors = [=](QString stationName, QStringList neighbors) {
        auto neighborsAtStation = network.neighbors(stationName);
        auto foundNeighbors = QSet<QString>(neighborsAtStation.begin(), neighborsAtStation.end());
        auto checkNeigbbors = QSet<QString>(neighbors.begin(), neighbors.end());
        CHECK(foundNeighbors == checkNeigbbors);
    };

    testStationNeigbors("a1", QStringList() << "a2" << "a4");
    testStationNeigbors("a2", QStringList() << "a1" << "a3");
    testStationNeigbors("a3", QStringList() << "a2" << "a4" << "a5");
    testStationNeigbors("a4", QStringList() << "a1" << "a3");
    testStationNeigbors("a5", QStringList() << "a3" << "a6");
    testStationNeigbors("a6", QStringList() << "a5");
}

TEST_CASE("Changing data adding and removing caves trips survey chunks should run plotting", "[LinePlotManager]")
{
    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName("Cave 1");
    region.addCave(cave);

    cwTrip* trip = new cwTrip();
    trip->setName("Trip 1");
    cave->addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);

    cwStation s1("a1");
    cwStation s2("a2");
    cwShot shot1;
    shot1.setDistance(cwDistanceReading("10.0"));
    shot1.setCompass(cwCompassReading("0.0"));
    shot1.setClino(cwClinoReading("0.0"));

    chunk->appendShot(s1, s2, shot1);

    auto plotManager = std::make_unique<cwLinePlotManager>();

    SECTION("Setting the region should run line plot generation") {
        plotManager->setRegion(&region);

        plotManager->waitToFinish();

        CHECK(cave->length()->value() == 10.0);
        CHECK(cave->depth()->value() == 0.0);
        CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
        CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));

        CHECK(cave->errorModel()->fatalCount() == 0);


        SECTION("Setting station name data should re-run line plot") {
            chunk->setData(cwSurveyChunk::StationNameRole, 1, "b2");

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == 10.0);
            CHECK(cave->depth()->value() == 0.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 0.0, 0.0)); //a2 should no-longer exists
            CHECK(cave->stationPositionLookup().position("b2") == QVector3D(0.0, 10.0, 0.0));
        }

        SECTION("Setting shot distance data should re-run line plot") {
            chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, 20.0);

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == 20.0);
            CHECK(cave->depth()->value() == 0.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 20.0, 0.0));
        }

        SECTION("Setting compass data should re-run line plot") {
            chunk->setData(cwSurveyChunk::ShotCompassRole, 0, 90.0);

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == 10.0);
            CHECK(cave->depth()->value() == 0.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(10.0, 0.0, 0.0));
        }

        SECTION("Setting back compass data should re-run line plot") {
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, 90.0);

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == Catch::Approx(9.9984903336));
            CHECK(cave->depth()->value() == 0.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(-7.07, 7.07, 0.0));
        }

        SECTION("Setting clino data should re-run line plot") {
            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "Up");

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == Catch::Approx(10.0));
            CHECK(cave->depth()->value() == 10.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 0.0, 10.0));
        }

        SECTION("Setting clino data should re-run line plot") {
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "45.0");

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == Catch::Approx(10.0).epsilon(0.01));
            CHECK(cave->depth()->value() == Catch::Approx(3.8299).epsilon(0.01));
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 9.24, -3.83));
        }

        SECTION("Setting exclude length should re-run line plot") {
            chunk->setData(cwSurveyChunk::ShotDistanceIncludedRole, 0, false);

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == Catch::Approx(0.0).epsilon(0.01));
            CHECK(cave->depth()->value() == Catch::Approx(0.0).epsilon(0.01));
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));
        }

        SECTION("Adding a shot re-run line plot") {
            int total = 30; //Change this value to 1 to debug...

            //There's a loop here to test for race conditions in cwTask when the task
            //is restarted
            for(int i = 0; i < total; i++) {
                cwShot shot;
                shot.setDistance(cwDistanceReading(20));
                shot.setCompass(cwCompassReading("90.0"));
                shot.setClino(cwClinoReading("0.0"));
                cwStation station(QString("a3-%1").arg(i));

                const auto stations = chunk->stations();
                chunk->appendShot(stations.last(), station, shot);

                plotManager->waitToFinish();

                INFO("i:" << i);

                CHECK(cave->length()->value() == Catch::Approx(10.0 + 20.0 * (i + 1)).epsilon(0.01));
                CHECK(cave->depth()->value() == Catch::Approx(0.0).epsilon(0.01));
                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));

                for(int ii = 0; ii <= i; ii++) {
                    CHECK(cave->stationPositionLookup().position(QString("a3-%1").arg(ii)) == QVector3D(20.0 * (ii+1), 10.0, 0.0));
                }
            }

            SECTION("Removes a shot re-run line plot") {
                chunk->removeStation(chunk->stationCount() - 1, cwSurveyChunk::Above);

                plotManager->waitToFinish();

                int i = total - 2;

                CHECK(cave->length()->value() == Catch::Approx(10.0 + 20.0 * (i + 1)).epsilon(0.01));
                CHECK(cave->depth()->value() == Catch::Approx(0.0).epsilon(0.01));

                CHECK(cave->stationPositionLookup().hasPosition(QString("a3-%1").arg(i)) == true);

                for(int ii = 0; ii < total - 1; ii++) {
                    CHECK(cave->stationPositionLookup().hasPosition(QString("a3-%1").arg(ii)) == true);
                    CHECK(cave->stationPositionLookup().position(QString("a3-%1").arg(ii)) == QVector3D(20.0 * (ii+1), 10.0, 0.0));
                }
            }
        }

        SECTION("Adding a trip re-run line plot") {
            cwTrip* trip2 = new cwTrip();

            cwSurveyChunk* chunk2 = new cwSurveyChunk();
            trip2->addChunk(chunk2);

            cwStation s2a("a2");
            cwStation s3("a3");
            cwShot shot1;
            shot1.setDistance(cwDistanceReading("15.0"));
            shot1.setCompass(cwCompassReading("0.0"));
            shot1.setClino(cwClinoReading("0.0"));

            chunk2->appendShot(s2a, s3, shot1);

            cave->addTrip(trip2);

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == 25.0);
            CHECK(cave->depth()->value() == 0.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a3") == QVector3D(0.0, 25.0, 0.0));

            SECTION("Remove a trip re-run line plot") {
                cave->removeTrip(1);

                plotManager->waitToFinish();

                CHECK(cave->length()->value() == 10.0);
                CHECK(cave->depth()->value() == 0.0);
                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));
            }
        }

        SECTION("Trip name change re-runs line plot") {
            trip->setName("trip sauce");

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == 10.0);
            CHECK(cave->depth()->value() == 0.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));
        }

        SECTION("Trip calibration changed re-runs line plot") {
            trip->calibrations()->setDeclinationManual(45.0);

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == Catch::Approx(10.0).epsilon(0.01));
            CHECK(cave->depth()->value() == 0.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(7.07, 7.07, 0.0));
        }

        SECTION("Adding Cave re-runs line plot") {
            cwCave* cave2 = new cwCave();
            cave2->setName("Cave 2");
            region.addCave(cave2);

            cwTrip* trip = new cwTrip();
            trip->setName("Trip 1");
            cave2->addTrip(trip);

            cwSurveyChunk* chunk = new cwSurveyChunk();
            trip->addChunk(chunk);

            cwStation s1("a1");
            cwStation s2("a2");
            cwShot shot1;
            shot1.setDistance(cwDistanceReading("10.0"));
            shot1.setCompass(cwCompassReading("0.0"));
            shot1.setClino(cwClinoReading("0.0"));

            chunk->appendShot(s1, s2, shot1);

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == 10.0);
            CHECK(cave->depth()->value() == 0.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));

            CHECK(cave2->length()->value() == 10.0);
            CHECK(cave2->depth()->value() == 0.0);
            CHECK(cave2->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave2->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));

            SECTION("Removing Cave re-runs line plot") {
                region.removeCave(1);

                plotManager->waitToFinish();

                CHECK(cave->length()->value() == 10.0);
                CHECK(cave->depth()->value() == 0.0);
                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));
            }
        }

        SECTION("Frontsite and backsite down/up work") {
            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "DOWN");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "UP");

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == 10.0);
            CHECK(cave->depth()->value() == 10.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 0.0, -10.0));

            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "UP");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "DOWN");

            plotManager->waitToFinish();

            CHECK(cave->length()->value() == 10.0);
            CHECK(cave->depth()->value() == 10.0);
            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 0.0, 10.0));

            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "0");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "0");

            plotManager->waitToFinish();

            trip->calibrations()->setCorrectedClinoBacksight(true);
            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "UP");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "UP");

            plotManager->waitToFinish();

            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 0.0, 10.0));

            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "DOWN");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "DOWN");

            plotManager->waitToFinish();

            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 0.0, -10.0));
        }

        SECTION("Trip calibration should cause the cave to re-run") {
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "182");
            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "-1");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "2");

            plotManager->waitToFinish();

            QVector3D originalA2 = QVector3D(0.17, 10.0, -0.26);

            CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
            CHECK(cave->stationPositionLookup().position("a2") == originalA2);

            SECTION("Turn off backsights") {

                trip->calibrations()->setBackSights(false);

                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, -0.17));

                SECTION("Turn the back on") {
                    trip->calibrations()->setBackSights(true);

                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("Turn off front sights") {
                trip->calibrations()->setFrontSights(false);

                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.35, 9.99, -0.35));

                SECTION("Turn the back on") {
                    trip->calibrations()->setFrontSights(true);

                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("Turn on correctedCompassBacksight") {
                trip->calibrations()->setFrontSights(false);
                plotManager->waitToFinish();

                trip->calibrations()->setCorrectedCompassBacksight(true);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(-0.35, -9.99, -0.35));

                SECTION("Turn it off") {
                    trip->calibrations()->setFrontSights(true);
                    plotManager->waitToFinish();

                    trip->calibrations()->setCorrectedCompassBacksight(false);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("Turn on correctedClinoBacksight") {
                trip->calibrations()->setFrontSights(false);
                plotManager->waitToFinish();

                trip->calibrations()->setCorrectedClinoBacksight(true);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.35, 9.99, 0.35));

                SECTION("Turn it off") {
                    trip->calibrations()->setFrontSights(true);
                    plotManager->waitToFinish();

                    trip->calibrations()->setCorrectedClinoBacksight(false);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("Turn on correctedCompassFrontsight") {
                trip->calibrations()->setBackSights(false);
                plotManager->waitToFinish();

                trip->calibrations()->setCorrectedCompassFrontsight(true);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, -10.0, -0.17));

                SECTION("Turn it off") {
                    trip->calibrations()->setBackSights(true);
                    plotManager->waitToFinish();

                    trip->calibrations()->setCorrectedCompassFrontsight(false);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("Turn on correctedClinoFrontsight") {
                trip->calibrations()->setBackSights(false);
                plotManager->waitToFinish();

                trip->calibrations()->setCorrectedClinoFrontsight(true);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.17));

                SECTION("Turn it off") {
                    trip->calibrations()->setBackSights(true);
                    plotManager->waitToFinish();

                    trip->calibrations()->setCorrectedClinoFrontsight(false);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("setTapeCalibration") {
                trip->calibrations()->setTapeCalibration(1.0);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.19, 10.99, -0.29));

                SECTION("Turn it off") {
                    trip->calibrations()->setTapeCalibration(0.0);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("setFrontCompassCalibration") {
                trip->calibrations()->setFrontCompassCalibration(-2.0);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, -0.26));

                SECTION("Turn it off") {
                    trip->calibrations()->setFrontCompassCalibration(0.0);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("setFrontClinoCalibration") {
                trip->calibrations()->setFrontClinoCalibration(3.0);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.17, 10.0, 0.0));

                SECTION("Turn it off") {
                    trip->calibrations()->setFrontClinoCalibration(0.0);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("setBackCompassCalibration") {
                trip->calibrations()->setBackCompassCalibration(-2.0);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, -0.26));

                SECTION("Turn it off") {
                    trip->calibrations()->setBackCompassCalibration(0.0);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("Turn on setBackClinoCalibration") {
                trip->calibrations()->setBackClinoCalibration(-3.0);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.17, 10.0, 0.0));

                SECTION("Turn it off") {
                    trip->calibrations()->setBackClinoCalibration(0.0);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("Turn on setDeclination") {
                trip->calibrations()->setDeclinationManual(180.0);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(-0.17, -10.0, -0.26));

                SECTION("Turn it off") {
                    trip->calibrations()->setDeclinationManual(0.0);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }

            SECTION("Turn on setDistance units") {
                trip->calibrations()->setDistanceUnit(cwUnits::Feet);
                plotManager->waitToFinish();

                CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.05, 3.05, -0.08));

                SECTION("Turn it off") {
                    trip->calibrations()->setDistanceUnit(cwUnits::Meters);
                    plotManager->waitToFinish();

                    CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
                    CHECK(cave->stationPositionLookup().position("a2") == originalA2);
                }
            }
        }

        SECTION("Chunk calibration changes should re-run cwLinePlotManager") {

            cwSurveyChunk* chunk = new cwSurveyChunk();

            chunk->appendNewShot();
            chunk->setData(cwSurveyChunk::StationNameRole, 0, "a1");
            chunk->setData(cwSurveyChunk::ShotCompassRole, 0, "0");
            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "0");
            chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "10");
            chunk->setData(cwSurveyChunk::StationNameRole, 1, "b2");
            chunk->appendNewShot();
            chunk->setData(cwSurveyChunk::ShotCompassRole, 1, "90");
            chunk->setData(cwSurveyChunk::ShotClinoRole, 1, "0");
            chunk->setData(cwSurveyChunk::ShotDistanceRole, 1, "5");
            chunk->setData(cwSurveyChunk::StationNameRole, 2, "b3");
            chunk->appendNewShot();
            chunk->setData(cwSurveyChunk::ShotCompassRole, 2, "180");
            chunk->setData(cwSurveyChunk::ShotClinoRole, 2, "0");
            chunk->setData(cwSurveyChunk::ShotDistanceRole, 2, "15");
            chunk->setData(cwSurveyChunk::StationNameRole, 3, "b4");
            chunk->appendNewShot();
            chunk->setData(cwSurveyChunk::ShotCompassRole, 3, "270");
            chunk->setData(cwSurveyChunk::ShotClinoRole, 3, "0");
            chunk->setData(cwSurveyChunk::ShotDistanceRole, 3, "20");
            chunk->setData(cwSurveyChunk::StationNameRole, 4, "b5");
            trip->addChunk(chunk);

            plotManager->waitToFinish();

            CHECK(cave->stationPositionLookup().position("b2") == QVector3D(0.0, 10.0, 0.0));
            CHECK(cave->stationPositionLookup().position("b3") == QVector3D(5.0, 10.0, 0.0));
            CHECK(cave->stationPositionLookup().position("b4") == QVector3D(5.0, -5.0, 0.0));
            CHECK(cave->stationPositionLookup().position("b5") == QVector3D(-15.0, -5.0, 0.0));


        }
    }

    /*
     * If this section fails, the line plot manager isn't reporting unconnected chunks. An
     * unconnected chunk is a survey leg that isn't connected to the rest of the cave. This will prevent
     * survex from running correctly.
     */
    SECTION("LinePlotManager should report unconnected chunk errors") {
        cwSurveyChunk* chunk2 = new cwSurveyChunk();
        trip->addChunk(chunk2);

        plotManager->setRegion(&region);
        plotManager->waitToFinish();

        REQUIRE(chunk2->errorModel()->fatalCount() == 0);
        REQUIRE(cave->errorModel()->fatalCount() == 0);

        SECTION("Add on unconnect chunk") {
            //Not connected to a1 and a2
            cwStation a3("a3");
            cwStation a4("a4");
            cwShot shot2;
            shot2.setDistance(cwDistanceReading("20.0"));
            shot2.setCompass(cwCompassReading("40.0"));
            shot2.setClino(cwClinoReading("2.0"));

            chunk2->appendShot(a3, a4, shot2);

            plotManager->waitToFinish();

            REQUIRE(chunk2->errorModel()->fatalCount() == 1);
            CHECK(chunk2->errorModel()->errors()->first().message() == QString("Survey leg isn't connect to the cave"));
        }

        SECTION("Append new shot that's unconnected") {
            //Not connected to a1 and a2
            chunk2->appendNewShot();
            chunk2->setData(cwSurveyChunk::StationNameRole, 0, "a5");

            plotManager->waitToFinish();

            REQUIRE(chunk2->errorModel()->fatalCount() == 1);
            CHECK(chunk2->errorModel()->errors()->first().message() == QString("Survey leg isn't connect to the cave"));

            SECTION("Connect the chunk") {
                chunk2->setData(cwSurveyChunk::StationNameRole, 0, "a2");

                plotManager->waitToFinish();

                REQUIRE(chunk2->errorModel()->errors()->count() == 0);
            }
        }
    }
}

TEST_CASE("cwLinePlotManager automatic update should work", "[cwLinePlotManager]") {

    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName("Cave 1");
    region.addCave(cave);

    cwTrip* trip = new cwTrip();
    trip->setName("Trip 1");
    cave->addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);

    cwStation s1("a1");
    cwStation s2("a2");
    cwShot shot1;
    shot1.setDistance(cwDistanceReading("10.0"));
    shot1.setCompass(cwCompassReading("0.0"));
    shot1.setClino(cwClinoReading("0.0"));

    chunk->appendShot(s1, s2, shot1);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    cwSignalSpy autoUpdateSpy(plotManager.get(), &cwLinePlotManager::automaticUpdateChanged);
    autoUpdateSpy.setObjectName("autoUpdateSpy");

    cwSignalSpy stationPositionSpy(cave, &cwCave::stationPositionPositionChanged);
    stationPositionSpy.setObjectName("stationPositionSpy");

    SpyChecker spyChecker {
        {&autoUpdateSpy, 0},
        {&stationPositionSpy, 0}
    };

    CHECK(plotManager->automaticUpdate() == true);

    plotManager->setAutomaticUpdate(false);

    spyChecker[&autoUpdateSpy]++;

    CHECK(plotManager->automaticUpdate() == false);
    spyChecker.checkSpies();

    plotManager->setRegion(&region);
    plotManager->waitToFinish();

    spyChecker.checkSpies(); //StationPositionSpy should be zero

    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "11.0");
    plotManager->waitToFinish();

    spyChecker.checkSpies(); //StationPositionSpy should be zero

    plotManager->setAutomaticUpdate(true);
    plotManager->waitToFinish();
    spyChecker[&autoUpdateSpy]++;
    spyChecker[&stationPositionSpy]++;
    spyChecker.checkSpies();
    CHECK(plotManager->automaticUpdate() == true);

    chunk->setData(cwSurveyChunk::ShotDistanceRole, 0, "12.0");
    plotManager->waitToFinish();
    spyChecker[&stationPositionSpy]++;
    spyChecker.checkSpies();
}

TEST_CASE("cwLinePlotManager clears geometry when all caves are removed", "[LinePlotManager]")
{
    // Regression: bug #335 — File->New Project left the old centerline visible because
    // clearCaves() triggered runSurvex() which returned early (no shots) without
    // calling updateLinePlot to clear the render geometry.

    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName("Cave 1");
    region.addCave(cave);

    cwTrip* trip = new cwTrip();
    trip->setName("Trip 1");
    cave->addTrip(trip);

    cwSurveyChunk* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);

    cwStation s1("a1");
    cwStation s2("a2");
    cwShot shot;
    shot.setDistance(cwDistanceReading("10.0"));
    shot.setCompass(cwCompassReading("0.0"));
    shot.setClino(cwClinoReading("0.0"));
    chunk->appendShot(s1, s2, shot);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(&region);
    plotManager->waitToFinish();

    REQUIRE(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));

    cwSignalSpy stationPositionInCavesSpy(plotManager.get(), &cwLinePlotManager::stationPositionInCavesChanged);
    stationPositionInCavesSpy.setObjectName("stationPositionInCavesSpy");
    SpyChecker spyChecker{{&stationPositionInCavesSpy, 0}};

    // Simulate File->New Project: clear all caves
    region.clearCaves();
    plotManager->waitToFinish();

    // updateLinePlot must be called so the renderer clears its geometry
    spyChecker[&stationPositionInCavesSpy]++;
    spyChecker.checkSpies();
}

TEST_CASE("cwLinePlotManager skips cavern when cave or trip has no shots", "[LinePlotManager]")
{
    // Regression: previously cwLinePlotManager would invoke cavern even when the
    // region contained no shots, causing cavern to warn "Survey name omitted from END"
    // and produce no useful output.

    cwCavingRegion region;

    cwCave* cave = new cwCave();
    cave->setName("Cave 1");
    region.addCave(cave);

    auto plotManager = std::make_unique<cwLinePlotManager>();

    cwSignalSpy stationPositionSpy(cave, &cwCave::stationPositionPositionChanged);
    stationPositionSpy.setObjectName("stationPositionSpy");
    SpyChecker spyChecker {{&stationPositionSpy, 0}};

    SECTION("Cave with no trips does not trigger cavern") {
        plotManager->setRegion(&region);
        plotManager->waitToFinish();

        spyChecker.checkSpies();
        CHECK(cave->stationPositionLookup().positions().isEmpty());
    }

    SECTION("Cave with trip but no shots does not trigger cavern") {
        cwTrip* trip = new cwTrip();
        trip->setName("Trip 1");
        cave->addTrip(trip);

        cwSurveyChunk* chunk = new cwSurveyChunk();
        trip->addChunk(chunk);

        plotManager->setRegion(&region);
        plotManager->waitToFinish();

        spyChecker.checkSpies();
        CHECK(cave->stationPositionLookup().positions().isEmpty());

        // Adding a shot should now trigger cavern and produce positions
        cwStation s1("a1");
        cwStation s2("a2");
        cwShot shot;
        shot.setDistance(cwDistanceReading("10.0"));
        shot.setCompass(cwCompassReading("0.0"));
        shot.setClino(cwClinoReading("0.0"));
        chunk->appendShot(s1, s2, shot);

        plotManager->waitToFinish();

        spyChecker[&stationPositionSpy]++;
        spyChecker.checkSpies();
        CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
        CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));
    }
}

TEST_CASE("cwLinePlotManager re-runs cavern when globalCS or fix stations change",
          "[LinePlotManager][fix]")
{
    // The picker writes to cwCavingRegion::globalCS and the fix-station
    // editor mutates cave->fixStations(). Both feed the *cs out / *fix
    // lines in the survex export, so the manager must observe both and
    // re-run cavern when either changes — otherwise cavern keeps emitting
    // a stale .3d that doesn't carry the user's CS/fix edits.
    //
    // We verify the connection in two complementary ways:
    //   (a) a signal spy on stationPositionInCavesChanged fires once per
    //       publish — incremented after each provoked change confirms the
    //       run pipeline executed end-to-end;
    //   (b) for fix-station edits, resolved positions actually move to the
    //       new fix coords, proving cavern saw the new *fix line.

    cwCavingRegion region;
    auto* cave = new cwCave();
    cave->setName(QStringLiteral("Cave 1"));
    region.addCave(cave);

    auto* trip = new cwTrip();
    cave->addTrip(trip);
    // Test asserts on raw magnetic bearings — disable auto-IGRF.
    trip->calibrations()->setAutoDeclination(false);
    auto* chunk = new cwSurveyChunk();
    trip->addChunk(chunk);

    cwShot shot;
    shot.setDistance(cwDistanceReading(QStringLiteral("10.0")));
    shot.setCompass(cwCompassReading(QStringLiteral("0.0")));
    shot.setClino(cwClinoReading(QStringLiteral("0.0")));
    chunk->appendShot(cwStation(QStringLiteral("a1")), cwStation(QStringLiteral("a2")), shot);

    auto plotManager = std::make_unique<cwLinePlotManager>();
    plotManager->setRegion(&region);
    plotManager->waitToFinish();

    // Sanity: with no globalCS / fixes, a1 sits at the origin.
    REQUIRE(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));

    cwSignalSpy positionSpy(plotManager.get(), &cwLinePlotManager::stationPositionInCavesChanged);

    SECTION("region.setGlobalCS triggers a re-run") {
        REQUIRE(positionSpy.count() == 0);
        region.setGlobalCS(QStringLiteral("EPSG:32616"));
        plotManager->waitToFinish();
        CHECK(positionSpy.count() >= 1);
    }

    SECTION("appending a fix station triggers a re-run and moves resolved positions") {
        // Survex rejects *fix-with-*cs unless *cs out is also set, so the
        // realistic test scenario is "globalCS is set, then a fix is added".
        // Set globalCS first; the spy after that captures only fix-related
        // re-runs. We don't reset the spy between this and the parent
        // setRegion run, so we tally only what happens after this point.
        region.setGlobalCS(QStringLiteral("EPSG:32616"));

        // Pre-set worldOrigin to a non-sentinel value so the first-solve
        // auto-compute branch in cwLinePlotManager::publishLinePlotResults
        // is skipped. Otherwise the first run after the fix is added would
        // auto-compute worldOrigin and queue a second run via
        // worldOriginChanged → runSurvex; waitToFinish only blocks on the
        // first run, so the assertions below would race the second run and
        // intermittently see un-shifted positions. The value is arbitrary:
        // the auto-compute guard only checks against the default sentinel.
        const cwGeoPoint kAutoComputeSuppressor{1.0, 1.0, 1.0};
        region.setWorldOrigin(kAutoComputeSuppressor);

        plotManager->waitToFinish();
        const int spyAfterGlobalCS = positionSpy.count();
        REQUIRE(spyAfterGlobalCS >= 1); // setting globalCS itself triggered a run

        // Fix a1 in EPSG:32616. Cavern propagates the fix through the rest
        // of the cave: a2 (10m magnetic north of a1) should land 10m
        // further north than a1 in projected coords.
        cwFixStation fix;
        fix.setStationName(QStringLiteral("a1"));
        fix.setInputCS(QStringLiteral("EPSG:32616"));
        fix.setEasting(100.0);
        fix.setNorthing(200.0);
        fix.setElevation(50.0);
        cave->fixStations()->appendFixStation(fix);

        plotManager->waitToFinish();

        // Reported positions are shifted by -worldOrigin for shader float
        // precision (see cwLinePlotTask::applyWorldOriginOffset). Add the
        // current worldOrigin back to recover absolute projected coords.
        const auto absolutePosition = [&](const QString& station) {
            return cave->stationPositionLookup().position(station)
                + region.worldOrigin().toVector3D();
        };

        CHECK(positionSpy.count() > spyAfterGlobalCS);
        CHECK(absolutePosition("a1") == QVector3D(100.0f, 200.0f, 50.0f));
        CHECK(absolutePosition("a2") == QVector3D(100.0f, 210.0f, 50.0f));

        SECTION("setData on the fix-station model triggers a re-run") {
            const int spyBefore = positionSpy.count();

            auto* fixModel = cave->fixStations();
            REQUIRE(fixModel->rowCount() == 1);
            fixModel->setData(fixModel->index(0),
                              QVariant::fromValue(300.0),
                              cwFixStationModel::EastingRole);

            plotManager->waitToFinish();

            CHECK(positionSpy.count() > spyBefore);
            CHECK(absolutePosition("a1") == QVector3D(300.0f, 200.0f, 50.0f));
            CHECK(absolutePosition("a2") == QVector3D(300.0f, 210.0f, 50.0f));
        }

        SECTION("removeAt on the fix-station model triggers a re-run") {
            const int spyBefore = positionSpy.count();

            cave->fixStations()->removeAt(0);
            plotManager->waitToFinish();

            CHECK(positionSpy.count() > spyBefore);
            // Without explicit fixes the legacy fallback (*fix a1 0 0 0)
            // re-anchors the cave at the origin.
            CHECK(absolutePosition("a1") == QVector3D(0.0f, 0.0f, 0.0f));
            CHECK(absolutePosition("a2") == QVector3D(0.0f, 10.0f, 0.0f));
        }
    }
}

TEST_CASE("cwLinePlotManager handles chunks with empty shots (bug #435)", "[LinePlotManager]")
{
    // Empty shot rows used to emit "from to - - -" / "name - - - -" stubs that
    // cavern rejected, killing the entire line plot.

    auto makeShot = [](const QString& dist, const QString& compass, const QString& clino) {
        cwShot s;
        s.setDistance(cwDistanceReading(dist));
        s.setCompass(cwCompassReading(compass));
        s.setClino(cwClinoReading(clino));
        return s;
    };

    SECTION("Empty leading station + empty leading shot is dropped") {
        cwCavingRegion region;
        cwCave* cave = new cwCave();
        cave->setName("Cave 1");
        region.addCave(cave);
        cwTrip* trip = new cwTrip();
        trip->setName("Trip 1");
        cave->addTrip(trip);

        cwSurveyChunk* chunk1 = new cwSurveyChunk();
        trip->addChunk(chunk1);
        chunk1->appendShot(cwStation("a1"), cwStation("a2"), makeShot("10.0", "0.0", "0.0"));

        cwSurveyChunk* chunk2 = new cwSurveyChunk();
        trip->addChunk(chunk2);
        cwSurveyChunkData data;
        data.stations = { cwStation(), cwStation("a2"), cwStation("a3") };
        data.shots = { cwShot(), makeShot("5.0", "90.0", "0.0") };
        chunk2->setData(data);

        auto plotManager = std::make_unique<cwLinePlotManager>();
        plotManager->setRegion(&region);
        plotManager->waitToFinish();

        CHECK(cave->stationPositionLookup().hasPosition("a1"));
        CHECK(cave->stationPositionLookup().hasPosition("a2"));
        CHECK(cave->stationPositionLookup().hasPosition("a3"));
        // Geometry must include both chunk1 (10m) and chunk2's a2->a3 (5m).
        CHECK(cave->length()->value() == Catch::Approx(15.0).epsilon(0.01));
    }

    SECTION("Empty trailing shot with valid trailing station is dropped") {
        cwCavingRegion region;
        cwCave* cave = new cwCave();
        cave->setName("Cave 1");
        region.addCave(cave);
        cwTrip* trip = new cwTrip();
        trip->setName("Trip 1");
        cave->addTrip(trip);

        cwSurveyChunk* chunk = new cwSurveyChunk();
        trip->addChunk(chunk);
        cwSurveyChunkData data;
        data.stations = {
            cwStation("a1"), cwStation("a2"), cwStation("a3"), cwStation("x")
        };
        data.shots = {
            makeShot("10.0", "0.0", "0.0"),
            makeShot("5.0", "90.0", "0.0"),
            cwShot()
        };
        chunk->setData(data);

        auto plotManager = std::make_unique<cwLinePlotManager>();
        plotManager->setRegion(&region);
        plotManager->waitToFinish();

        CHECK(cave->stationPositionLookup().hasPosition("a1"));
        CHECK(cave->stationPositionLookup().hasPosition("a2"));
        CHECK(cave->stationPositionLookup().hasPosition("a3"));
        CHECK(cave->stationPositionLookup().position("a1") == QVector3D(0.0, 0.0, 0.0));
        CHECK(cave->stationPositionLookup().position("a2") == QVector3D(0.0, 10.0, 0.0));
        CHECK(cave->stationPositionLookup().position("a3") == QVector3D(5.0, 10.0, 0.0));
        // Length covers a1->a2 + a2->a3 only; the dropped a3->x has no distance.
        CHECK(cave->length()->value() == Catch::Approx(15.0).epsilon(0.01));
    }

    SECTION("Empty middle shot lets cavern run on the rest of the cave") {
        cwCavingRegion region;
        cwCave* cave = new cwCave();
        cave->setName("Cave 1");
        region.addCave(cave);
        cwTrip* trip = new cwTrip();
        trip->setName("Trip 1");
        cave->addTrip(trip);

        cwSurveyChunk* chunk1 = new cwSurveyChunk();
        trip->addChunk(chunk1);
        chunk1->appendShot(cwStation("a1"), cwStation("a2"), makeShot("10.0", "0.0", "0.0"));

        cwSurveyChunk* chunk2 = new cwSurveyChunk();
        trip->addChunk(chunk2);
        cwSurveyChunkData data;
        data.stations = { cwStation("a2"), cwStation("ghost") };
        data.shots = { cwShot() };
        chunk2->setData(data);

        cwSurveyChunk* chunk3 = new cwSurveyChunk();
        trip->addChunk(chunk3);
        chunk3->appendShot(cwStation("a2"), cwStation("a4"), makeShot("5.0", "90.0", "0.0"));

        auto plotManager = std::make_unique<cwLinePlotManager>();
        plotManager->setRegion(&region);
        plotManager->waitToFinish();

        CHECK(cave->stationPositionLookup().hasPosition("a1"));
        CHECK(cave->stationPositionLookup().hasPosition("a2"));
        CHECK(cave->stationPositionLookup().hasPosition("a4"));
        // Geometry must cover chunk1 (10m) + chunk3 (5m); chunk2 is dropped.
        CHECK(cave->length()->value() == Catch::Approx(15.0).epsilon(0.01));
    }
}
