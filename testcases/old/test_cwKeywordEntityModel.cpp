//Catch include
#include <catch.hpp>

//Cavewhere includes
#include "cwKeywordEntityModel.h"

//Qt includes
#include <QEntity>

using namespace Qt3DCore;

TEST_CASE("cwKeywordEntityModel should initialize", "[cwKeywordEntityModel]") {
    cwKeywordEntityModel model;
}

TEST_CASE("cwKeywordEntityModel should add / remove entity correctly", "[cwKeywordEntityModel]") {
    cwKeywordEntityModel model;

    QEntity* entity1 = new QEntity();


}

