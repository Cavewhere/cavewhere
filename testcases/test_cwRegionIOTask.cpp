//catch inculdes
#include <catch2/catch.hpp>

//Our includes
#include "cwRegionIOTask.h"

TEST_CASE("Proto version should report a CaveWhere version correctly", "[cwRegionIOTask]") {
    //If this fails you need to update cwRegionIOTask to have the current version in there
    CHECK(cwRegionIOTask::toVersion(cwRegionIOTask::protoVersion()).isEmpty() == false);
}
