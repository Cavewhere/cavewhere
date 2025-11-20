//Catch includes
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

//Our inculdes
#include "InfiniteGridModel.h"

using namespace cwSketch;

TEST_CASE("Default property values", "[InfiniteGridModel]") {

    InfiniteGridModel model;
    model.setViewport(QRectF(1,2,3,4));
    model.setLineWidth(5.2);

}
