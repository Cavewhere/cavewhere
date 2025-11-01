//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our includes
#include "cwRootData.h"

TEST_CASE("cwNoteLiDARManager should warp and fit lidar data", "[cwNoteLiDARManager]")
{
    // Set up the root data and project
    auto root = std::make_unique<cwRootData>();


    //Remove local directory, use zip version of jaws of the beast
    CHECK(false);
    root->project()->loadFile("/Users/cave/Desktop/lidarTest/jaws of the beast/jaws of the beast.cw");
    root->project()->waitLoadToFinish();


    root->futureManagerModel()->waitForFinished();

}
