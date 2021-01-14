//Catch includes
#include "catch.hpp"

//Our includes
#include "cwCave.h"
#include "cwFixedStationModel.h"

TEST_CASE("cwCave should copy fixed stations correctly", "[cwCave]") {

    cwCave cave;
    REQUIRE(cave.fixedStations() != nullptr);
    CHECK(cave.fixedStations()->parent() == &cave);

    auto fixedStations = cave.fixedStations();

    cwFixedStation f1;
    f1.setStationName("a1");
    f1.setLatitude("0.1");
    f1.setLongitude("100");
    f1.setAltitude("30.0");

    cwFixedStation f2;
    f2.setStationName("a2");
    f2.setLatitude("0.2");
    f2.setLatitude("0.3");
    f2.setAltitude("10.0");

    fixedStations->append({f1, f2});

    CHECK(fixedStations->size() == 2);

    cwCave other(cave);

    CHECK(cave.fixedStations()->toList() == other.fixedStations()->toList());
}

