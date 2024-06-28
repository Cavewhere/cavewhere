//Catch includes
#include <catch2/catch_test_macros.hpp>

//Our includes
#include "cwRootData.h"
#include "cwJobSettings.h"
#include "cwScrapManager.h"
#include "cwLinePlotManager.h"
#include "cwGLLinePlot.h"

TEST_CASE("cwRootData should update the managers with auto update correctly", "[cwRootData]") {

    cwJobSettings::initialize();
    cwJobSettings::instance()->setAutomaticUpdate(false);

    cwRootData rootData;
    CHECK(rootData.linePlotManager()->automaticUpdate() == false);
    CHECK(rootData.scrapManager()->automaticUpdate() == false);

    cwJobSettings::instance()->setAutomaticUpdate(true);

    CHECK(rootData.linePlotManager()->automaticUpdate() == true);
    CHECK(rootData.scrapManager()->automaticUpdate() == true);
}
