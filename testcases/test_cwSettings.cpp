//Catch includes
#include "catch.hpp"

//Our includes
#include "cwSettings.h"

TEST_CASE("cwSettings should initilize correctly", "[cwSettings]") {
    cwSettings::initialize();
    auto settings1 = cwSettings::instance();
    cwSettings::initialize();
    auto settings2 = cwSettings::instance();

    CHECK(settings1 == settings2);
    REQUIRE(settings1);

    CHECK(settings1->renderingSettings());
    CHECK(settings1->jobSettings());
}
