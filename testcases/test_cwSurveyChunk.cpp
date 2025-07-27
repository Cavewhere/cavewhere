/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#define CATCH_CONFIG_SFINAE
#include <catch2/catch_test_macros.hpp>

//Cavewhere includes
#include "cwError.h"
#include "cwErrorModel.h"
#include "cwErrorListModel.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwTripCalibration.h"

//Our includes
#include "TestHelper.h"

//Qt includes
#include "cwSignalSpy.h"

std::ostream& operator << ( std::ostream& os, QMap<int, cwTripCalibration*> const& value ) {
    os << "QMap<int, cwTripCalibration*>:[";
    for(auto iter = value.begin(); iter != value.end(); ++iter) {
        os << iter.key() << ":" << iter.value();
        if(std::next(iter, 1) != value.end()) {
            os << ", ";
        }
    }
    os << "]";
    return os;
}

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
TEST_CASE("Checks cwSurveyChunk errors", "[cwSurveyChunk]")
{
    auto chunk = std::make_unique<cwSurveyChunk>();

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
    checkLRUDWarnings(chunk.get(), 0);

    chunk->setData(cwSurveyChunk::StationNameRole, 1, "a2");
    checkLRUDWarnings(chunk.get(), 1); //4 warnings for LRUD

    //There should be errors for each of the reading components
    CHECK(chunk->errorModel()->fatalCount() == 5); //Full shot data: distance, comp, clino, bs's
    CHECK(chunk->errorModel()->warningCount() == 8); //2 x LRUD

    checkNoShotDataError(chunk.get(), 0, cwSurveyChunk::ShotDistanceRole);
    checkNoShotDataError(chunk.get(), 0, cwSurveyChunk::ShotCompassRole);
    checkNoShotDataError(chunk.get(), 0, cwSurveyChunk::ShotClinoRole);
    checkNoShotDataError(chunk.get(), 0, cwSurveyChunk::ShotBackClinoRole);
    checkNoShotDataError(chunk.get(), 0, cwSurveyChunk::ShotClinoRole);

    //Set the first shots data
    int shotIndex = 0;
    chunk->setData(cwSurveyChunk::ShotDistanceRole, shotIndex, 10);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::ShotDistanceRole) == nullptr);

    chunk->setData(cwSurveyChunk::ShotCompassRole, shotIndex, 0);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::ShotCompassRole) == nullptr);
    checkNoShotDataError(chunk.get(), shotIndex, cwSurveyChunk::ShotBackCompassRole, cwError::Warning);

    chunk->setData(cwSurveyChunk::ShotBackCompassRole, shotIndex, 180);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::ShotBackCompassRole) == nullptr);

    chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, 3);
    CHECK(chunk->errorsAt(shotIndex, cwSurveyChunk::ShotClinoRole) == nullptr);
    checkNoShotDataError(chunk.get(), shotIndex, cwSurveyChunk::ShotBackClinoRole, cwError::Warning);

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
            checkNoShotDataError(chunk.get(), shotIndex, cwSurveyChunk::ShotCompassRole, cwError::Warning);
        }

        SECTION("Remove back compass") {
            shotIndex = 1;
            chunk->setData(cwSurveyChunk::ShotBackCompassRole, shotIndex, "");
            checkNoShotDataError(chunk.get(), shotIndex, cwSurveyChunk::ShotBackCompassRole, cwError::Warning);
        }

        SECTION("Remove clino") {
            shotIndex = 1;
            chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, "");
            checkNoShotDataError(chunk.get(), shotIndex, cwSurveyChunk::ShotClinoRole, cwError::Warning);
        }

        SECTION("Remove back clino") {
            shotIndex = 1;
            chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, "");
            checkNoShotDataError(chunk.get(), shotIndex, cwSurveyChunk::ShotBackClinoRole, cwError::Warning);
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
        auto trip = std::make_unique<cwTrip>();

        CHECK(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 0);

        trip->addChunk(chunk.get());

        CHECK(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 0);

        trip->calibrations()->setCorrectedCompassBacksight(true);

        CHECK(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 2); //2 x compass

        trip->calibrations()->setCorrectedClinoBacksight(true);

        CHECK(chunk->errorModel()->fatalCount() == 0);
        CHECK(chunk->errorModel()->warningCount() == 4); //2 x compass and clino
        chunk.release();
    }

    SECTION("Turn off back sights") {
        shotIndex = 1;
        auto trip = std::make_unique<cwTrip>();
        trip->addChunk(chunk.get());

        //Remove the back sites for compass
        chunk->setData(cwSurveyChunk::ShotBackCompassRole, shotIndex, "");

        //Remove the back sites for clino
        chunk->setData(cwSurveyChunk::ShotBackClinoRole, shotIndex, "");

        CHECK(chunk->errorModel()->warningCount() == 2);

        //Turn off backsights
        trip->calibrations()->setBackSights(false);

        CHECK(chunk->errorModel()->warningCount() == 0);
        chunk.release(); //Trip owns this pointer now
    }

    SECTION("Turn off front sights") {
        shotIndex = 1;
        auto trip = std::make_unique<cwTrip>();
        trip->addChunk(chunk.get());

        //Remove the back sites for compass
        chunk->setData(cwSurveyChunk::ShotCompassRole, shotIndex, "");

        //Remove the back sites for clino
        chunk->setData(cwSurveyChunk::ShotClinoRole, shotIndex, "");

        CHECK(chunk->errorModel()->warningCount() == 2);

        //Turn off backsights
        trip->calibrations()->setFrontSights(false);

        CHECK(chunk->errorModel()->warningCount() == 0);
        chunk.release(); //Trip owns this pointer now
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
        auto trip = std::make_unique<cwTrip>();
        trip->addChunk(chunk.get());

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

        chunk.release(); //trip owns this pointer now
    }
}

/**
 * @brief TEST_CASE
 *
 * This test creates 5 shots and adds and removes calibrations from the surveyChunk
 */
TEST_CASE("Tests adding removing and getting copying cwSurveyChunk calibrations", "[cwSurveyChunk]") {

    cwSurveyChunk chunk;
    cwSignalSpy spy(&chunk, SIGNAL(calibrationsChanged()));

    chunk.appendNewShot();
    chunk.setData(cwSurveyChunk::StationNameRole, 0, "a1");
    chunk.setData(cwSurveyChunk::ShotCompassRole, 0, "0");
    chunk.setData(cwSurveyChunk::ShotClinoRole, 0, "0");
    chunk.setData(cwSurveyChunk::ShotDistanceRole, 0, "10");
    chunk.setData(cwSurveyChunk::StationNameRole, 1, "a2");
    chunk.appendNewShot();
    chunk.setData(cwSurveyChunk::ShotCompassRole, 1, "0");
    chunk.setData(cwSurveyChunk::ShotClinoRole, 1, "0");
    chunk.setData(cwSurveyChunk::ShotDistanceRole, 1, "10");
    chunk.setData(cwSurveyChunk::StationNameRole, 2, "a3");
    chunk.appendNewShot();
    chunk.setData(cwSurveyChunk::ShotCompassRole, 2, "0");
    chunk.setData(cwSurveyChunk::ShotClinoRole, 2, "0");
    chunk.setData(cwSurveyChunk::ShotDistanceRole, 2, "10");
    chunk.setData(cwSurveyChunk::StationNameRole, 3, "a4");
    chunk.appendNewShot();
    chunk.setData(cwSurveyChunk::ShotCompassRole, 3, "0");
    chunk.setData(cwSurveyChunk::ShotClinoRole, 3, "0");
    chunk.setData(cwSurveyChunk::ShotDistanceRole, 3, "10");
    chunk.setData(cwSurveyChunk::StationNameRole, 4, "a5");

    SECTION("Add calibration") {
        CHECK(chunk.calibrations().isEmpty());
        CHECK(spy.isEmpty());

        chunk.addCalibration(0);
        CHECK(chunk.calibrations().size() == 1);
        CHECK(spy.size() == 1);

        REQUIRE(chunk.calibrations().value(0) != nullptr);
        CHECK(chunk.calibrations().value(0)->parent() == &chunk);

        cwTripCalibration* calibration = new cwTripCalibration();
        calibration->setTapeCalibration(1);
        chunk.addCalibration(3, calibration);
        CHECK(chunk.calibrations().size() == 2);
        CHECK(spy.size() == 2);
        REQUIRE(chunk.calibrations().value(3) == calibration);
        CHECK(chunk.calibrations().value(3)->parent() == &chunk);

        chunk.addCalibration(1);
        CHECK(chunk.calibrations().size() == 3);
        CHECK(spy.size() == 3);
        CHECK(chunk.calibrations().value(1) != nullptr);
        CHECK(chunk.calibrations().value(2) == nullptr);

        SECTION("Copy calibration") {
            cwSurveyChunk chunkCopy;
            chunkCopy.setData(chunk.data());

            CHECK(chunkCopy.calibrations().size() == 3);
            auto calibrations = chunkCopy.calibrations();
            for(auto iter = calibrations.begin();
                iter != calibrations.end();
                iter++)
            {
                CHECK(iter.key() != 2);
                CHECK(iter.value()->parent() == &chunkCopy);
            }

            CHECK(chunkCopy.calibrations().value(3)->tapeCalibration() == 1);
        }

        CHECK(spy.size() == 3);

        SECTION("Remove calibration") {
            chunk.removeCalibration(2);
            CHECK(chunk.calibrations().size() == 3);

            chunk.removeCalibration(0);
            CHECK(chunk.calibrations().size() == 2);
            CHECK(chunk.calibrations().value(0) == nullptr);
            CHECK(chunk.calibrations().value(3)->tapeCalibration() == 1);
            CHECK(spy.size() == 4);

            chunk.removeCalibration(3);
            CHECK(chunk.calibrations().size() == 1);
            CHECK(chunk.calibrations().value(3) == nullptr);
            CHECK(spy.size() == 5);

            chunk.removeCalibration(1);
            CHECK(chunk.calibrations().size() == 0);
            CHECK(chunk.calibrations().value(1) == nullptr);
            CHECK(spy.size() == 6);
        }

        SECTION("Insert shots") {
            CHECK(chunk.calibrations().size() == 3);
            CHECK(spy.size() == 3);

            QMap<int, cwTripCalibration*> calibrations1 = chunk.calibrations();

            chunk.appendNewShot();

            CHECK(chunk.calibrations().size() == 3);
            CHECK(spy.size() == 3);

            CHECK(chunk.calibrations() == calibrations1);

            QMap<int, cwTripCalibration*> calibrations2;
            for(auto iter = calibrations1.begin(); iter != calibrations1.end(); ++iter) {
                calibrations2.insert(iter.key() + 1, iter.value());
            }

            chunk.insertShot(0, cwSurveyChunk::Above);
            CHECK(chunk.calibrations() == calibrations2);
            CHECK(spy.size() == 4);

            QMap<int, cwTripCalibration*> calibrations3;
            for(auto iter = calibrations2.begin(); iter != calibrations2.end(); ++iter) {
                if(iter.key() >= 3) {
                    calibrations3.insert(iter.key() + 1, iter.value());
                } else {
                    calibrations3.insert(iter.key(), iter.value());
                }
            }
            chunk.insertShot(3, cwSurveyChunk::Above);
            CHECK(chunk.calibrations() == calibrations3);
            CHECK(spy.size() == 5);
        }

        SECTION("Remove shots") {
            CHECK(chunk.calibrations().size() == 3);
            CHECK(spy.size() == 3);

            chunk.appendNewShot();

            QMap<int, cwTripCalibration*> calibrations1 = chunk.calibrations();

            //Remove the last shot
            chunk.removeShot(4, cwSurveyChunk::Below);
            CHECK(chunk.calibrations().size() == 3);
            CHECK(spy.size() == 3);
            CHECK(calibrations1 == chunk.calibrations());

            //Remove First shot, overrides the calibration
            QMap<int, cwTripCalibration*> calibrations2;
            for(auto iter = calibrations1.begin(); iter != calibrations1.end(); ++iter) {
                calibrations2.insert(iter.key() - 1, iter.value());
            }
            calibrations2.insert(0, calibrations2.value(-1));
            calibrations2.remove(-1);
            chunk.removeShot(0, cwSurveyChunk::Above);
            CHECK(spy.size() == 4);
            CHECK(chunk.calibrations().size() == 2);
            CHECK(chunk.calibrations() == calibrations2);

            //Remove the last shot again
            QMap<int, cwTripCalibration*> calibrations3 = calibrations2;
            calibrations3.remove(2);
            chunk.removeShot(2, cwSurveyChunk::Below);
            CHECK(chunk.calibrations().size() == 1);
            CHECK(spy.size() == 5);
            CHECK(calibrations3 == chunk.calibrations());
        }
    }

}

TEST_CASE("Fix to abort - Test cwSurveyChunk with mostly empty data and copy", "[cwSurveyChunk]") {
    cwSurveyChunk chunk;
    chunk.appendNewShot();

    REQUIRE(chunk.stationCount() == 2);
    REQUIRE(chunk.shotCount() == 1);

    chunk.setData(cwSurveyChunk::StationNameRole, 1, "b2");
    CHECK(chunk.data(cwSurveyChunk::StationNameRole, 1).toString().toStdString() == "b2");

    cwSurveyChunk chunkCopy;
    chunkCopy.setData(chunk.data());

    REQUIRE(chunkCopy.stationCount() == 2);
    REQUIRE(chunkCopy.shotCount() == 1);

    CHECK(chunkCopy.data(cwSurveyChunk::StationNameRole, 1).toString().toStdString() == "b2");
}
