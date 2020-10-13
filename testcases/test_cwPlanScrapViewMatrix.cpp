//Catch includes
#include "catch.hpp"

//Our includes
#include "cwPlanScrapViewMatrix.h"

TEST_CASE("Plan scrap view matrix should produce the correct ViewMatrix", "[cwPlanScrapViewMatrix]") {

    cwPlanScrapViewMatrix matrix;
    CHECK(matrix.type() == cwScrap::Plan);
    CHECK(matrix.data()->type() == cwScrap::Plan);
    CHECK(matrix.matrix() == QMatrix4x4());

    CHECK(dynamic_cast<const cwPlanScrapViewMatrix::Data*>(matrix.data()) != nullptr);

    auto matrixData = std::unique_ptr<cwPlanScrapViewMatrix::Data>(matrix.data()->clone());
    CHECK(matrixData->matrix() == QMatrix4x4());
}
