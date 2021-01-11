//Catch includes
#include "catch.hpp"

//Our includes
#include "cwFixedStation.h"

TEST_CASE("cwFixedStation should initilize correctly", "[cwFixedStation]") {
    cwFixedStation s;

    CHECK(s.latitude().toStdString() == "");
    CHECK(s.longitude().toStdString() == "");
    CHECK(s.altitude().toStdString() == "");
    CHECK(s.altitudeUnit() == cwUnits::Meters);

    CHECK(s.toGeoCoordinate().isValid() == false);
    CHECK(s.isValid() == false);
}

TEST_CASE("cwFixedStation should be valid with correct values", "[cwFixedStation]") {
    cwFixedStation s;
    s.setStationName("a1");
    s.setLatitude("40.2");
    s.setLongitude("20.1");
    s.setAltitude("23.0");
    CHECK(s.stationName().toStdString() == "a1");
    CHECK(s.latitude().toStdString() == "40.2");
    CHECK(s.longitude().toStdString() == "20.1");
    CHECK(s.altitude().toStdString() == "23.0");
    CHECK(s.toGeoCoordinate().isValid() == true);
    CHECK(s.toGeoCoordinate().latitude() == 40.2);
    CHECK(s.toGeoCoordinate().longitude() == 20.1);
    CHECK(s.toGeoCoordinate().altitude() == 23.0);

    SECTION("Units should work correctly") {
        s.setAltitudeUnit(cwUnits::Feet);
        CHECK(s.toGeoCoordinate().altitude() == Approx(7.0104));
    }
}

TEST_CASE("cwFixedStation should be invalid with bad values", "[cwFixedStation]") {

    cwFixedStation s;
    s.setStationName("a1");
    s.setLatitude("40.2");
    s.setLongitude("20.1");
    s.setAltitude("23.0");
    CHECK(s.isValid());

    SECTION("Bad name") {
        s.setStationName(QString());
        CHECK(s.isValid() == false);
    }

    SECTION("Bad Latitude") {
        s.setLatitude("100.0");
        CHECK(s.latitude().toStdString() == "100.0");
        CHECK(s.isValid() == false);

        s.setLatitude("-100.0");
        CHECK(s.isValid() == false);

        s.setLatitude("auoeu");
        CHECK(s.isValid() == false);
    }

    SECTION("Bad longitude") {
        s.setLongitude("190.0");
        CHECK(s.longitude().toStdString() == "190.0");
        CHECK(s.isValid() == false);

        s.setLongitude("-190.0");
        CHECK(s.isValid() == false);

        s.setLongitude("auo23eu");
        CHECK(s.isValid() == false);
    }

    SECTION("Bad altitude") {
        s.setAltitude("aoeust");
        CHECK(s.altitude().toStdString() == "aoeust");
        CHECK(s.isValid() == true);

        s.setAltitude("");
        CHECK(s.isValid() == true);
    }
}
