/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#define CATCH_CONFIG_SFINAE
#include "catch.hpp"

//Our includes
#include "cwError.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"

#include "TestHelper.h"

void printErrorForChunk(const cwSurveyChunk* chunk) {
    foreach(cwErrorModel* model, chunk->errorModel()->childModels()) {
        foreach(cwError error, model->errors()->toList()) {
            qDebug() << "Error: " << error.message();
        }
    }
}

/**
 * @brief checkLRUDWarnings
 * @param chunk
 * @param index
 *
 * Check to see if the LRUD is empty
 */
void checkLRUDWarnings(cwSurveyChunk* chunk, int index) {
    QList<cwErrorModel*> models;
    models.append(chunk->errorsAt(index, cwSurveyChunk::StationLeftRole));
    models.append(chunk->errorsAt(index, cwSurveyChunk::StationRightRole));
    models.append(chunk->errorsAt(index, cwSurveyChunk::StationUpRole));
    models.append(chunk->errorsAt(index, cwSurveyChunk::StationDownRole));

    QStringList directions;
    directions.append("left");
    directions.append("right");
    directions.append("up");
    directions.append("down");

    QString stationName = chunk->data(cwSurveyChunk::StationNameRole, index).toString();

    for(int i = 0; i < models.size(); i++) {
        cwErrorModel* model = models.at(i);

        REQUIRE(model != nullptr);

        REQUIRE(model->warningCount() == 1);
        CHECK(model->fatalCount() == 0);

        CHECK(model->errors()->first().type() == cwError::Warning);
        CHECK(model->errors()->first().message() == QString("Missing \"%1\" for station \"%2\"").arg(directions.at(i)).arg(stationName));
    }

}

void checkNoShotDataError(cwSurveyChunk* chunk, int index, cwSurveyChunk::DataRole role, cwError::ErrorType type = cwError::Fatal) {
    cwErrorModel* model = chunk->errorsAt(index, role);

    INFO("Model:" << model << " chunk:" << chunk << " index:" << index << " role:" << role << " type:" << type);

    QString roleName;

    switch(role) {
    case cwSurveyChunk::ShotDistanceRole:
        roleName = "distance";
        break;
    case cwSurveyChunk::ShotCompassRole:
        roleName = "compass";
        break;
    case cwSurveyChunk::ShotBackCompassRole:
        roleName = "backsite compass";
        break;
    case cwSurveyChunk::ShotClinoRole:
        roleName = "clino";
        break;
    case cwSurveyChunk::ShotBackClinoRole:
        roleName = "backsite clino";
        break;
    default:
        REQUIRE(false); //Error in the testcase
    }

    QString fromStation = chunk->data(cwSurveyChunk::StationNameRole, index).toString();
    QString toStation = chunk->data(cwSurveyChunk::StationNameRole, index + 1).toString();

    REQUIRE(model != nullptr);
    if(type == cwError::Fatal) {
        CHECK(model->fatalCount() == 1);
        CHECK(model->warningCount() == 0);
    } else {
        CHECK(model->fatalCount() == 0);
        CHECK(model->warningCount() == 1);
    }
    INFO(model->errors()->first().message() << "|" << QString("Missing \"%1\" from shot \"%2\" ➔ \"%3\"").arg(roleName, fromStation, toStation));
    CHECK(model->errors()->first().message() == QString("Missing \"%1\" from shot \"%2\" ➔ \"%3\"").arg(roleName, fromStation, toStation));
    CHECK(model->errors()->first().type() == type);
}

/**
  This testcase tests the error handling inside of a cwSurveyChunk
  */
TEST_CASE("Checks cwSurveyChunk errors")
{
    cwSurveyChunk* chunk = new cwSurveyChunk();

    //New chunks have no warnings or errors
    CHECK(chunk->errorModel()->fatalCount() == 0);
    CHECK(chunk->errorModel()->warningCount() == 0);

    //Adding a new shot has no warnings or errors
    chunk->appendNewShot();
    CHECK(chunk->errorModel()->fatalCount() == 0);
    CHECK(chunk->errorModel()->warningCount() == 0);

    //    SECTION("Setting the first station name") {
    chunk->setData(cwSurveyChunk::StationNameRole, 0, "a1");
    CHECK(chunk->errorModel()->fatalCount() == 0);
    CHECK(chunk->errorModel()->warningCount() == 4); //4 warnings for LRUD
    checkLRUDWarnings(chunk, 0);

    chunk->setData(cwSurveyChunk::StationNameRole, 1, "a2");
    checkLRUDWarnings(chunk, 1); //4 warnings for LRUD

    //There should be errors for each of the reading components
    CHECK(chunk->errorModel()->fatalCount() == 5); //Full shot data: distance, comp, clino, bs's
    CHECK(chunk->errorModel()->warningCount() == 8); //2 x LRUD

    checkNoShotDataError(chunk, 0, cwSurveyChunk::ShotDistanceRole);
    checkNoShotDataError(chunk, 0, cwSurveyChunk::ShotCompassRole);
    checkNoShotDataError(chunk, 0, cwSurveyChunk::ShotClinoRole);
    checkNoShotDataError(chunk, 0, cwSurveyChunk::ShotBackClinoRole);
    checkNoShotDataError(chunk, 0, cwSurveyChunk::ShotClinoRole);

    //Set the first shots data
    int shotIndex = 0;
    chunk->setData(cwSurveyChunk::ShotDistanceRole, shotIndex, 10);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::ShotDistanceRole) == nullptr);

    chunk->setData(cwSurveyChunk::ShotCompassRole, shotIndex, 0);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::ShotCompassRole) == nullptr);
    checkNoShotDataError(chunk, shotIndex, cwSurveyChunk::ShotBackCompassRole, cwError::Warning);

    chunk->setData(cwSurveyChunk::ShotBackCompassRole, shotIndex, 180);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::ShotBackCompassRole) == nullptr);

    chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, 3);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::ShotClinoRole) == nullptr);
    checkNoShotDataError(chunk, shotIndex, cwSurveyChunk::ShotBackClinoRole, cwError::Warning);

    chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, -3);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::ShotBackClinoRole) == nullptr);

    CHECK(chunk->errorModel()->fatalCount() == 0);
    CHECK(chunk->errorModel()->warningCount() == 8); //Still 2 x LRUD

    //Fill in the LRUD data
    int stationIndex = 0;
    chunk->setData(cwSurveyChunk::StationLeftRole, stationIndex, 1.2);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::StationLeftRole) == nullptr);

    chunk->setData(cwSurveyChunk::StationRightRole, stationIndex, 3.2);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::StationRightRole) == nullptr);

    chunk->setData(cwSurveyChunk::StationUpRole, stationIndex, 4.2);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::StationUpRole) == nullptr);

    chunk->setData(cwSurveyChunk::StationDownRole, stationIndex, 6.4);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::StationDownRole) == nullptr);

    CHECK(chunk->errorModel()->fatalCount() == 0);
    CHECK(chunk->errorModel()->warningCount() == 4); //Still 1 x LRUD

    stationIndex = 1;
    chunk->setData(cwSurveyChunk::StationLeftRole, stationIndex, 2.1);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::StationLeftRole) == nullptr);

    chunk->setData(cwSurveyChunk::StationRightRole, stationIndex, 2.3);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::StationRightRole) == nullptr);

    chunk->setData(cwSurveyChunk::StationUpRole, stationIndex, 2.4);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::StationUpRole) == nullptr);

    chunk->setData(cwSurveyChunk::StationDownRole, stationIndex, 4.6);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::StationDownRole) == nullptr);

    CHECK(chunk->errorModel()->fatalCount() == 0);
    CHECK(chunk->errorModel()->warningCount() == 0); //All good

    //Append one more shot
    chunk->appendNewShot();
    stationIndex = 2;
    shotIndex = 1;
    chunk->setData(cwSurveyChunk::StationNameRole, stationIndex, "a3");
    chunk->setData(cwSurveyChunk::StationLeftRole, stationIndex, 5.32);
    chunk->setData(cwSurveyChunk::StationRightRole, stationIndex, 5.3);
    chunk->setData(cwSurveyChunk::StationUpRole, stationIndex, 6.4);
    chunk->setData(cwSurveyChunk::StationDownRole, stationIndex, 43.6);

    chunk->setData(cwSurveyChunk::ShotDistanceRole, shotIndex, 10);
    chunk->setData(cwSurveyChunk::ShotCompassRole, shotIndex, 120);
    chunk->setData(cwSurveyChunk::ShotBackCompassRole, shotIndex, 300);
    chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, -4.3);
    chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, 4.5);

    CHECK(chunk->errorModel()->fatalCount() == 0);
    CHECK(chunk->errorModel()->warningCount() == 0); //All good

    SECTION("Delete values out of the chunk") {

        SECTION("Remove empty station names") {
            chunk->setData(cwSurveyChunk::StationNameRole, 1, "");
            REQUIRE(chunk->errorModel()->fatalCount() == 1);

            cwErrorModel* errorModel = chunk->errorsAt(1, cwSurveyChunk::StationNameRole);

            REQUIRE(errorModel != nullptr);
            REQUIRE(errorModel->errors()->size() == 1);
            CHECK(errorModel->errors()->first().message() == QString("Missing station name"));
            CHECK(errorModel->errors()->first().type() == cwError::Fatal);
        }

        SECTION("Remove empty station names and shot from the middle of the chunk") {
            shotIndex = 1;
            chunk->setData(cwSurveyChunk::StationNameRole, 1, "");
            chunk->setData(cwSurveyChunk::ShotDistanceRole, shotIndex, "");
            chunk->setData(cwSurveyChunk::ShotCompassRole, shotIndex, "");
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, shotIndex, "");
            chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, "");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, "");
            REQUIRE(chunk->errorModel()->fatalCount() == 1);

            cwErrorModel* errorModel = chunk->errorsAt(1, cwSurveyChunk::StationNameRole);

            REQUIRE(errorModel != nullptr);
            REQUIRE(errorModel->errors()->size() == 1);
            CHECK(errorModel->errors()->first().message() == QString("Missing station name"));
            CHECK(errorModel->errors()->first().type() == cwError::Fatal);
        }

        SECTION("Remove compass") {
            shotIndex = 1;
            chunk->setData(cwSurveyChunk::ShotCompassRole, shotIndex, "");
            checkNoShotDataError(chunk, shotIndex, cwSurveyChunk::ShotCompassRole, cwError::Warning);
        }

        SECTION("Remove back compass") {
            shotIndex = 1;
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, shotIndex, "");
            checkNoShotDataError(chunk, shotIndex, cwSurveyChunk::ShotBackCompassRole, cwError::Warning);
        }

        SECTION("Remove clino") {
            shotIndex = 1;
            chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, "");
            checkNoShotDataError(chunk, shotIndex, cwSurveyChunk::ShotClinoRole, cwError::Warning);
        }

        SECTION("Remove back clino") {
            shotIndex = 1;
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, "");
            checkNoShotDataError(chunk, shotIndex, cwSurveyChunk::ShotBackClinoRole, cwError::Warning);
        }
    }

    SECTION("Add a new shot without setting the station name") {
        //Append a new shot to make sure there's no new warnings or fatal errors
        chunk->appendNewShot();
        CHECK(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 0);

        chunk->setData(cwSurveyChunk::ShotDistanceRole, 2, 100.3);

        //The new station name, doesn't exist, so there should be a fatal error at the station name cell
        REQUIRE(chunk->errorModel()->fatalCount() == 1);
        CHECK(chunk->errorModel()->warningCount() == 0);

        cwErrorModel* errorModel = chunk->errorsAt(3, cwSurveyChunk::StationNameRole);

        REQUIRE(errorModel != nullptr);
        REQUIRE(errorModel->errors()->size() == 1);
        CHECK(errorModel->errors()->first().message() == QString("Missing station name"));
        CHECK(errorModel->errors()->first().type() == cwError::Fatal);
    }

    SECTION("Off by two change front sight") {
        shotIndex = 1;
        chunk->setData(cwSurveyChunk::ShotCompassRole, shotIndex, 117);

        REQUIRE(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 1);

        cwErrorModel* errorModel = chunk->errorsAt(shotIndex, cwSurveyChunk::ShotCompassRole);

        REQUIRE(errorModel != nullptr);
        REQUIRE(errorModel->errors()->size() == 1);
        CHECK(errorModel->errors()->first().message() == QString("Frontsight and backsight differs by 3°"));
        CHECK(errorModel->errors()->first().type() == cwError::Warning);

        chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, -1.1);

        REQUIRE(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 2);

        errorModel = chunk->errorsAt(shotIndex, cwSurveyChunk::ShotClinoRole);

        REQUIRE(errorModel != nullptr);
        REQUIRE(errorModel->errors()->size() == 1);
        CHECK(errorModel->errors()->first().message() == QString("Frontsight and backsight differs by 3.4°"));
        CHECK(errorModel->errors()->first().type() == cwError::Warning);
    }

    SECTION("Off by two change back sight") {
        shotIndex = 1;
        chunk->setData(cwSurveyChunk::ShotBackCompassRole, shotIndex, 303);

        cwErrorModel* errorModel = chunk->errorsAt(shotIndex, cwSurveyChunk::ShotCompassRole);

        REQUIRE(errorModel != nullptr);
        REQUIRE(errorModel->errors()->size() == 1);
        CHECK(errorModel->errors()->first().message() == QString("Frontsight and backsight differs by 3°"));
        CHECK(errorModel->errors()->first().type() == cwError::Warning);

        chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, 7.9);

        REQUIRE(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 2);

        errorModel = chunk->errorsAt(shotIndex, cwSurveyChunk::ShotClinoRole);

        REQUIRE(errorModel != nullptr);
        REQUIRE(errorModel->errors()->size() == 1);
        INFO(errorModel->errors()->first().message());
        CHECK(errorModel->errors()->first().message() == QString("Frontsight and backsight differs by 3.6°"));
        CHECK(errorModel->errors()->first().type() == cwError::Warning);
    }

    SECTION("Off by two change calibration") {
        shotIndex = 1;
        cwTrip* trip = new cwTrip();

        CHECK(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 0);

        trip->addChunk(chunk);

        CHECK(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 0);

        trip->calibrations()->setCorrectedCompassBacksight(true);

        CHECK(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 2); //2 x compass

        trip->calibrations()->setCorrectedClinoBacksight(true);

        CHECK(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 4); //2 x compass and clino
    }

    SECTION("Turn off back sights") {
        shotIndex = 1;
        cwTrip* trip = new cwTrip();
        trip->addChunk(chunk);

        //Remove the back sites for compass
        chunk->setData(cwSurveyChunk::ShotBackCompassRole, shotIndex, "");

        //Remove the back sites for clino
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, "");

        CHECK(chunk->errorModel()->warningCount() == 2);

        //Turn off backsights
        trip->calibrations()->setBackSights(false);

        CHECK(chunk->errorModel()->warningCount() == 0);
    }

    SECTION("Turn off front sights") {
        shotIndex = 1;
        cwTrip* trip = new cwTrip();
        trip->addChunk(chunk);

        //Remove the back sites for compass
        chunk->setData(cwSurveyChunk::ShotCompassRole, shotIndex, "");

        //Remove the back sites for clino
        chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, "");

        CHECK(chunk->errorModel()->warningCount() == 2);

        //Turn off backsights
        trip->calibrations()->setFrontSights(false);

        CHECK(chunk->errorModel()->warningCount() == 0);
    }

    SECTION("Use Up and Down for the Clino") {
        shotIndex = 1;

        //Remove the back sites for compass
        chunk->setData(cwSurveyChunk::ShotBackCompassRole, shotIndex, "");
        chunk->setData(cwSurveyChunk::ShotCompassRole, shotIndex, "");

        //Remove the back sites for clino
        chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, "Down");
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, "");

        CHECK(chunk->errorModel()->warningCount() == 0);
        CHECK(chunk->errorModel()->fatalCount() == 0);
    }

    SECTION("Fatal error for different types for clino Up/Down and degrees for clino") {
        shotIndex = 1;

        //Different types, this will cause a failure
        chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, "Up");
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, "4.2");

        CHECK(chunk->errorModel()->warningCount() == 0);
        CHECK(chunk->errorModel()->fatalCount() == 1);

        cwErrorModel* errorModel = chunk->errorsAt(shotIndex, cwSurveyChunk::ShotClinoRole);
        REQUIRE(errorModel != nullptr);
        REQUIRE(errorModel->errors()->size() == 1);
        CHECK(errorModel->errors()->first().message() == QString("You are mixing types. Frontsight and backsight must both be Up or Down, or both numbers"));
        CHECK(errorModel->errors()->first().type() == cwError::Fatal);

        //Different types, this will cause a failure
        chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, "5.3");
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, "Down");

        CHECK(chunk->errorModel()->warningCount() == 0);
        CHECK(chunk->errorModel()->fatalCount() == 1);

        errorModel = chunk->errorsAt(shotIndex, cwSurveyChunk::ShotClinoRole);
        REQUIRE(errorModel != nullptr);
        REQUIRE(errorModel->errors()->size() == 1);
        CHECK(errorModel->errors()->first().message() == QString("You are mixing types. Frontsight and backsight must both be Up or Down, or both numbers"));
        CHECK(errorModel->errors()->first().type() == cwError::Fatal);
    }

    SECTION("No warning should be create with calibrations change") {
        cwTrip* trip = new cwTrip();
        trip->addChunk(chunk);

        chunk->removeStation(2, cwSurveyChunk::Above);

        CHECK(chunk->errorModel()->warningCount() == 0);
        CHECK(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->stations().size() == 2);
        CHECK(chunk->shots().size() == 1);

        SECTION("Corrected Front Compass") {
            chunk->setData(cwSurveyChunk::ShotCompassRole, 0, "10");
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "10");

            CHECK(chunk->errorModel()->warningCount() == 1);

            SECTION("Back compass calibration") {
                trip->calibrations()->setBackCompassCalibration(180);
                CHECK(chunk->errorModel()->warningCount() == 0);
                trip->calibrations()->setBackCompassCalibration(0.0);
            }

            CHECK(chunk->errorModel()->warningCount() == 1);

            SECTION("Compass calibration") {
                trip->calibrations()->setFrontCompassCalibration(180);
                CHECK(chunk->errorModel()->warningCount() == 0);
                trip->calibrations()->setFrontCompassCalibration(0.0);
            }

            CHECK(chunk->errorModel()->warningCount() == 1);

            SECTION("Compass Backwards") {
                trip->calibrations()->setCorrectedCompassFrontsight(true);
                CHECK(chunk->errorModel()->warningCount() == 0);
                trip->calibrations()->setCorrectedCompassFrontsight(false);
            }

            CHECK(chunk->errorModel()->warningCount() == 1);

            SECTION("Compass Corrected Backsight") {
                trip->calibrations()->setCorrectedCompassBacksight(true);
                CHECK(chunk->errorModel()->warningCount() == 0);
                trip->calibrations()->setCorrectedCompassBacksight(false);
            }
        }

        SECTION("Small Corrected Compass") {
            chunk->setData(cwSurveyChunk::ShotCompassRole, 0, "0");
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, 0, "183");

            //Off by 3
            CHECK(chunk->errorModel()->warningCount() == 1);

            SECTION("Back compass calibration") {
                trip->calibrations()->setBackCompassCalibration(-3);
                CHECK(chunk->errorModel()->warningCount() == 0);
                trip->calibrations()->setBackCompassCalibration(0);
            }

            CHECK(chunk->errorModel()->warningCount() == 1);

            SECTION("Compass calibration") {
                trip->calibrations()->setFrontCompassCalibration(3);
                CHECK(chunk->errorModel()->warningCount() == 0);
                trip->calibrations()->setFrontCompassCalibration(0);
            }
        }

        SECTION("Corrected Front Clino") {
            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "-45");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "-45");

            CHECK(chunk->errorModel()->warningCount() == 1);

            SECTION("Clino Backwards") {
                trip->calibrations()->setCorrectedClinoFrontsight(true);
                CHECK(chunk->errorModel()->warningCount() == 0);
                trip->calibrations()->setCorrectedClinoFrontsight(false);
            }

            CHECK(chunk->errorModel()->warningCount() == 1);

            SECTION("Clino Corrected Backsight") {
                trip->calibrations()->setCorrectedClinoBacksight(true);
                CHECK(chunk->errorModel()->warningCount() == 0);
                trip->calibrations()->setCorrectedClinoBacksight(false);
            }
        }

        SECTION("Small Corrected Clino") {
            chunk->setData(cwSurveyChunk::ShotClinoRole, 0, "10");
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, 0, "-13");

            //Off by 3
            CHECK(chunk->errorModel()->warningCount() == 1);

            SECTION("Back compass calibration") {
                trip->calibrations()->setBackClinoCalibration(3);
                CHECK(chunk->errorModel()->warningCount() == 0);
                trip->calibrations()->setBackClinoCalibration(0);
            }

            CHECK(chunk->errorModel()->warningCount() == 1);

            SECTION("Compass calibration") {
                trip->calibrations()->setFrontClinoCalibration(3);
                CHECK(chunk->errorModel()->warningCount() == 0);
                trip->calibrations()->setFrontClinoCalibration(0);
            }
        }
    }
}

