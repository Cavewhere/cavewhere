//catch inculdes
#include <catch.hpp>

//Our includes
#include "cwRegionIOTask.h"

TEST_CASE("Proto version should report a CaveWhere version correctly", "[cwRegionIOTask]") {
    bool okay;
    //If this fails you need to update cwRegionIOTask to have the current version in there
    double versionNumber = cwRegionIOTask::toVersion(cwRegionIOTask::protoVersion()).toDouble(&okay);
    CHECK(versionNumber > 0);
    CHECK(okay == true);
}
