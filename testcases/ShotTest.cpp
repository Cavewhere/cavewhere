/**************************************************************************
**
**    Copyright (C) 2015 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#include "catch.hpp"

//Cavewhere includes
#include "cwShot.h"
#include "cwReadingStates.h"

TEST_CASE("Shot can be created and data can be changed", "[shot]") {
    cwShot shot;

    SECTION("Default constructor creates an invalid shot") {
        CHECK(shot.distance() == 0.0);
        CHECK(shot.distanceState() == cwDistanceStates::Empty);
        CHECK(shot.compass() == 0.0);
        CHECK(shot.compassState() == cwCompassStates::Empty);
        CHECK(shot.backCompass() == 0.0);
        CHECK(shot.backCompassState() == cwCompassStates::Empty);
        CHECK(shot.clino() == 0.0);
        CHECK(shot.clinoState() == cwClinoStates::Empty);
        CHECK(shot.backClino() == 0.0);
        CHECK(shot.backClinoState() == cwClinoStates::Empty);
        CHECK(shot.isDistanceIncluded() == true);
        CHECK(shot.isValid() == false);
    }

    SECTION("Normal shot constructor for a shot") {
        cwShot normalShot("4.3", "180.0", "0", "10", "-11");
        CHECK(normalShot.distance() == 4.3);
        CHECK(normalShot.distanceState() == cwDistanceStates::Valid);
        CHECK(normalShot.compass() == 180.0);
        CHECK(normalShot.compassState() == cwCompassStates::Valid);
        CHECK(normalShot.backCompass() == 0.0);
        CHECK(normalShot.backCompassState() == cwCompassStates::Valid);
        CHECK(normalShot.clino() == 10.0);
        CHECK(normalShot.clinoState() == cwClinoStates::Valid);
        CHECK(normalShot.backClino() == -11.0);
        CHECK(normalShot.backClinoState() == cwClinoStates::Valid);
        CHECK(normalShot.isDistanceIncluded() == true);
        CHECK(normalShot.isValid() == true);
    }

    SECTION("Shots can have thier data changed") {
        shot.setDistance(5.2);
        CHECK(shot.distance() == 5.2);
        shot.setDistance("4.2");
        CHECK(shot.distance() == 4.2);
        CHECK(shot.distanceState() == cwDistanceStates::Valid);

        shot.setCompass(2.2);
        CHECK(shot.compass() == 2.2);
        shot.setCompass("3.1");
        CHECK(shot.compass() == 3.1);
        CHECK(shot.compassState() == cwCompassStates::Valid);

        shot.setBackCompass(2.2);
        CHECK(shot.backCompass() == 2.2);
        shot.setBackCompass("3.1");
        CHECK(shot.backCompass() == 3.1);
        CHECK(shot.backCompassState() == cwCompassStates::Valid);

        shot.setClino(-30.0);
        CHECK(shot.clino() == -30.0);
        shot.setClino("10.0");
        CHECK(shot.clino() == 10.0);
        CHECK(shot.clinoState() == cwClinoStates::Valid);
        shot.setClino("up");
        CHECK(shot.clinoState() == cwClinoStates::Up);
        shot.setClino("DoWn");
        CHECK(shot.clinoState() == cwClinoStates::Down);

        shot.setBackClino(-30.0);
        CHECK(shot.backClino() == -30.0);
        shot.setBackClino("10.0");
        CHECK(shot.backClino() == 10.0);
        CHECK(shot.backClinoState() == cwClinoStates::Valid);
        shot.setBackClino("up");
        CHECK(shot.backClinoState() == cwClinoStates::Up);
        shot.setBackClino("DoWn");
        CHECK(shot.backClinoState() == cwClinoStates::Down);

        shot.setDistanceIncluded(false);
        CHECK(shot.isDistanceIncluded() == false);

        CHECK(shot.isValid() == true);
    }
}

