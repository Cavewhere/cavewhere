/**************************************************************************
**
**    Copyright (C) 2016 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Catch includes
#include "catch.hpp"

//Cavewhere includes
#include <cwTripCalibration.h>

//Qt inculdes
#include <QSignalSpy>

//Our includes
#include "TestHelper.h"

TEST_CASE("Trip Calibration Getters and setters should work") {

    cwTripCalibration* tripCalibration = new cwTripCalibration();

    //Check constructor
    CHECK(tripCalibration->hasCorrectedClinoBacksight() == false);
    CHECK(tripCalibration->hasCorrectedCompassBacksight() == false);
    CHECK(tripCalibration->hasCorrectedCompassFrontsight() == false);
    CHECK(tripCalibration->hasCorrectedClinoFrontsight() == false);
    CHECK(tripCalibration->tapeCalibration() == 0.0);
    CHECK(tripCalibration->frontCompassCalibration() == 0.0);
    CHECK(tripCalibration->backCompassCalibration() == 0.0);
    CHECK(tripCalibration->frontClinoCalibration() == 0.0);
    CHECK(tripCalibration->backClinoCalibration() == 0.0);
    CHECK(tripCalibration->declination() == 0.0);
    CHECK(tripCalibration->distanceUnit() == cwUnits::Meters);
    CHECK(tripCalibration->hasFrontSights() == true);
    CHECK(tripCalibration->hasBackSights() == true);

    QSignalSpy* calibrationChanged = new QSignalSpy(tripCalibration, SIGNAL(calibrationsChanged()));

    tripCalibration->setCorrectedClinoBacksight(true);
    CHECK(tripCalibration->hasCorrectedClinoBacksight() == true);
    CHECK(calibrationChanged->size() == 1);

    tripCalibration->setCorrectedCompassBacksight(true);
    CHECK(tripCalibration->hasCorrectedCompassBacksight() == true);
    CHECK(calibrationChanged->size() == 2);

    tripCalibration->setCorrectedCompassFrontsight(true);
    CHECK(tripCalibration->hasCorrectedCompassFrontsight() == true);
    CHECK(calibrationChanged->size() == 3);

    tripCalibration->setCorrectedClinoFrontsight(true);
    CHECK(tripCalibration->hasCorrectedClinoFrontsight() == true);
    CHECK(calibrationChanged->size() == 4);

    tripCalibration->setTapeCalibration(1.0);
    CHECK(tripCalibration->tapeCalibration() == 1.0);
    CHECK(calibrationChanged->size() == 5);

    tripCalibration->setFrontCompassCalibration(2.0);
    CHECK(tripCalibration->frontCompassCalibration() == 2.0);
    CHECK(calibrationChanged->size() == 6);

    tripCalibration->setBackCompassCalibration(3.0);
    CHECK(tripCalibration->backCompassCalibration() == 3.0);
    CHECK(calibrationChanged->size() == 7);

    tripCalibration->setFrontClinoCalibration(4.0);
    CHECK(tripCalibration->frontClinoCalibration() == 4.0);
    CHECK(calibrationChanged->size() == 8);

    tripCalibration->setBackClinoCalibration(5.0);
    CHECK(tripCalibration->backClinoCalibration() == 5.0);
    CHECK(calibrationChanged->size() == 9);

    tripCalibration->setDeclination(6.0);
    CHECK(tripCalibration->declination() == 6.0);
    CHECK(calibrationChanged->size() == 10);

    tripCalibration->setDistanceUnit(cwUnits::Feet);
    CHECK(tripCalibration->distanceUnit() == cwUnits::Feet);
    CHECK(calibrationChanged->size() == 11);

    tripCalibration->setFrontSights(false);
    CHECK(tripCalibration->hasFrontSights() == false);
    CHECK(calibrationChanged->size() == 12);

    tripCalibration->setBackSights(false);
    CHECK(tripCalibration->hasBackSights() == false);
    CHECK(calibrationChanged->size() == 13);

    SECTION("Check copy constructor") {
        //Check copy operator
        cwTripCalibration* calibration2 = new cwTripCalibration(*tripCalibration);

        //Byte for byte comparison
        propertyCompare(tripCalibration, calibration2);
    }

    SECTION("Check assignment operator") {
        //Check copy operator
        cwTripCalibration* calibration2 = new cwTripCalibration();

        *calibration2 = *tripCalibration;

        //Byte for byte comparison
        propertyCompare(tripCalibration, calibration2);
    }



}

